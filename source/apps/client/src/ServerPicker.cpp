#include <manifest/client/ServerPicker.h>

#include <algorithm>
#include <charconv>
#include <exception>
#include <format>
#include <iterator>
#include <optional>
#include <system_error>
#include <utility>

#include <jettison/core/Log.h>

#include <manifest/host/Locations.h>

namespace Manifest::Client
{
namespace
{
constexpr Jettison::Core::Category kClient{"client"};

// The ports a host:port may name.
constexpr std::uint16_t kLeastPort = 1;

constexpr std::string_view kSpace = " \t\r\n";

struct Endpoint
{
	std::string host;
	std::uint16_t port{0};
};


std::optional<Endpoint> Split(const std::string &endpoint)
{
	const std::size_t colon = endpoint.rfind(':');
	if (colon == std::string::npos || colon == 0)
		return std::nullopt;
	const char *pEnd = endpoint.data() + endpoint.size();
	std::uint16_t port = 0;
	const auto parsed = std::from_chars(endpoint.data() + colon + 1, pEnd, port);
	if (parsed.ec != std::errc{} || parsed.ptr != pEnd || port < kLeastPort)
		return std::nullopt;
	return Endpoint{endpoint.substr(0, colon), port};
}


std::string Trim(std::string text)
{
	text.erase(text.find_last_not_of(kSpace) + 1);
	text.erase(0, text.find_first_not_of(kSpace));
	return text;
}
} // namespace


std::unique_ptr<Remote::Client> Dial(
    const std::filesystem::path &servers, const std::string &endpoint, std::string_view token)
{
	const std::optional<Endpoint> split = Split(endpoint);
	if (!split)
		throw Refused{PickRefusal::BadEndpoint, std::format("expected host:port, got '{}'", endpoint)};
	std::optional<std::string> key;
	try
	{
		key = Remote::PinnedKey(Remote::ReadServers(servers), endpoint);
	}
	catch (const std::runtime_error &error)
	{
		throw Refused{PickRefusal::Unreadable, error.what()};
	}
	if (!key)
		throw Refused{
		    PickRefusal::Unpinned,
		    std::format("the server list {} pins no key for {}", servers.string(), endpoint)};
	try
	{
		return std::make_unique<Remote::Client>(split->host, split->port, std::move(*key), token);
	}
	catch (const std::invalid_argument &)
	{
		throw Refused{
		    PickRefusal::BadKey,
		    std::format(
		        "the server list {} pins a key for {} that is not one", servers.string(), endpoint)};
	}
	catch (const std::runtime_error &error)
	{
		throw Refused{PickRefusal::UnknownHost, error.what()};
	}
}


ServerPicker::ServerPicker(std::filesystem::path own, std::filesystem::path shipped)
    : m_own{std::move(own)}, m_shipped{std::move(shipped)}
{
	try
	{
		List(Read());
	}
	catch (const Refused &error)
	{
		Jettison::Core::Log(Jettison::Core::Severity::Warning, kClient, "{}", error.what());
	}
}


void ServerPicker::Add(std::string name, std::string endpoint, std::string key, std::string token)
{
	name = Trim(std::move(name));
	endpoint = Trim(std::move(endpoint));
	key = Trim(std::move(key));
	token = Trim(std::move(token));
	if (name.empty())
		throw Refused{PickRefusal::NoName, "a server needs a name"};
	if (!Split(endpoint))
		throw Refused{PickRefusal::BadEndpoint, std::format("expected host:port, got '{}'", endpoint)};
	if (token.empty())
		throw Refused{PickRefusal::NoToken, std::format("no token was given for {}", endpoint)};
	Remote::ServerList list = Read();
	auto found = std::ranges::find(list.servers, endpoint, &Remote::PinnedServer::endpoint);
	if (found == list.servers.end())
	{
		if (key.empty())
			throw Refused{
			    PickRefusal::Unpinned,
			    std::format("the server list {} pins no key for {}", Listed().string(), endpoint)};
		list.servers.push_back({endpoint, {}, {}, {}});
		found = std::prev(list.servers.end());
	}
	if (!key.empty())
		found->key = std::move(key);
	found->name = std::move(name);
	found->token = std::move(token);
	Write(list);
}


void ServerPicker::Remove(std::string_view endpoint)
{
	Remote::ServerList list = Read();
	const auto found = std::ranges::find(list.servers, endpoint, &Remote::PinnedServer::endpoint);
	if (found == list.servers.end())
		return;
	found->name.clear();
	found->token.clear();
	Write(list);
}


std::unique_ptr<Remote::Client> ServerPicker::Connect(const std::string &endpoint) const
{
	const Remote::ServerList list = Read();
	const auto found = std::ranges::find(list.servers, endpoint, &Remote::PinnedServer::endpoint);
	if (found == list.servers.end() || found->token.empty())
		throw Refused{PickRefusal::NoToken, std::format("no token is saved for {}", endpoint)};
	return Dial(Listed(), endpoint, found->token);
}


const std::filesystem::path &ServerPicker::Listed() const
{
	return std::filesystem::exists(m_own) ? m_own : m_shipped;
}


Remote::ServerList ServerPicker::Read() const
{
	const std::filesystem::path &path = Listed();
	if (!std::filesystem::exists(path))
		return {};
	try
	{
		return Remote::ReadServers(path);
	}
	catch (const std::runtime_error &error)
	{
		throw Refused{PickRefusal::Unreadable, error.what()};
	}
}


void ServerPicker::Write(const Remote::ServerList &list)
{
	try
	{
		Host::MakePrivateDirectories(m_own.parent_path());
		Remote::WriteServers(m_own, list);
	}
	catch (const std::exception &error)
	{
		throw Refused{PickRefusal::Unwritable, error.what()};
	}
	List(list);
}


void ServerPicker::List(const Remote::ServerList &list)
{
	m_servers.clear();
	for (const Remote::PinnedServer &server : list.servers)
		if (!server.token.empty())
			m_servers.push_back({server.name, server.endpoint});
}
} // namespace Manifest::Client
