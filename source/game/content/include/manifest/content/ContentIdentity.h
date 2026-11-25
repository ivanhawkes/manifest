#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/serialise/Archive.h>

namespace Jettison::Sim
{
class DefinitionRegistry;
}

namespace Manifest::Content
{
// The canonical manifest of a content set and its fingerprint (docs/content.md, "Content identity").
// The manifest is what a save compares exactly; the fingerprint is a 64-bit FNV-1a of it, a
// compatibility aid that a list can show, not an authenticity check.
//
// Format "Manifest.ContentManifest", in Jettison's binary layout (BinaryWriter): for each type of
// DefinitionTypes, in that order, its reflected name, its format version (4 bytes) and its record
// count (4 bytes), then each record in name order, as its name and its BinaryArchive bytes after
// their length, then the selected scenario's name, empty for a set selected by game mode alone.
// Every loaded definition is in it, used by the selected scenario or not; host paths,
// mounts, pack compression and times are not, so a directory and a pack of the same content give
// the same bytes. A change to that layout, or to the hash, is a new version.
inline constexpr Jettison::Serialise::FormatVersion kContentManifestFormat{1};

struct ContentIdentity
{
	std::vector<std::byte> manifest;
	std::uint64_t fingerprint{0};
};

// The registry holds every type of DefinitionTypes, as LoadDefinitions leaves it.
ContentIdentity BuildContentIdentity(
    const Jettison::Sim::DefinitionRegistry &registry, std::string_view scenario);

// What a refusal names of two manifests this build made (ContentIdentity::manifest), a world's and
// the running content's: the first definition, in the manifest's order, that the two do not hold
// alike, by its type and name, or else the scenario; empty for two alike. Throws
// Jettison::Serialise::FormatError for one that is not a manifest.
std::string FirstDifference(std::span<const std::byte> was, std::span<const std::byte> is);
} // namespace Manifest::Content
