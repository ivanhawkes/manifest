#include <manifest/remote/Keys.h>

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <jettison/core/File.h>
#include <jettison/net/Key.h>
#include <jettison/serialise/Archive.h>

#include <manifest/remote/Protocol.h>

namespace Manifest::Remote
{
namespace
{
constexpr Jettison::Serialise::FormatVersion kServerListVersion{1};
constexpr std::filesystem::perms kOwnerOnly =
    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;


std::string ReadText(const std::filesystem::path &path, std::string_view what)
{
	std::ifstream in{path, std::ios::binary};
	if (!in)
		throw std::runtime_error{std::format("the {} {} cannot be read", what, path.string())};
	return {std::istreambuf_iterator<char>{in}, {}};
}
} // namespace


std::string ReadServerKey(const std::filesystem::path &path)
{
	std::string key = ReadText(path, "server key file");
	if (key.ends_with('\n'))
		key.pop_back();
	try
	{
		(void)Jettison::Net::PublicKeyOf(key);
	}
	catch (const std::invalid_argument &)
	{
		throw std::runtime_error{
		    std::format("the server key file {} holds no secret key", path.string())};
	}
	return key;
}


void WriteServerKey(const std::filesystem::path &path, std::string_view secretKey)
{
	Jettison::Core::WriteFileDurably(path, std::format("{}\n", secretKey), kOwnerOnly);
}


ServerList ReadServers(const std::filesystem::path &path)
{
	RegisterTypes();
	const std::string text = ReadText(path, "server list");
	try
	{
		return Jettison::Serialise::YamlArchive::Read<ServerList>(text, kServerListVersion);
	}
	catch (const std::exception &error)
	{
		throw std::runtime_error{
		    std::format("the server list {} is not one: {}", path.string(), error.what())};
	}
}


std::optional<std::string> PinnedKey(const ServerList &list, std::string_view endpoint)
{
	const auto found = std::ranges::find(list.servers, endpoint, &PinnedServer::endpoint);
	if (found == list.servers.end())
		return std::nullopt;
	return found->key;
}
} // namespace Manifest::Remote
