#include "TestHelpers.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <jettison/net/Client.h>
#include <jettison/net/Token.h>
#include <jettison/serialise/Archive.h>

#include <manifest/company/Company.h>
#include <manifest/host/Start.h>
#include <manifest/remote/Client.h>
#include <manifest/remote/Server.h>
#include <manifest/remote/Tokens.h>

using namespace Manifest::Remote;
using Jettison::Net::Reason;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
namespace Company = Manifest::Company;

namespace
{
// A hosted world, with a home region for each of two players, served on an ephemeral loopback port, its
// operator having issued a token to Ada and to an administrator.
struct Served
{
	Served()
	{
		Write();
		pServer = std::make_unique<Server>(*pSession, tokens.Path(), "127.0.0.1", 0);
	}

	void Write()
	{
		WriteTokens(
		    tokens.Path(),
		    {{{Jettison::Net::DigestToken(ada), ActorKind::Principal, "Ada"},
		      {Jettison::Net::DigestToken(admin), ActorKind::Administrator, "Operator"}}});
	}

	Client Connect(const std::string &token) const
	{
		return {"127.0.0.1", pServer->Port(), token};
	}

	// Both ends polled until done holds, for at most two seconds; the client's replies kept.
	bool Pump(Client &client, const std::function<bool()> &done)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
		while (!done())
		{
			if (std::chrono::steady_clock::now() > deadline)
				return false;
			pServer->Poll(std::chrono::milliseconds{1});
			for (Reply &reply : client.Poll())
				replies.push_back(reply);
		}
		return true;
	}

	bool Welcome(Client &client)
	{
		return Pump(client, [&] { return client.Welcomed().has_value() || client.Closed(); }) &&
		       client.Welcomed().has_value();
	}

	// The reply to the sequence with the status, once it arrives.
	std::optional<Receipt> Await(Client &client, std::uint64_t sequence, Receipt::Status status)
	{
		std::optional<Receipt> found;
		Pump(
		    client,
		    [&]
		    {
			    for (const Reply &reply : replies)
				    if (reply.sequence == sequence && reply.receipt.status == status)
					    found = reply.receipt;
			    return found.has_value();
		    });
		return found;
	}

	void Step()
	{
		pSession->World().Step();
		pServer->Publish();
	}

	std::string ada = Jettison::Net::GenerateToken();
	std::string admin = Jettison::Net::GenerateToken();
	Manifest::Test::TempFile tokens{"", ".tokens.yaml"};
	std::unique_ptr<Jettison::Host::Session> pSession =
	    Manifest::Host::Start({MANIFEST_MOD_DIRECTORY, {}, "governance"});
	std::unique_ptr<Server> pServer;
	std::vector<Reply> replies;
};


std::vector<std::byte> Provision(const std::string &principal)
{
	return Jettison::Serialise::BinaryArchive::Write(
	    Company::Provision{principal, principal, principal + " Freight"},
	    Jettison::Serialise::FormatVersion{Company::kVersion});
}
} // namespace


TEST_CASE("a token file keeps digests and resolves only the tokens it was issued")
{
	const std::string token = Jettison::Net::GenerateToken();
	const Manifest::Test::TempFile file{"", ".tokens.yaml"};
	WriteTokens(file.Path(), {{{Jettison::Net::DigestToken(token), ActorKind::Principal, "Ada"}}});
	const TokenFile read = ReadTokens(file.Path());
	REQUIRE(read.tokens.size() == 1);
	CHECK(read.tokens[0].digest != token);
	CHECK(Resolve(read, token).value().id == "Ada");
	CHECK_FALSE(Resolve(read, Jettison::Net::GenerateToken()).has_value());

	const Manifest::Test::TempFile broken{"tokens: [", ".tokens.yaml"};
	CHECK_THROWS_AS(ReadTokens(broken.Path()), std::runtime_error);
}


