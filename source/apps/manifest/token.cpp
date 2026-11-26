// manifest-token [--tokens <file>] issue principal|administrator <id>: issues the actor a new bearer
// token, printed once on stdout, and keeps only its digest in the file, replacing any the actor had.
// manifest-token [--tokens <file>] revoke principal|administrator <id>: removes the actor's token. A
// running server reads the file again and closes the actor's sessions.
// manifest-token [--key <file>] key [--replace]: makes the server's key pair, keeps the secret in the
// file, its owner's alone, and prints the public half once on stdout, for every client to pin. It
// refuses to replace a key there unless told to, since every client would need the new one.
// The files default to Manifest::Host::DefaultTokens and DefaultServerKey, their directory made private
// when it is first written to. An actor with no token to revoke, a key already there, or a file that
// cannot be found, read or written, exits 1; a bad command line 2.
#include <manifest/command/Request.h>
#include <manifest/host/Locations.h>
#include <manifest/remote/Keys.h>
#include <manifest/remote/Tokens.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/net/Key.h>
#include <jettison/net/Token.h>

namespace
{
using Manifest::Command::ActorKind;


int Usage()
{
	std::fputs(
	    "usage: manifest-token [--tokens <file>] issue|revoke principal|administrator <id>\n"
	    "       manifest-token [--key <file>] key [--replace]\n",
	    stderr);
	return 2;
}


// The path named, or the default, whose directory is made private to its owner.
std::filesystem::path Place(
    const std::optional<std::string_view> &named, std::filesystem::path (*fallback)())
{
	if (named)
		return *named;
	std::filesystem::path path = fallback();
	Manifest::Host::MakePrivateDirectories(path.parent_path());
	return path;
}


int MakeKey(const std::optional<std::string_view> &named, bool replace)
{
	const std::filesystem::path path = Place(named, Manifest::Host::DefaultServerKey);
	if (!replace && std::filesystem::exists(path))
	{
		std::fputs(
		    std::format(
		        "{} already holds a key: --replace makes a new one, which every client must then pin\n",
		        path.string())
		        .c_str(),
		    stderr);
		return 1;
	}
	const Jettison::Net::ServerKey key = Jettison::Net::GenerateServerKey();
	Manifest::Remote::WriteServerKey(path, key.secretKey);
	std::puts(key.publicKey.c_str());
	return 0;
}
} // namespace


int main(int argc, char **argv)
{
	const std::vector<std::string_view> all(argv + 1, argv + argc);
	const bool keyNamed = all.size() >= 2 && all[0] == "--key";
	// key, then --replace or nothing.
	const std::span<const std::string_view> keyArguments = std::span{all}.subspan(keyNamed ? 2 : 0);
	if (!keyArguments.empty() && keyArguments[0] == "key")
	{
		if (keyArguments.size() > 2 || (keyArguments.size() == 2 && keyArguments[1] != "--replace"))
			return Usage();
		try
		{
			return MakeKey(keyNamed ? std::optional{all[1]} : std::nullopt, keyArguments.size() == 2);
		}
		catch (const std::exception &error)
		{
			std::fputs(std::format("{}\n", error.what()).c_str(), stderr);
			return 1;
		}
	}

	const bool named = all.size() == 5 && all[0] == "--tokens";
	// The action, its actor's kind and its id.
	const std::span<const std::string_view> arguments = std::span{all}.subspan(named ? 2 : 0);
	if (arguments.size() != 3 || (arguments[0] != "issue" && arguments[0] != "revoke") ||
	    (arguments[1] != "principal" && arguments[1] != "administrator") || arguments[2].empty() ||
	    arguments[2].size() > Manifest::Command::kMaxActorIdBytes)
		return Usage();
	const ActorKind kind =
	    arguments[1] == "principal" ? ActorKind::Principal : ActorKind::Administrator;
	const std::string id{arguments[2]};

	try
	{
		const std::filesystem::path path =
		    Place(named ? std::optional{all[1]} : std::nullopt, Manifest::Host::DefaultTokens);
		Manifest::Remote::TokenFile file = std::filesystem::exists(path)
		                                       ? Manifest::Remote::ReadTokens(path)
		                                       : Manifest::Remote::TokenFile{};
		const std::size_t removed = std::erase_if(
		    file.tokens,
		    [&](const auto &credential) { return credential.kind == kind && credential.id == id; });
		if (arguments[0] == "revoke" && removed == 0)
		{
			std::fputs(std::format("{} {} has no token\n", arguments[1], id).c_str(), stderr);
			return 1;
		}
		std::string token;
		if (arguments[0] == "issue")
		{
			token = Jettison::Net::GenerateToken();
			file.tokens.push_back({Jettison::Net::DigestToken(token), kind, id});
		}
		Manifest::Remote::WriteTokens(path, file);
		if (!token.empty())
			std::puts(token.c_str());
	}
	catch (const std::exception &error)
	{
		std::fputs(std::format("{}\n", error.what()).c_str(), stderr);
		return 1;
	}
	return 0;
}
