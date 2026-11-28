// manifest <options> --check-startup (Manifest::Host::kOptionsUsage): starts Manifest on the content,
// reports it ready, shuts down and exits 0.
// manifest <options> --serve [--world <directory>] [--tokens <file>] [--key <file>]
// [--address <ipv4>] [--port <n>] [--flush-seconds <n>] [--snapshot-seconds <n>]
// [--backups <directory>]: the dedicated server. It holds the world's directory, makes the world there
// or recovers it from its newest snapshot and the journal after it, then steps it unattended at its
// fixed step, with or without clients. It writes what has run to the journal every flush interval,
// 30 s unless given, which is the most a crash loses, and a snapshot every snapshot interval, an hour
// unless given (Manifest::Host::Journal), with a backup of each in the directory for them, outside the
// world's. It admits the clients the token file names (manifest-token), sealing every connection to
// keys made with the secret key manifest-token key wrote, whose public half each client pins, and
// stops cleanly on SIGINT or SIGTERM: it admits nothing more, runs what it admitted in one last step,
// answers it and writes a snapshot, losing nothing. The port defaults to 0, an ephemeral one, which
// the serving line names, with the public half of the key.
// manifest <options> --restore <backup> [--world <directory>]: restores the backup into the
// directory, which must be empty, and recovers it to check it, for --serve to serve. The backup is
// only read.
// The world, its backups, the token file and the key default to Manifest::Host::DefaultWorld's and
// the others', each directory made private when it is first written to. A server's crash writes a
// report (Jettison::Platform::InstallCrashHandler) to $XDG_STATE_HOME/manifest/crashes. A failed
// start-up exits 1, a bad command line 2.
#include <manifest/host/Journal.h>
#include <manifest/host/Locations.h>
#include <manifest/host/Start.h>
#include <manifest/host/WorldDirectory.h>
#include <manifest/remote/Server.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/core/Log.h>
#include <jettison/platform/Crash.h>
#include <jettison/platform/Paths.h>
#include <jettison/script/ScriptHost.h>
#include <jettison/sim/Events.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/time/FixedStep.h>

