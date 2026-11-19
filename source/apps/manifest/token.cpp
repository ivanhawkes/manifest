// manifest-token --tokens <file> issue principal|administrator <id>: issues the actor a new bearer
// token, printed once on stdout, and keeps only its digest in the file, replacing any the actor had.
// manifest-token --tokens <file> revoke principal|administrator <id>: removes the actor's token. A
// running server reads the file again and closes the actor's sessions. An actor with no token to
// revoke, or a file that cannot be read or written, exits 1; a bad command line 2.
#include <manifest/remote/Tokens.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
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
	    "usage: manifest-token --tokens <file> issue|revoke principal|administrator <id>\n", stderr);
	return 2;
}
} // namespace


int main(int argc, char **argv)
{
	const std::vector<std::string_view> arguments(argv + 1, argv + argc);
	if (arguments.size() != 5 || arguments[0] != "--tokens" ||
	    (arguments[2] != "issue" && arguments[2] != "revoke") ||
	    (arguments[3] != "principal" && arguments[3] != "administrator") || arguments[4].empty())
		return Usage();
	const std::filesystem::path path{arguments[1]};
	const ActorKind kind =
	    arguments[3] == "principal" ? ActorKind::Principal : ActorKind::Administrator;
	const std::string id{arguments[4]};

	try
	{
		Manifest::Remote::TokenFile file = std::filesystem::exists(path)
		                                       ? Manifest::Remote::ReadTokens(path)
		                                       : Manifest::Remote::TokenFile{};
		const std::size_t removed = std::erase_if(
		    file.tokens,
		    [&](const auto &credential) { return credential.kind == kind && credential.id == id; });
		if (arguments[2] == "revoke" && removed == 0)
		{
			std::fputs(std::format("{} {} has no token\n", arguments[3], id).c_str(), stderr);
			return 1;
		}
		std::string token;
		if (arguments[2] == "issue")
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
