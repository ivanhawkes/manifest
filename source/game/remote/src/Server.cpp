#include <manifest/remote/Server.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include <jettison/core/Log.h>
#include <jettison/serialise/Archive.h>

#include <manifest/host/Start.h>
#include <manifest/query/Query.h>
#include <manifest/remote/Protocol.h>

namespace Manifest::Remote
{
namespace
{
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;

constexpr Jettison::Core::Category kServer{"server"};
constexpr FormatVersion kFormat{kVersion};


Jettison::Net::ServerConfig Config(std::string address, std::uint16_t port)
{
	Jettison::Net::ServerConfig config;
	config.address = std::move(address);
	config.port = port;
	config.protocol = std::string{kProtocol};
	config.versions = {kVersion};
	return config;
}


Command::Actor ActorOf(const Credential &credential)
{
	return {credential.kind, credential.id};
}


std::filesystem::file_time_type WrittenAt(const std::filesystem::path &path)
{
	std::error_code error;
	return std::filesystem::last_write_time(path, error);
}


template <typename T>
void Send(Jettison::Net::Server &net, Jettison::Net::SessionId id, const T &message)
{
	constexpr Kind kKind = std::is_same_v<T, Welcome> ? Kind::Welcome : Kind::Reply;
	(void)net.Send(
	    id, Jettison::Net::Channel::Reliable, static_cast<Jettison::Net::MessageKind>(kKind),
	    BinaryArchive::Write(message, kFormat));
}
} // namespace


Server::Server(
    Jettison::Host::Session &session, std::filesystem::path tokens, std::string address,
    std::uint16_t port)
    : m_hosted{session}, m_tokensPath{std::move(tokens)}, m_tokensWritten{WrittenAt(m_tokensPath)},
      m_tokens{ReadTokens(m_tokensPath)},
      m_net{
          Config(std::move(address), port),
          [this](Jettison::Net::SessionId id, std::span<const std::byte> credential)
          { return Verify(id, credential); }}
{
	RegisterTypes();
}


std::uint16_t Server::Port() const
{
	return m_net.Port();
}


bool Server::Verify(Jettison::Net::SessionId id, std::span<const std::byte> credential)
{
	const std::string_view token{reinterpret_cast<const char *>(credential.data()), credential.size()};
	std::optional<Credential> resolved = Resolve(m_tokens, token);
	if (!resolved)
		return false;
	m_sessions[id] = {std::move(*resolved), {}};
	return true;
}


void Server::Poll(std::chrono::milliseconds wait)
{
	ReloadTokens();
	using Event = Jettison::Net::ServerEvent;
	for (const Event &event : m_net.Poll(wait))
	{
		const auto session = m_sessions.find(event.session);
		if (session == m_sessions.end())
			continue;
		const Command::Actor actor = ActorOf(session->second.credential);
		if (event.type == Event::Type::Closed)
		{
			Jettison::Core::Log(
			    Jettison::Core::Severity::Info, kServer, "{} left: {}", actor.id,
			    Jettison::Net::ReasonName(event.reason));
			m_sessions.erase(session);
			continue;
		}
		Jettison::Core::Log(Jettison::Core::Severity::Info, kServer, "{} joined", actor.id);
		const Command::Requests &requests = Host::HostedServices(m_hosted).requests;
		Send(m_net, event.session, Welcome{actor.kind, actor.id, requests.NextSequence(actor)});
	}
	// A verified session whose admission has not yet been polled has no queue, so takes nothing.
	for (auto session = m_sessions.begin(); session != m_sessions.end();)
	{
		const auto next = std::next(session);
		Admit(session->first, session->second);
		session = next;
	}
}


void Server::Admit(Jettison::Net::SessionId id, Session &session)
{
	Command::Requests &requests = Host::HostedServices(m_hosted).requests;
	const Command::Actor actor = ActorOf(session.credential);
	while (const std::optional<Jettison::Net::Message> message = m_net.Receive(id))
	{
		Submit submit;
		try
		{
			if (message->kind != static_cast<Jettison::Net::MessageKind>(Kind::Submit))
				throw std::runtime_error{"not a request"};
			submit = BinaryArchive::Read<Submit>(message->body, kFormat);
		}
		catch (const std::exception &error)
		{
			Jettison::Core::Log(
			    Jettison::Core::Severity::Warning, kServer, "{} sent a bad message: {}", actor.id,
			    error.what());
			m_net.Disconnect(id, Jettison::Net::Reason::BadMessage);
			m_sessions.erase(id);
			return;
		}
		const Command::Receipt receipt = requests.Admit(
		    m_hosted.World(), actor, submit.sequence, submit.command, submit.version,
		    std::as_bytes(std::span{submit.payload}));
		if (receipt.status == Command::Receipt::Status::Queued)
			session.pending.push_back(submit.sequence);
		Send(m_net, id, Reply{submit.sequence, receipt, requests.NextSequence(actor)});
	}
}


void Server::Publish()
{
	const Host::Services services = Host::HostedServices(m_hosted);
	for (auto &[id, session] : m_sessions)
	{
		const Command::Actor actor = ActorOf(session.credential);
		std::erase_if(
		    session.pending,
		    [&](std::uint64_t sequence)
		    {
			    const std::optional<Command::Completed> outcome =
			        services.requests.Outcome(actor, sequence);
			    if (outcome)
				    Send(
				        m_net, id,
				        Reply{
				            sequence,
				            {Command::Receipt::Status::Completed, outcome->result,
				             outcome->request.order, outcome->step},
				            services.requests.NextSequence(actor)});
			    return outcome.has_value();
		    });
	}
	const Jettison::Sim::SimWorld &world = m_hosted.World();
	m_net.SendSnapshots(
	    world.Clock().Step(),
	    [&](Jettison::Net::SessionId id) -> std::optional<std::vector<std::byte>>
	    {
		    const auto session = m_sessions.find(id);
		    if (session == m_sessions.end())
			    return std::nullopt;
		    return BinaryArchive::Write(
		        Query::Observe(world, services.identities, ActorOf(session->second.credential)),
		        kFormat);
	    });
}


void Server::ReloadTokens()
{
	const std::filesystem::file_time_type written = WrittenAt(m_tokensPath);
	if (written == m_tokensWritten)
		return;
	m_tokensWritten = written;
	try
	{
		m_tokens = ReadTokens(m_tokensPath);
	}
	catch (const std::runtime_error &error)
	{
		// The last good set stays: a token is revoked by the CLI rewriting the file, not by losing it.
		Jettison::Core::Log(Jettison::Core::Severity::Warning, kServer, "{}", error.what());
		return;
	}
	for (auto session = m_sessions.begin(); session != m_sessions.end();)
	{
		const bool kept = std::ranges::any_of(
		    m_tokens.tokens, [&](const Credential &credential)
		    { return credential.digest == session->second.credential.digest; });
		if (kept)
		{
			++session;
			continue;
		}
		Jettison::Core::Log(
		    Jettison::Core::Severity::Info, kServer, "{}'s token was revoked",
		    session->second.credential.id);
		m_net.Disconnect(session->first, Jettison::Net::Reason::Revoked);
		session = m_sessions.erase(session);
	}
}
} // namespace Manifest::Remote
