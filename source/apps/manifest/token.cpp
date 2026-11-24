// manifest-token [--tokens <file>] issue principal|administrator <id>: issues the actor a new bearer
// token, printed once on stdout, and keeps only its digest in the file, replacing any the actor had.
// manifest-token [--tokens <file>] revoke principal|administrator <id>: removes the actor's token. A
// running server reads the file again and closes the actor's sessions. The file defaults to
// Manifest::Host::DefaultTokens, its directory made private when it is first written to. An actor
// with no token to revoke, or a file that cannot be found, read or written, exits 1; a bad command
// line 2.
#include <manifest/command/Request.h>
#include <manifest/host/Locations.h>
#include <manifest/remote/Tokens.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/net/Token.h>

namespace
{
using Manifest::Command::ActorKind;


int Usage()
{
	std::fputs(
	    "usage: manifest-token [--tokens <file>] issue|revoke principal|administrator <id>\n", stderr);
	return 2;
}
} // namespace


int main(int argc, char **argv)
{
	const std::vector<std::string_view> all(argv + 1, argv + argc);
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
		std::filesystem::path path;
		if (named)
			path = all[1];
		else
		{
			path = Manifest::Host::DefaultTokens();
			Manifest::Host::MakePrivateDirectories(path.parent_path());
		}
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
