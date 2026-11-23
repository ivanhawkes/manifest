#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <manifest/command/Request.h>

namespace Manifest::Remote
{
// A token the server's operator issued: its digest (Jettison::Net::DigestToken), never the token,
// and the actor it names. One per actor: issuing again replaces it.
struct Credential
{
	std::string digest;
	Command::ActorKind kind{Command::ActorKind::Principal};
	std::string id;
};

// The server's credentials, as the token CLI writes them: a YAML file of digests.
struct TokenFile
{
	std::vector<Credential> tokens;
};

// Throws std::runtime_error naming the file when it cannot be read or is not a token file.
TokenFile ReadTokens(const std::filesystem::path &path);

// Replaces the file whole and durably (Jettison::Core::WriteFileDurably), so a server reading it, or a
// power cut, never leaves half of one. Throws std::runtime_error naming the file when it cannot.
void WriteTokens(const std::filesystem::path &path, const TokenFile &file);

// The credential the token matches, compared in constant time; none for a token issued to nobody.
std::optional<Credential> Resolve(const TokenFile &file, std::string_view token);
} // namespace Manifest::Remote
