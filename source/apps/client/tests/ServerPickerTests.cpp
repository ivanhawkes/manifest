#include "TestHelpers.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <jettison/core/Log.h>
#include <jettison/host/Session.h>
#include <jettison/host/WorldContent.h>
#include <jettison/net/Key.h>
#include <jettison/net/Token.h>

#include <manifest/client/ServerPicker.h>
#include <manifest/host/Start.h>
#include <manifest/remote/Keys.h>
#include <manifest/remote/Server.h>
#include <manifest/remote/Tokens.h>

using Manifest::Client::PickRefusal;
using Manifest::Client::Refused;
using Manifest::Client::SavedServer;
using Manifest::Client::ServerPicker;
using std::filesystem::perms;

namespace
{
// A directory of the test's own, the player's configuration under it.
class TempDirectory
{
  public:
	TempDirectory()
	{
		std::filesystem::remove_all(m_directory);
	}


	~TempDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_directory, ignored);
	}


	TempDirectory(const TempDirectory &) = delete;


	TempDirectory &operator=(const TempDirectory &) = delete;


	std::filesystem::path Path() const
	{
		return m_directory;
	}

  private:
	Manifest::Test::TempFile m_marker{""};
	std::filesystem::path m_directory{m_marker.Path() + "-home"};
};


// Every message logged while it lives.
class LogCapture
{
  public:
	LogCapture()
	    : m_id{Jettison::Core::AddLogCallback(
	          [this](const Jettison::Core::LogRecord &record)
	          {
		          const std::scoped_lock lock{m_mutex};
		          m_text += std::string{record.message} + "\n";
	          })}
	{
	}


	~LogCapture()
	{
		Jettison::Core::RemoveLogCallback(m_id);
	}


	LogCapture(const LogCapture &) = delete;


	LogCapture &operator=(const LogCapture &) = delete;


	std::string Text()
	{
		const std::scoped_lock lock{m_mutex};
		return m_text;
	}

  private:
	std::mutex m_mutex;
	std::string m_text;
	Jettison::Core::LogCallbackId m_id;
};


// What a call refused, or none when it did not throw Refused.
template <typename Call> std::optional<PickRefusal> RefusalOf(Call call)
{
	try
	{
		call();
	}
	catch (const Refused &refused)
	{
		return refused.Reason();
	}
	return std::nullopt;
}
} // namespace


TEST_CASE("The server picker saves a server with its token privately, and removes it")
{
	const TempDirectory home;
	const std::filesystem::path own = home.Path() / "config" / "manifest" / "servers.yaml";
	const std::string key = Jettison::Net::GenerateServerKey().publicKey;
	const Manifest::Test::TempFile shipped{
	    "version: 1\nservers:\n  - endpoint: 127.0.0.1:47720\n    key: " + key + "\n"};
	const std::string token = Jettison::Net::GenerateToken();
	LogCapture log;

	// The build's pins hold no token, so nothing is saved yet.
	ServerPicker picker{own, shipped.Path()};
	CHECK(picker.Servers().empty());

	// Each refusal is made before anything is written.
	CHECK(RefusalOf([&] { picker.Add(" ", "127.0.0.1:47720", "", token); }) == PickRefusal::NoName);
	CHECK(RefusalOf([&] { picker.Add("Home", "127.0.0.1", "", token); }) == PickRefusal::BadEndpoint);
	CHECK(RefusalOf([&] { picker.Add("Home", "127.0.0.1:0", "", token); }) == PickRefusal::BadEndpoint);
	CHECK(RefusalOf([&] { picker.Add("Home", ":47720", "", token); }) == PickRefusal::BadEndpoint);
	CHECK(RefusalOf([&] { picker.Add("Home", "127.0.0.1:47720", "", "\n"); }) == PickRefusal::NoToken);
	CHECK(
	    RefusalOf([&] { picker.Add("Home", "127.0.0.1:47721", "", token); }) == PickRefusal::Unpinned);
	CHECK_FALSE(std::filesystem::exists(own));

	// A server the build pins needs no key; the player's list is made with the build's pins in it, the
	// file its owner's alone and its directories private.
	picker.Add("Home", " 127.0.0.1:47720 ", "", token + "\n");
	CHECK(picker.Servers() == std::vector<SavedServer>{{"Home", "127.0.0.1:47720"}});
	CHECK(std::filesystem::status(own).permissions() == (perms::owner_read | perms::owner_write));
	CHECK(std::filesystem::status(own.parent_path()).permissions() == perms::owner_all);
	CHECK(std::filesystem::status(own.parent_path().parent_path()).permissions() == perms::owner_all);
	Manifest::Remote::ServerList list = Manifest::Remote::ReadServers(own);
	REQUIRE(list.servers.size() == 1);
	CHECK(list.servers[0].key == key);
	CHECK(list.servers[0].token == token);

	// Another, with its key, and the first renamed by saving it again.
	const std::string other = Jettison::Net::GenerateServerKey().publicKey;
	const std::string otherToken = Jettison::Net::GenerateToken();
	picker.Add("Elsewhere", "example.org:47720", other, otherToken);
	picker.Add("At home", "127.0.0.1:47720", "", token);
	CHECK(
	    picker.Servers() ==
	    std::vector<SavedServer>{{"At home", "127.0.0.1:47720"}, {"Elsewhere", "example.org:47720"}});
	CHECK(
	    Manifest::Remote::PinnedKey(Manifest::Remote::ReadServers(own), "example.org:47720") == other);

	// Removed, its token goes and its pin stays; the list reads so again.
	picker.Remove("127.0.0.1:47720");
	CHECK(picker.Servers() == std::vector<SavedServer>{{"Elsewhere", "example.org:47720"}});
	list = Manifest::Remote::ReadServers(own);
	CHECK(Manifest::Remote::PinnedKey(list, "127.0.0.1:47720") == key);
	CHECK(list.servers[0].token.empty());
	CHECK(ServerPicker{own, shipped.Path()}.Servers() == picker.Servers());
	CHECK(std::filesystem::status(own).permissions() == (perms::owner_read | perms::owner_write));

	// A list that cannot be read lists nothing and saves nothing; one that cannot be written says so.
	const Manifest::Test::TempFile broken{"servers: ["};
	ServerPicker unread{broken.Path(), shipped.Path()};
	CHECK(unread.Servers().empty());
	CHECK(
	    RefusalOf([&] { unread.Add("Home", "127.0.0.1:47720", key, token); }) ==
	    PickRefusal::Unreadable);
	ServerPicker unwritten{
	    std::filesystem::path{shipped.Path()} / "manifest" / "servers.yaml", shipped.Path()};
	CHECK(
	    RefusalOf([&] { unwritten.Add("Home", "127.0.0.1:47720", "", token); }) ==
	    PickRefusal::Unwritable);

	// No token is in the log.
	const std::string logged = log.Text();
	CHECK(logged.find(token) == std::string::npos);
	CHECK(logged.find(otherToken) == std::string::npos);
}


