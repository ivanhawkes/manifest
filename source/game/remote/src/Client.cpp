#include <manifest/remote/Client.h>

#include <jettison/serialise/Archive.h>

namespace Manifest::Remote
{
namespace
{
using Jettison::Serialise::BinaryArchive;

constexpr Jettison::Serialise::FormatVersion kFormat{kVersion};


Jettison::Net::ClientConfig Config(std::string address, std::uint16_t port, std::string_view token)
{
	RegisterTypes();
	Jettison::Net::ClientConfig config;
	config.address = std::move(address);
	config.port = port;
	config.protocol = {std::string{kProtocol}, kVersion};
	const auto bytes = std::as_bytes(std::span{token});
	config.credential.assign(bytes.begin(), bytes.end());
	return config;
}
} // namespace


Client::Client(std::string address, std::uint16_t port, std::string_view token)
    : m_net{Config(std::move(address), port, token)}
{
}


std::vector<Reply> Client::Poll(std::chrono::milliseconds wait)
{
	using Event = Jettison::Net::ClientEvent;
	std::vector<Reply> replies;
	for (Event &event : m_net.Poll(wait))
	{
		// The server is trusted as far as its own encoding: a body that does not decode throws.
		switch (event.type)
		{
			case Event::Type::Admitted: break;
			case Event::Type::Closed: m_closed = event.reason; break;
			case Event::Type::Snapshot:
				m_latest = BinaryArchive::Read<Query::Snapshot>(event.message.body, kFormat);
				break;
			case Event::Type::Message:
				if (event.message.kind == static_cast<Jettison::Net::MessageKind>(Kind::Welcome))
					m_welcome = BinaryArchive::Read<Welcome>(event.message.body, kFormat);
				else if (event.message.kind == static_cast<Jettison::Net::MessageKind>(Kind::Reply))
				{
					replies.push_back(BinaryArchive::Read<Reply>(event.message.body, kFormat));
					// The refused sequence was not consumed, nor any sent after it.
					if (m_welcome && replies.back().receipt.status == Command::Receipt::Status::Refused)
						m_welcome->nextSequence = replies.back().nextSequence;
				}
				break;
		}
	}
	return replies;
}


std::optional<std::uint64_t> Client::Submit(
    std::string_view command, std::uint32_t version, std::span<const std::byte> payload)
{
	if (!m_welcome || m_closed)
		return std::nullopt;
	const auto *pFirst = reinterpret_cast<const std::uint8_t *>(payload.data());
	const Remote::Submit submit{
	    m_welcome->nextSequence, std::string{command}, version, {pFirst, pFirst + payload.size()}};
	if (!m_net.Send(
	        Jettison::Net::Channel::Reliable, static_cast<Jettison::Net::MessageKind>(Kind::Submit),
	        BinaryArchive::Write(submit, kFormat)))
		return std::nullopt;
	return m_welcome->nextSequence++;
}


void Client::Disconnect()
{
	m_net.Disconnect();
}
} // namespace Manifest::Remote
