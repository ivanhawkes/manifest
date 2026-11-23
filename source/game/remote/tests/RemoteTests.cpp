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

#include <manifest/command/Request.h>
#include <manifest/company/Company.h>
#include <manifest/host/Start.h>
#include <manifest/remote/Client.h>
#include <manifest/remote/Server.h>
#include <manifest/remote/Tokens.h>
#include <manifest/world/State.h>

using namespace Manifest::Remote;
using Jettison::Net::Reason;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
namespace Company = Manifest::Company;

namespace
{
std::vector<std::byte> Provision(const std::string &principal);


std::vector<std::byte> Load(std::uint32_t quantity)
{
	return Jettison::Serialise::BinaryArchive::Write(
	    Company::Move{"Ship1", "VenueForgeHighport", "ItemCopper", quantity},
	    Jettison::Serialise::FormatVersion{Company::kVersion});
}


// A client that is not Manifest's, presenting the token: it sends whatever the test makes it.
Jettison::Net::Client RawClient(std::uint16_t port, const std::string &token)
{
	Jettison::Net::ClientConfig config;
	config.port = port;
	config.protocol = {std::string{kProtocol}, kVersion};
	for (const char character : token)
		config.credential.push_back(static_cast<std::byte>(character));
	return Jettison::Net::Client{config};
}


// A hosted world, with a home region for each of two players, served on an ephemeral loopback port, its
// operator having issued a token to Ada, to Bea and to an administrator.
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
		      {Jettison::Net::DigestToken(bea), ActorKind::Principal, "Bea"},
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

	// The client's snapshot of the world as it now is, once it arrives.
	const Manifest::Query::Snapshot *Current(Client &client)
	{
		const std::uint64_t step = pSession->World().Clock().Step();
		const bool arrived =
		    Pump(client, [&] { return client.Latest().has_value() && client.Latest()->step == step; });
		return arrived ? &*client.Latest() : nullptr;
	}

	// The administrator provisions Ada's and Bea's companies.
	void ProvisionPlayers()
	{
		Client operatorClient = Connect(admin);
		REQUIRE(Welcome(operatorClient));
		const std::uint64_t forAda =
		    operatorClient.Submit(Company::kProvision, Company::kVersion, Provision("Ada")).value();
		const std::uint64_t forBea =
		    operatorClient.Submit(Company::kProvision, Company::kVersion, Provision("Bea")).value();
		REQUIRE(Await(operatorClient, forBea, Receipt::Status::Queued));
		Step();
		REQUIRE(Await(operatorClient, forAda, Receipt::Status::Completed));
		REQUIRE(Await(operatorClient, forBea, Receipt::Status::Completed));
		replies.clear();
	}