TEST_CASE("The server picker connects to a saved server as its token's player, and says why it cannot")
{
	const std::string token = Jettison::Net::GenerateToken();
	const Manifest::Test::TempFile tokens{"", ".tokens.yaml"};
	Manifest::Remote::WriteTokens(
	    tokens.Path(),
	    {{{Jettison::Net::DigestToken(token), Manifest::Command::ActorKind::Principal, "Ada"}}});
	const std::unique_ptr<Jettison::Host::Session> session =
	    Manifest::Host::Start({MANIFEST_MOD_DIRECTORY, {}, "trading"});
	const Jettison::Net::ServerKey key = Jettison::Net::GenerateServerKey();
	const Manifest::Test::TempFile keyFile{"", ".key"};
	Manifest::Remote::WriteServerKey(keyFile.Path(), key.secretKey);
	Manifest::Remote::Server server{*session, tokens.Path(), keyFile.Path(), "127.0.0.1", 0};
	const std::string endpoint = "127.0.0.1:" + std::to_string(server.Port());

	const TempDirectory home;
	const std::filesystem::path own = home.Path() / "manifest" / "servers.yaml";
	ServerPicker picker{own, home.Path() / "none.yaml"};
	LogCapture log;

	// Both ends polled until the connection is welcomed or closed.
	const auto settle = [&](Manifest::Remote::Client &remote)
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
		while (!remote.Welcomed() && !remote.Closed() && std::chrono::steady_clock::now() < deadline)
		{
			server.Poll(std::chrono::milliseconds{1});
			(void)remote.Poll(std::chrono::milliseconds{1});
		}
	};

	// Saved with its key, the server is connected to as the token's player, the key pinned.
	picker.Add("Local", endpoint, key.publicKey, token);
	std::unique_ptr<Manifest::Remote::Client> remote = picker.Connect(endpoint);
	remote->Hold(Jettison::Host::DescribeWorldContent(session->Files()));
	settle(*remote);
	REQUIRE(remote->Welcomed());
	CHECK(remote->Welcomed()->actorId == "Ada");
	remote->Disconnect();
	(void)remote->Poll(std::chrono::milliseconds{10});

	// A token the server does not take is refused by it.
	const std::string wrong = Jettison::Net::GenerateToken();
	picker.Add("Local", endpoint, "", wrong);
	remote = picker.Connect(endpoint);
	remote->Hold(Jettison::Host::DescribeWorldContent(session->Files()));
	settle(*remote);
	CHECK(remote->Closed() == Jettison::Net::Reason::BadCredential);

	// What the picker refuses before anything is sent.
	CHECK(RefusalOf([&] { (void)picker.Connect("127.0.0.1:1"); }) == PickRefusal::NoToken);
	picker.Add("Nowhere", "nowhere.invalid:47720", key.publicKey, token);
	CHECK(
	    RefusalOf([&] { (void)picker.Connect("nowhere.invalid:47720"); }) == PickRefusal::UnknownHost);
	picker.Add("Garbled", "127.0.0.1:2", "not a key", token);
	CHECK(RefusalOf([&] { (void)picker.Connect("127.0.0.1:2"); }) == PickRefusal::BadKey);
	CHECK(
	    RefusalOf([&] { (void)Manifest::Client::Dial(own, "127.0.0.1:3", token); }) ==
	    PickRefusal::Unpinned);
	CHECK(
	    RefusalOf([&] { (void)Manifest::Client::Dial(own, "nowhere", token); }) ==
	    PickRefusal::BadEndpoint);

	const std::string logged = log.Text();
	CHECK(logged.find(token) == std::string::npos);
	CHECK(logged.find(wrong) == std::string::npos);
}
