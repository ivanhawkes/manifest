#include <manifest/remote/Tokens.h>

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <jettison/net/Token.h>
#include <jettison/serialise/Archive.h>

#include <manifest/remote/Protocol.h>

namespace Manifest::Remote
{
namespace
{
constexpr Jettison::Serialise::FormatVersion kTokenFileVersion{1};
} // namespace


TokenFile ReadTokens(const std::filesystem::path &path)
{
	RegisterTypes();
	std::ifstream in{path, std::ios::binary};
	if (!in)
		throw std::runtime_error{std::format("the token file {} cannot be read", path.string())};
	const std::string text{std::istreambuf_iterator<char>{in}, {}};
	try
	{
		return Jettison::Serialise::YamlArchive::Read<TokenFile>(text, kTokenFileVersion);
	}
	catch (const std::exception &error)
	{
		throw std::runtime_error{
		    std::format("the token file {} is not one: {}", path.string(), error.what())};
	}
}


void WriteTokens(const std::filesystem::path &path, const TokenFile &file)
{
	RegisterTypes();
	std::filesystem::path partial = path;
	partial += ".partial";
	{
		std::ofstream out{partial, std::ios::binary | std::ios::trunc};
		out << Jettison::Serialise::YamlArchive::Write(file, kTokenFileVersion);
		if (!out.flush())
			throw std::runtime_error{std::format("{} cannot be written", partial.string())};
	}
	std::filesystem::rename(partial, path);
}


std::optional<Credential> Resolve(const TokenFile &file, std::string_view token)
{
	for (const Credential &credential : file.tokens)
		if (Jettison::Net::TokenMatches(token, credential.digest))
			return credential;
	return std::nullopt;
}
} // namespace Manifest::Remote
