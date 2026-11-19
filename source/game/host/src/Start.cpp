#include <manifest/host/Start.h>

#include <cstdint>
#include <exception>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>

#include <jettison/host/Package.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/content/Content.h>

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

// shortcut: the Manifest game mode until C06's options select a scenario, which names its mode.
constexpr std::string_view kGameMode = "manifest";

// shortcut: one-second steps and seed 0 while the world is empty; A03 selects the cadence and a
// scenario its seed.
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


class Game final : public Jettison::Host::Game
{
  public:
	void LoadContent(
	    const Jettison::Vfs::FileSystem &files, std::span<const Package> packages,
	    Jettison::Sim::DefinitionRegistry &definitions) override
	{
		// The session reports a hook's failure without a source; this one has the source to name.
		try
		{
			CheckPackage(packages.front());
			m_content = Content::LoadContent(files, definitions, kGameMode);
		}
		catch (const std::exception &problem)
		{
			throw StartupError{Phase::Content, files.Mounts().front().source, problem.what()};
		}
	}

  private:
	// What the world will run on, once A03 builds one.
	Content::ContentSet m_content;
};
} // namespace


std::unique_ptr<Jettison::Host::Session> Start(const std::filesystem::path &content)
{
	return std::make_unique<Jettison::Host::Session>(
	    Jettison::Host::Options{{{"manifest", content}}, kFixedStep, kSeed}, std::make_unique<Game>());
}
} // namespace Manifest::Host