	std::string ada = Jettison::Net::GenerateToken();
	std::string bea = Jettison::Net::GenerateToken();
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


TEST_CASE("no client connects as an NPC's principal, whatever token the file holds for it")
{
	Served served;
	served.ProvisionPlayers();
	const Manifest::Host::Services services = Manifest::Host::HostedServices(*served.pSession);
	served.pSession->World()
	    .Registry()
	    .get<Manifest::World::Principal>(
	        services.identities.Find(Manifest::World::Kind::Principal, "Ada"))
	    .npc = true;
	Client npc = served.Connect(served.ada);
	CHECK_FALSE(served.Welcome(npc));
	CHECK(npc.Closed() == Reason::BadCredential);
	Client player = served.Connect(served.bea);
	CHECK(served.Welcome(player));
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
	Jettison::Net::Client raw = RawClient(served.pServer->Port(), served.admin);
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


TEST_CASE("several players are served at once, and one actor holds at most three sessions")
{
	Served served;
	served.ProvisionPlayers();

	Client first = served.Connect(served.ada);
	Client second = served.Connect(served.ada);
	Client third = served.Connect(served.ada);
	REQUIRE(served.Welcome(first));
	REQUIRE(served.Welcome(second));
	REQUIRE(served.Welcome(third));
	Client fourth = served.Connect(served.ada);
	CHECK_FALSE(served.Welcome(fourth));
	CHECK(fourth.Closed() == Reason::ServerFull);
	// The cap is each actor's, not the server's.
	Client bea = served.Connect(served.bea);
	REQUIRE(served.Welcome(bea));

	served.Step();
	for (Client *pAda : {&first, &second, &third})
	{
		const Manifest::Query::Snapshot *pSeen = served.Current(*pAda);
		REQUIRE(pSeen != nullptr);
		REQUIRE(pSeen->companies.size() == 1);
		CHECK(pSeen->companies[0].displayName == "Ada Freight");
	}
	const Manifest::Query::Snapshot *pBea = served.Current(bea);
	REQUIRE(pBea != nullptr);
	REQUIRE(pBea->companies.size() == 1);
	CHECK(pBea->companies[0].displayName == "Bea Freight");

	// Ada's sessions share her sequences: the second to send one with other content is refused, and
	// sends its next request under the sequence the refusal names.
	const std::uint64_t sequence = first.Submit(Company::kLoad, Company::kVersion, Load(1)).value();
	REQUIRE(served.Await(first, sequence, Receipt::Status::Queued));
	CHECK(second.Submit(Company::kLoad, Company::kVersion, Load(2)) == sequence);
	CHECK(
	    served.Await(second, sequence, Receipt::Status::Refused).value().result ==
	    Result::RequestConflict);
	CHECK(second.Submit(Company::kLoad, Company::kVersion, Load(2)) == sequence + 1);
	CHECK(served.Await(second, sequence + 1, Receipt::Status::Queued).has_value());

	// A session leaving frees its place, once the server has heard it go.
	third.Disconnect();
	std::unique_ptr<Client> pAgain;
	for (int attempt = 0; attempt < 20 && !(pAgain && pAgain->Welcomed()); ++attempt)
	{
		(void)third.Poll(std::chrono::milliseconds{10});
		pAgain = std::make_unique<Client>("127.0.0.1", served.pServer->Port(), served.ada);
		(void)served.Welcome(*pAgain);
	}
	REQUIRE(pAgain->Welcomed().has_value());
}


TEST_CASE("the world steps with no session, and a reconnect retries the request it never heard of")
{
	Served served;
	Client admin = served.Connect(served.admin);
	REQUIRE(served.Welcome(admin));
	const std::uint64_t sequence =
	    admin.Submit(Company::kProvision, Company::kVersion, Provision("Ada")).value();
	REQUIRE(served.Await(admin, sequence, Receipt::Status::Queued));
	admin.Disconnect();
	(void)admin.Poll(std::chrono::milliseconds{10});

	// No one is connected: the world steps, and its requests run, all the same.
	const std::uint64_t left = served.pSession->World().Clock().Step();
	for (int step = 0; step < 5; ++step)
	{
		served.pServer->Poll(std::chrono::milliseconds{1});
		served.Step();
	}
	CHECK(served.pSession->World().Clock().Step() == left + 5);

	Client again = served.Connect(served.admin);
	REQUIRE(served.Welcome(again));
	CHECK(again.Welcomed()->nextSequence == sequence + 1);
	REQUIRE(again.Retry(sequence, Company::kProvision, Company::kVersion, Provision("Ada")));
	CHECK(
	    served.Await(again, sequence, Receipt::Status::Completed).value().result == Result::Succeeded);
	served.Step();
	const Manifest::Query::Snapshot *pSeen = served.Current(again);
	REQUIRE(pSeen != nullptr);
	CHECK(pSeen->step == left + 6);
	// Provisioned once, though asked twice.
	CHECK(pSeen->companies.size() == 1);
	// The regions as the world has them, the governors' figures the administrator's (D-110).
	const Manifest::Host::Services services = Manifest::Host::HostedServices(*served.pSession);
	CHECK_FALSE(pSeen->regions.empty());
	CHECK(
	    pSeen->regions ==
	    Manifest::Query::Observe(
	        served.pSession->World(), services.identities, {ActorKind::Administrator, "Host"})
	        .regions);
}


TEST_CASE("a full queue answers QueueFull, and a flood past the inbound bound closes its session")
{
	Served served;
	Client admin = served.Connect(served.admin);
	REQUIRE(served.Welcome(admin));
	std::uint64_t last = 0;
	for (std::size_t request = 0; request <= Manifest::Command::kPendingPerActor; ++request)
		last = admin.Submit(Company::kProvision, Company::kVersion, Provision("Ada")).value();
	CHECK(served.Await(admin, last, Receipt::Status::Refused).value().result == Result::QueueFull);
	CHECK(served.Await(admin, last - 1, Receipt::Status::Queued).has_value());

	// Far more than a session may have waiting, sent before the server takes in any.
	Jettison::Net::Client flood = RawClient(served.pServer->Port(), served.ada);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
	while (!flood.Admitted() && std::chrono::steady_clock::now() < deadline)
	{
		served.pServer->Poll(std::chrono::milliseconds{1});
		(void)flood.Poll();
	}
	REQUIRE(flood.Admitted());
	for (int message = 0; message < 1000; ++message)
		(void)flood.Send(Jettison::Net::Channel::Reliable, 2, std::vector<std::byte>(3));
	(void)flood.Poll(std::chrono::milliseconds{50});
	std::optional<Reason> closed;
	while (!closed && std::chrono::steady_clock::now() < deadline + std::chrono::seconds{2})
	{
		served.pServer->Poll(std::chrono::milliseconds{1});
		for (const Jettison::Net::ClientEvent &event : flood.Poll())
			if (event.type == Jettison::Net::ClientEvent::Type::Closed)
				closed = event.reason;
	}
	CHECK(closed == Reason::InboundOverflow);
}
