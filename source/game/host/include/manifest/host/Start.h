#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/vfs/Mount.h>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>

namespace Manifest::Host
{
// What a host and the validator run on, given to both the same way: the Manifest package, the
// overlays mounted over it in order, a later one winning, and the scenario, which names its game
// mode. Nothing is found from the working directory.
struct Options
{
	std::filesystem::path content;
	std::vector<std::filesystem::path> overlays;
	std::string scenario;
};

// The options as a usage line writes them.
inline constexpr std::string_view kOptionsUsage =
    "--content <directory-or-pack> [--overlay <directory-or-pack>]... --scenario <name>";

// Reads the options from a command line, leaving every other argument in `rest`, in order. None
// when --content or --scenario is missing or given twice, or an option has no value.
std::optional<Options> ParseOptions(
    std::span<const std::string_view> arguments, std::vector<std::string_view> &rest);

// The sources in mount order: the package as "manifest", then each overlay under the name its
// mod.yaml gives, which a later overlay's requires names. Throws Jettison::Host::StartupError in the
// mount phase for an overlay with no mod.yaml or no name, or a name already mounted.
std::vector<Jettison::Vfs::Mount> Mounts(const Options &options);

// Starts Manifest on the sources, mounted read-only: no user mods, no writable overlay. The first
// must be the Manifest package: a mod.yaml naming it, with its release and engine requirement
// given. Every package's engine requirement and required packages are satisfied, and the
// definitions are loaded and checked as one content set for the scenario
// (Manifest::Content::LoadScenario). The world it starts holds the scenario's state
// (Manifest::World::CreateInitialState) and the brokers' first quotes (Manifest::Trade::Quote). Throws
// Jettison::Host::StartupError naming the phase, the source and the cause, having released everything
// it acquired.
std::unique_ptr<Jettison::Host::Session> Start(const Options &options);

// The services of a started session's world, which it owns: what a frontend admits requests to and
// observes through, with the world. Throws std::bad_cast for a session Start did not return.
struct Services
{
	World::Identities &identities;
	Command::Requests &requests;
};
Services HostedServices(Jettison::Host::Session &session);

// What Start loads, with no world: the same sources, package rules and content set, failing as
// Start fails. The validator's check.
Content::ContentSet Validate(const Options &options);
} // namespace Manifest::Host