namespace
{
constexpr Jettison::Core::Category kManifest{"manifest"};

// How long the server waits for a request between steps, and at a clean stop for its clients to
// acknowledge their last replies: a client on a LAN answers in milliseconds, one gone never.
constexpr std::chrono::milliseconds kPollWait{10};
constexpr std::chrono::milliseconds kStopWait{2'000};

volatile std::sig_atomic_t g_stopping = 0;


int Usage()
{
	// The usage line goes to stderr directly, as the engine shell's does; the exit status pairs it.
	const std::string_view options = Manifest::Host::kOptionsUsage;
	std::fputs(
	    std::format(
	        "usage: manifest {} --check-startup\n"
	        "       manifest {} --serve [--world <directory>] [--tokens <file>] [--key <file>] "
	        "[--address <ipv4>] [--port <n>] [--flush-seconds <n>] [--snapshot-seconds <n>] "
	        "[--backups <directory>]\n"
	        "       manifest {} --restore <backup> [--world <directory>]\n",
	        options, options, options)
	        .c_str(),
	    stderr);
	return 2;
}


// A place an option names, or none for its default.
using Place = std::optional<std::filesystem::path>;


// The place given, or the default, whose directory is made private to its owner when it is missing.
std::filesystem::path ResolvePlace(const Place &place, std::filesystem::path (*fallback)())
{
	if (place)
		return *place;
	std::filesystem::path path = fallback();
	Manifest::Host::MakePrivateDirectories(path.parent_path());
	return path;
}


struct Serve
{
	Place world;
	Place tokens;
	Place key;
	Place backups;
	std::string address{"127.0.0.1"};
	std::uint16_t port{0};

	// What a crash may lose, and what a start may replay (D-113).
	std::uint32_t flushSeconds{30};
	std::uint32_t snapshotSeconds{3'600};
};


// Reads a whole decimal number, false for anything else or one the type cannot hold.
template <typename T> bool ReadNumber(std::string_view text, T &number)
{
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
	return error == std::errc{} && end == text.data() + text.size();
}


// None unless the rest is --serve then the optional world, tokens, key, endpoint, intervals and
// backups, each once, an interval at least a second.
std::optional<Serve> ParseServe(const std::vector<std::string_view> &rest)
{
	if (rest.empty() || rest[0] != "--serve" || rest.size() % 2 == 0)
		return std::nullopt;
	Serve serve;
	bool directory = false;
	bool tokens = false;
	bool key = false;
	bool address = false;
	bool port = false;
	bool flush = false;
	bool snapshot = false;
	bool backups = false;
	for (std::size_t index = 1; index < rest.size(); index += 2)
	{
		const std::string_view value = rest[index + 1];
		if (rest[index] == "--world" && !std::exchange(directory, true))
			serve.world = value;
		else if (rest[index] == "--tokens" && !std::exchange(tokens, true))
			serve.tokens = value;
		else if (rest[index] == "--key" && !std::exchange(key, true))
			serve.key = value;
		else if (rest[index] == "--backups" && !std::exchange(backups, true))
			serve.backups = value;
		else if (rest[index] == "--address" && !std::exchange(address, true))
			serve.address = value;
		else if (rest[index] == "--port" && !std::exchange(port, true))
		{
			if (!ReadNumber(value, serve.port))
				return std::nullopt;
		}
		else if (rest[index] == "--flush-seconds" && !std::exchange(flush, true))
		{
			if (!ReadNumber(value, serve.flushSeconds) || serve.flushSeconds == 0)
				return std::nullopt;
		}
		else if (rest[index] == "--snapshot-seconds" && !std::exchange(snapshot, true))
		{
			if (!ReadNumber(value, serve.snapshotSeconds) || serve.snapshotSeconds == 0)
				return std::nullopt;
		}
		else
			return std::nullopt;
	}
	return serve;
}


// Steps the world at its fixed step against the wall clock, polling between steps, until a signal.
void RunServer(Jettison::Host::Session &session, const Serve &serve)
{
	Jettison::Sim::SimWorld &world = session.World();
	const std::filesystem::path backups = ResolvePlace(serve.backups, Manifest::Host::DefaultBackups);
	const std::filesystem::path tokens = ResolvePlace(serve.tokens, Manifest::Host::DefaultTokens);
	const std::filesystem::path key = ResolvePlace(serve.key, Manifest::Host::DefaultServerKey);

	// Before the world is touched, so a crash in its recovery is reported too. A release's reports are
	// named against its kept debug files (scripts/symbolise-crash.sh, scripts/stage-bundle.sh).
	const std::filesystem::path crashes =
	    Jettison::Platform::ResolvePath(Jettison::Platform::Directory::State, "manifest") / "crashes";
	Manifest::Host::MakePrivateDirectories(crashes);
	const Manifest::Host::Identity identity = Manifest::Host::RunningIdentity(session);
	Jettison::Platform::InstallCrashHandler(
	    {crashes, std::format("Manifest {}, engine {}", identity.game, identity.sdk), {}, true});

	// Before a client can reach the server: the world held, and brought to its journal's last step.
	const Manifest::Host::WorldDirectory directory =
	    Manifest::Host::OpenWorldDirectory(ResolvePlace(serve.world, Manifest::Host::DefaultWorld));
	Manifest::Host::Storage storage;
	storage.backups = backups;
	storage.pScripts = &session.Scripts();
	const auto stepsIn = [&](std::uint32_t seconds)
	{
		return std::max<std::uint64_t>(
		    1, static_cast<std::uint64_t>(std::llround(seconds / world.FixedStep())));
	};
	Manifest::Host::Journal journal{
	    directory,
	    Manifest::Host::RunningIdentity(session),
	    {stepsIn(serve.flushSeconds), stepsIn(serve.snapshotSeconds)},
	    {},
	    storage};
	journal.Recover(world);

	// The mods' scripts read the world as the last steps left it, as a client's shell has them do.
	Jettison::Script::ScriptHost &scripts = session.Scripts();
	Jettison::Sim::Snapshot shown = Jettison::Sim::Capture(world);
	scripts.ReadFrom(shown);
	Manifest::Remote::Server server{session, tokens, key, serve.address, serve.port};
	std::signal(SIGINT, [](int) { g_stopping = 1; });
	std::signal(SIGTERM, [](int) { g_stopping = 1; });
	Jettison::Core::Log(
	    Jettison::Core::Severity::Info, kManifest,
	    "serving on {}:{} at step {}, world {} at batch {}, key {}", serve.address, server.Port(),
	    world.Clock().Step(), journal.World(), journal.Head(), server.PublicKey());

	Jettison::Time::FixedStepAccumulator steps{world.FixedStep()};
	auto last = std::chrono::steady_clock::now();
	while (g_stopping == 0)
	{
		server.Poll(kPollWait);
		const auto now = std::chrono::steady_clock::now();
		const double elapsed = std::chrono::duration<double>(now - last).count();
		last = now;
		const auto count =
		    static_cast<std::uint64_t>(steps.Accumulate(world.Clock().ScaledDelta(elapsed)));
		if (count == 0)
			continue;

		// The steps the time makes, quiet ones passed over at once (Jettison::Sim::Quiet), written to
		// the journal when a flush is due. A write that fails throws, and the server stops: the next
		// start finds what the journal holds.
		journal.Advance(world, count);

		// What the world raised goes to the mods' scripts, as a client's shell gives it them; a
		// dedicated server presents nothing else of it.
		const std::vector<Jettison::Sim::SimEvent> events = world.Events().Take();
		if (!events.empty())
		{
			shown = Jettison::Sim::Capture(world);
			for (const Jettison::Sim::SimEvent &event : events)
				scripts.Dispatch(event);
		}
		server.Publish();
	}

	// Nothing is admitted from here; what was is run, answered and kept. A write that fails throws, and
	// no stop is reported.
	journal.Stop(world);
	server.Publish();
	server.Stop();
	const auto stopBy = std::chrono::steady_clock::now() + kStopWait;
	while (server.Stopping() && std::chrono::steady_clock::now() < stopBy)
		server.Poll(kPollWait);

	// The time dropped is the steps the disk or the step itself could not keep to the clock.
	Jettison::Core::Log(
	    Jettison::Core::Severity::Info, kManifest, "stopping at step {}, batch {}, {:.1f} s dropped",
	    world.Clock().Step(), journal.Head(), steps.DroppedTime());
}


struct Restoring
{
	std::filesystem::path backup;
	Place world;
};


// None unless the rest is --restore <backup>, then --world <directory> or nothing.
std::optional<Restoring> ParseRestore(const std::vector<std::string_view> &rest)
{
	if (rest.size() == 2 && rest[0] == "--restore")
		return Restoring{rest[1], std::nullopt};
	if (rest.size() != 4 || rest[0] != "--restore" || rest[2] != "--world")
		return std::nullopt;
	return Restoring{rest[1], std::filesystem::path{rest[3]}};
}


void RunRestore(Jettison::Host::Session &session, const Restoring &restoring)
{
	const std::filesystem::path into = ResolvePlace(restoring.world, Manifest::Host::DefaultWorld);
	const Manifest::Host::WorldDirectory directory = Manifest::Host::OpenWorldDirectory(into);
	const Manifest::Host::Journal journal = Manifest::Host::Restore(
	    restoring.backup, directory, Manifest::Host::RunningIdentity(session), session.World());
	Jettison::Core::Log(
	    Jettison::Core::Severity::Info, kManifest, "restored world {} at batch {}, step {}, into {}",
	    journal.World(), journal.Head(), session.World().Clock().Step(), into.string());
}
} // namespace


int main(int argc, char **argv)
{
	using Jettison::Core::Severity;
	try
	{
		const std::vector<std::string_view> arguments(argv + 1, argv + argc);
		std::vector<std::string_view> rest;
		const std::optional<Manifest::Host::Options> options =
		    Manifest::Host::ParseOptions(arguments, rest);
		const bool check = rest == std::vector<std::string_view>{"--check-startup"};
		const std::optional<Serve> serve = check ? std::nullopt : ParseServe(rest);
		const std::optional<Restoring> restoring = check ? std::nullopt : ParseRestore(rest);
		if (!options || (!check && !serve && !restoring))
			return Usage();

		const auto session = Manifest::Host::Start(*options);
		Jettison::Core::Log(
		    Severity::Info, kManifest, "ready, from {}, scenario {}", options->content.string(),
		    options->scenario);
		if (serve)
			RunServer(*session, *serve);
		if (restoring)
			RunRestore(*session, *restoring);
		session->Shutdown();
	}
	catch (const std::runtime_error &error)
	{
		// A failed start-up (Jettison::Host::StartupError), the default content or place that cannot
		// be found, the world's directory, journal, token file, key or address the server could not
		// take, a write of the journal's that failed, or a backup that does not restore.
		Jettison::Core::Log(Severity::Error, kManifest, "{}", error.what());
		return 1;
	}
	return 0;
}
