#include <manifest/host/Start.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <jettison/host/Package.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/command/Requests.h>
#include <manifest/company/Company.h>
#include <manifest/host/Locations.h>
#include <manifest/policy/Policy.h>
#include <manifest/procurement/Procurement.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/World.h>

namespace Manifest::Host
{
namespace
{
using Jettison::Host::Package;
using Jettison::Host::PackageDescription;
using Jettison::Host::Phase;
using Jettison::Host::StartupError;

// The name mod.yaml gives, not the source's file name: a renamed pack is still Manifest.
constexpr std::string_view kPackageName = "Manifest";

// The package's mount, which an overlay's requires names.
constexpr std::string_view kPackageMount = "manifest";

// The engine's own assets, which Jettison's shell mounts below a game's content (App::MountAssets).
constexpr std::string_view kEngineMount = "engine";

// An overlay's mount is "mod:<identifier>", as Jettison's shell mounts a player's mod
// (App::MountAssets), so the same world mod on a server and its client compares equal
// (Host::ContentDifference).
constexpr std::string_view kModPrefix = "mod:";
constexpr std::string_view kPackExtension = ".pack";

// shortcut: seed 0, nothing drawing on the world's generator yet; a scenario gives its seed once
// something does.
constexpr std::uint64_t kSeed = 0;


void CheckPackage(const Package &package)
{
	if (!package.description)
		throw std::runtime_error{"it has no mod.yaml"};
	const PackageDescription &description = *package.description;
	if (description.name.empty())
		throw std::runtime_error{"its mod.yaml gives no name"};
	if (description.name != kPackageName)
		throw std::runtime_error{
		    std::format("it is the package '{}', not {}", description.name, kPackageName)};
	if (description.release.empty())
		throw std::runtime_error{"its mod.yaml gives no release"};
	if (description.engine.empty())
		throw std::runtime_error{"its mod.yaml gives no engine requirement"};
}


// The content phase's own step, the host's and the validator's.
Content::ContentSet LoadContent(
    const Jettison::Vfs::FileSystem &files, std::span<const Package> packages,
    Jettison::Sim::DefinitionRegistry &definitions, std::string_view scenario)
{
	const auto package =
	    std::ranges::find_if(packages, [](const Package &each) { return each.mount != kEngineMount; });
	const auto mount = std::ranges::find(
	    files.Mounts(), package != packages.end() ? package->mount : std::string{},
	    &Jettison::Vfs::Mount::name);

	// The session reports a hook's failure without a source; this one has the source to name.
	try
	{
		if (package == packages.end())
			throw std::runtime_error{"there is no content over the engine's assets"};
		CheckPackage(*package);
		return Content::LoadScenario(files, definitions, scenario);
	}
	catch (const std::exception &problem)
	{
		throw StartupError{
		    Phase::Content, mount != files.Mounts().end() ? mount->source : std::filesystem::path{},
		    problem.what()};
	}
}


class Game final : public Jettison::Host::Game
{
  public:
	explicit Game(std::string scenario) : m_scenario{std::move(scenario)}
	{
	}


	void RegisterTypes() override
	{
		Travel::RegisterTypes();
		Company::RegisterTypes();
		Trade::RegisterTypes();
		Policy::RegisterTypes();
		Procurement::RegisterTypes();
	}


	void LoadContent(
	    const Jettison::Vfs::FileSystem &files, std::span<const Package> packages,
	    Jettison::Sim::DefinitionRegistry &definitions) override
	{
		m_content = Host::LoadContent(files, packages, definitions, m_scenario);
	}


	void RegisterWorld(
	    Jettison::Sim::SimWorld &world,
	    const Jettison::Sim::DefinitionRegistry & /*definitions*/) override
	{
		m_pIdentities = &World::RegisterWorld(world);
		m_pRequests = &Command::RegisterRequests(world, *m_pIdentities);
		m_pFleet = &Travel::Register(world, *m_pRequests, *m_pIdentities, m_content);
		Company::RegisterCommands(world, *m_pRequests, *m_pIdentities, *m_pFleet, m_content);
		Trade::Register(world, *m_pRequests, *m_pIdentities, m_content);
		Policy::Register(world, *m_pRequests, *m_pIdentities, m_content);
		Procurement::Register(world, *m_pRequests, *m_pIdentities, m_content);
	}


