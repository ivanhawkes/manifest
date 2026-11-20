#include <manifest/host/Start.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <jettison/host/Package.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/command/Requests.h>
#include <manifest/company/Company.h>
#include <manifest/trade/Trade.h>
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

// shortcut: one-second steps and seed 0 while no system steps the world; A03 selects the cadence and
// a scenario its seed.
constexpr double kFixedStep = 1.0;
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
	// The session reports a hook's failure without a source; this one has the source to name.
	try
	{
		CheckPackage(packages.front());
		return Content::LoadScenario(files, definitions, scenario);
	}
	catch (const std::exception &problem)
	{
		throw StartupError{Phase::Content, files.Mounts().front().source, problem.what()};
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
		Company::RegisterTypes();
		Trade::RegisterTypes();
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
		Company::RegisterCommands(*m_pRequests, *m_pIdentities, m_content);
		Trade::Register(world, *m_pRequests, *m_pIdentities, m_content);
	}


	void CreateInitialState(Jettison::Sim::SimWorld &world) override
	{
		World::CreateInitialState(world, *m_pIdentities, m_content);
		Trade::Quote(world, *m_pIdentities, m_content);
	}

	Services WorldServices() const
	{
		return {*m_pIdentities, *m_pRequests};
	}

  private:
	std::string m_scenario;
	Content::ContentSet m_content;
	// The world's own services, which outlive every call the session makes here.
	World::Identities *m_pIdentities{nullptr};
	Command::Requests *m_pRequests{nullptr};
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
	if (!content || !scenario)
		return std::nullopt;
	return Options{*content, std::move(overlays), *scenario};
}


std::vector<Jettison::Vfs::Mount> Mounts(const Options &options)
{
	std::vector<Jettison::Vfs::Mount> mounts{{std::string{kPackageMount}, options.content}};
	for (const std::filesystem::path &overlay : options.overlays)
	{
		std::optional<PackageDescription> description;
		try
		{
			description = Jettison::Host::DescribePackage(overlay);
		}
		catch (const std::exception &problem)
		{
			throw StartupError{Phase::Mount, overlay, problem.what()};
		}
		if (!description)
			throw StartupError{Phase::Mount, overlay, "an overlay has no mod.yaml to name it"};
		if (description->name.empty())
			throw StartupError{Phase::Mount, overlay, "its mod.yaml gives no name"};
		if (std::ranges::find(mounts, description->name, &Jettison::Vfs::Mount::name) != mounts.end())
			throw StartupError{
			    Phase::Mount, overlay, std::format("'{}' is already mounted", description->name)};
		mounts.push_back({description->name, overlay});
	}
	return mounts;
}


std::unique_ptr<Jettison::Host::Session> Start(const Options &options)
{
	return std::make_unique<Jettison::Host::Session>(
	    Jettison::Host::Options{Mounts(options), kFixedStep, kSeed},
	    std::make_unique<Game>(options.scenario));
}


Services HostedServices(Jettison::Host::Session &session)
{
	return dynamic_cast<const Game &>(session.GetGame()).WorldServices();
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
