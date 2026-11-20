// manifest <options> --check-startup (Manifest::Host::kOptionsUsage): starts Manifest on the content,
// reports it ready, shuts down and exits 0.
// manifest <options> --serve --tokens <file> [--address <ipv4>] [--port <n>]: the dedicated server.
// It steps the world unattended at its fixed step, with or without clients, admits the clients the
// token file names (manifest-token), and stops cleanly on SIGINT or SIGTERM. The port defaults to 0,
// an ephemeral one, which the serving line names.
// A failed start-up exits 1, a bad command line 2.
#include <manifest/host/Start.h>
#include <manifest/remote/Server.h>

#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/core/Log.h>
#include <jettison/time/FixedStep.h>

namespace
{
constexpr Jettison::Core::Category kManifest{"manifest"};

volatile std::sig_atomic_t g_stopping = 0;


int Usage()
{
	// The usage line goes to stderr directly, as the engine shell's does; the exit status pairs it.
	const std::string_view options = Manifest::Host::kOptionsUsage;
	std::fputs(
	    std::format(
	        "usage: manifest {} --check-startup\n"
	        "       manifest {} --serve --tokens <file> [--address <ipv4>] [--port <n>]\n",
	        options, options)
	        .c_str(),
	    stderr);
	return 2;
}


struct Serve
{
	std::string tokens;
	std::string address{"127.0.0.1"};
	std::uint16_t port{0};
};


// None unless the rest is --serve then --tokens and the optional endpoint, each once.
std::optional<Serve> ParseServe(const std::vector<std::string_view> &rest)
{
	if (rest.empty() || rest[0] != "--serve" || rest.size() % 2 == 0)
		return std::nullopt;
	Serve serve;
	bool tokens = false;
	bool address = false;
	bool port = false;
	for (std::size_t index = 1; index < rest.size(); index += 2)
	{
		const std::string_view value = rest[index + 1];
		if (rest[index] == "--tokens" && !std::exchange(tokens, true))
			serve.tokens = value;
		else if (rest[index] == "--address" && !std::exchange(address, true))
			serve.address = value;
		else if (rest[index] == "--port" && !std::exchange(port, true))
		{
			const auto [end, error] =
			    std::from_chars(value.data(), value.data() + value.size(), serve.port);
			if (error != std::errc{} || end != value.data() + value.size())
				return std::nullopt;
		}
		else
			return std::nullopt;
	}
	if (!tokens)
		return std::nullopt;
	return serve;
}


// Steps the world at its fixed step against the wall clock, polling between steps, until a signal.
void RunServer(Jettison::Host::Session &session, const Serve &serve)
{
	Manifest::Remote::Server server{session, serve.tokens, serve.address, serve.port};
	Jettison::Sim::SimWorld &world = session.World();
	std::signal(SIGINT, [](int) { g_stopping = 1; });
	std::signal(SIGTERM, [](int) { g_stopping = 1; });
	Jettison::Core::Log(
	    Jettison::Core::Severity::Info, kManifest, "serving on {}:{} at step {}", serve.address,
	    server.Port(), world.Clock().Step());

	Jettison::Time::FixedStepAccumulator steps{world.FixedStep()};
	auto last = std::chrono::steady_clock::now();
	while (g_stopping == 0)
	{
		server.Poll(std::chrono::milliseconds{10});
		const auto now = std::chrono::steady_clock::now();
		const double elapsed = std::chrono::duration<double>(now - last).count();
		last = now;
		const int count = steps.Accumulate(world.Clock().ScaledDelta(elapsed));
		// The steps the time makes, quiet ones passed over at once (Jettison::Sim::Quiet).
		const std::uint64_t target = world.Clock().Step() + static_cast<std::uint64_t>(count);
		while (world.Clock().Step() < target)
			world.FastForward(target);
		// What the engine raises is presentation's, and a dedicated server presents none of it.
		(void)world.Events().Take();
		if (count > 0)
			server.Publish();
	}
	Jettison::Core::Log(
	    Jettison::Core::Severity::Info, kManifest, "stopping at step {}", world.Clock().Step());
}
} // namespace


int main(int argc, char **argv)
{
	const std::vector<std::string_view> arguments(argv + 1, argv + argc);
	std::vector<std::string_view> rest;
	const std::optional<Manifest::Host::Options> options =
	    Manifest::Host::ParseOptions(arguments, rest);
	const bool check = rest == std::vector<std::string_view>{"--check-startup"};
	const std::optional<Serve> serve = check ? std::nullopt : ParseServe(rest);
	if (!options || (!check && !serve))
		return Usage();

	using Jettison::Core::Severity;
	try
	{
		const auto session = Manifest::Host::Start(*options);
		Jettison::Core::Log(
		    Severity::Info, kManifest, "ready, from {}, scenario {}", options->content.string(),
		    options->scenario);
		if (serve)
			RunServer(*session, *serve);
		session->Shutdown();
	}
	catch (const std::runtime_error &error)
	{
		// A failed start-up (Jettison::Host::StartupError), or the token file or the address the
		// server could not take.
		Jettison::Core::Log(Severity::Error, kManifest, "{}", error.what());
		return 1;
	}
	return 0;
}