TEST_CASE("a remote player is welcomed as its token's actor and sees only its own company")
{
	Served served;

	Client impostor = served.Connect(Jettison::Net::GenerateToken());
	CHECK_FALSE(served.Welcome(impostor));
	CHECK(impostor.Closed() == Reason::BadCredential);

	Client admin = served.Connect(served.admin);
	REQUIRE(served.Welcome(admin));
	CHECK(admin.Welcomed()->actorKind == ActorKind::Administrator);
	CHECK(admin.Welcomed()->actorId == "Operator");
	const std::uint64_t ada =
	    admin.Submit(Company::kProvision, Company::kVersion, Provision("Ada")).value();
	const std::uint64_t bea =
	    admin.Submit(Company::kProvision, Company::kVersion, Provision("Bea")).value();
	CHECK(served.Await(admin, bea, Receipt::Status::Queued).has_value());
	served.Step();
	CHECK(served.Await(admin, ada, Receipt::Status::Completed).value().result == Result::Succeeded);
	CHECK(served.Await(admin, bea, Receipt::Status::Completed).value().result == Result::Succeeded);

	Client player = served.Connect(served.ada);
	REQUIRE(served.Welcome(player));
	CHECK(player.Welcomed()->actorId == "Ada");
	CHECK(player.Welcomed()->nextSequence == 1);
	// Provisioning is the administrator's: the player's request is refused at admission.
	const std::uint64_t refused =
	    player.Submit(Company::kProvision, Company::kVersion, Provision("Cai")).value();
	CHECK(
	    served.Await(player, refused, Receipt::Status::Refused).value().result ==
	    Result::NotAuthorised);
	// The refusal consumed no sequence, so the player's next request is sent under it again.
	const std::vector<std::byte> move = Jettison::Serialise::BinaryArchive::Write(
	    Company::Move{"Ship1", "VenueForgeHighport", "ItemCopper", 1},
	    Jettison::Serialise::FormatVersion{Company::kVersion});
	CHECK(player.Submit(Company::kLoad, Company::kVersion, move) == refused);
	CHECK(served.Await(player, refused, Receipt::Status::Queued).has_value());

	served.Step();
	REQUIRE(served.Pump(player, [&] { return player.Latest().has_value(); }));
	CHECK(player.Latest()->step == served.pSession->World().Clock().Step());
	REQUIRE(player.Latest()->companies.size() == 1);
	CHECK(player.Latest()->companies[0].displayName == "Ada Freight");
	REQUIRE(served.Pump(admin, [&] { return admin.Latest().has_value(); }));
	CHECK(admin.Latest()->companies.size() == 2);
}


TEST_CASE("a remote session is closed for a revoked token, a bad message and the server stopping")
{
	Served served;
	Client player = served.Connect(served.ada);
	REQUIRE(served.Welcome(player));
	WriteTokens(served.tokens.Path(), {});
	CHECK(served.Pump(player, [&] { return player.Closed().has_value(); }));
	CHECK(player.Closed() == Reason::Revoked);
	Client again = served.Connect(served.ada);
	CHECK_FALSE(served.Welcome(again));
	CHECK(again.Closed() == Reason::BadCredential);

	// A client that is not Manifest's sends something that is not a request.
	served.Write();
	Jettison::Net::ClientConfig config;
	config.port = served.pServer->Port();
	config.protocol = {std::string{kProtocol}, kVersion};
	for (const char character : served.admin)
		config.credential.push_back(static_cast<std::byte>(character));
	Jettison::Net::Client raw{config};
	std::optional<Reason> closed;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
	while (!closed && std::chrono::steady_clock::now() < deadline)
	{
		served.pServer->Poll(std::chrono::milliseconds{1});
		for (const Jettison::Net::ClientEvent &event : raw.Poll())
			if (event.type == Jettison::Net::ClientEvent::Type::Closed)
				closed = event.reason;
		if (raw.Admitted())
			(void)raw.Send(Jettison::Net::Channel::Reliable, 2, std::vector<std::byte>(3));
	}
	CHECK(closed == Reason::BadMessage);

	Client admin = served.Connect(served.admin);
	REQUIRE(served.Welcome(admin));
	served.pServer.reset();
	const auto stopping = std::chrono::steady_clock::now() + std::chrono::seconds{2};
	while (!admin.Closed() && std::chrono::steady_clock::now() < stopping)
		(void)admin.Poll(std::chrono::milliseconds{1});
	CHECK(admin.Closed() == Reason::Shutdown);
}