	void CreateInitialState(Jettison::Sim::SimWorld &world) override
	{
		World::CreateInitialState(world, *m_pIdentities, m_content);
		Travel::OpenStations(world, *m_pFleet);
		Trade::Quote(world, *m_pIdentities, m_content);
		Policy::ProvisionTraders(world, *m_pRequests, *m_pIdentities, m_content);
	}


	Services WorldServices() const
	{
		if (m_pRequests == nullptr)
			throw std::logic_error{"Manifest's game has not been started by a session"};
		return {*m_pIdentities, *m_pRequests, *m_pFleet, m_content.identity};
	}

  private:
	std::string m_scenario;
	Content::ContentSet m_content;

	// The world's own services, which outlive every call the session makes here.
	World::Identities *m_pIdentities{nullptr};
	Command::Requests *m_pRequests{nullptr};
	Travel::Fleet *m_pFleet{nullptr};
};
} // namespace


std::optional<Options> ParseOptions(
    std::span<const std::string_view> arguments, std::vector<std::string_view> &rest)
{
	std::optional<std::filesystem::path> content;
	std::vector<std::filesystem::path> overlays;
	std::optional<std::string> scenario;
	for (auto at = arguments.begin(); at != arguments.end(); ++at)
	{
		const std::string_view argument = *at;
		if (argument != "--content" && argument != "--overlay" && argument != "--scenario")
		{
			rest.push_back(argument);
			continue;
		}
		if (++at == arguments.end())
			return std::nullopt;
		if (argument == "--overlay")
			overlays.emplace_back(*at);
		else if (argument == "--content" ? content.has_value() : scenario.has_value())
			return std::nullopt;
		else if (argument == "--content")
			content = *at;
		else
			scenario = *at;
	}
	if (!scenario)
		return std::nullopt;
	return Options{content ? *content : DefaultContent(), std::move(overlays), *scenario};
}


std::vector<Jettison::Vfs::Mount> Mounts(const Options &options)
{
	std::vector<Jettison::Vfs::Mount> mounts{{std::string{kPackageMount}, options.content}};
	for (const std::filesystem::path &overlay : options.overlays)
	{
		// The shell's identifier: the directory's name, or the pack's without its extension.
		const std::filesystem::path named = overlay.has_filename() ? overlay : overlay.parent_path();
		const std::string name =
		    std::string{kModPrefix} +
		    (named.extension() == kPackExtension ? named.stem().string() : named.filename().string());
		if (std::ranges::find(mounts, name, &Jettison::Vfs::Mount::name) != mounts.end())
			throw StartupError{Phase::Mount, overlay, std::format("'{}' is already mounted", name)};
		mounts.push_back({name, overlay});
	}
	return mounts;
}


std::unique_ptr<Jettison::Host::Session> Start(const Options &options)
{
	Jettison::Host::Options hosted{Mounts(options), kFixedStep, kSeed};
	hosted.scripts = Jettison::Script::HostLimits{};
	return std::make_unique<Jettison::Host::Session>(std::move(hosted), MakeGame(options.scenario));
}


std::unique_ptr<Jettison::Host::Game> MakeGame(std::string scenario)
{
	return std::make_unique<Game>(std::move(scenario));
}


Services HostedServices(Jettison::Host::Session &session)
{
	return HostedServices(session.GetGame());
}


Services HostedServices(Jettison::Host::Game &game)
{
	return dynamic_cast<const Game &>(game).WorldServices();
}


Content::ContentSet Validate(const Options &options)
{
	Jettison::Vfs::FileSystem files;
	for (const Jettison::Vfs::Mount &mount : Mounts(options))
		try
		{
			files.AddMount(mount);
		}
		catch (const std::exception &problem)
		{
			throw StartupError{Phase::Mount, mount.source, problem.what()};
		}
	Jettison::Sim::DefinitionRegistry definitions{files};
	return LoadContent(files, Jettison::Host::DescribePackages(files), definitions, options.scenario);
}
} // namespace Manifest::Host
