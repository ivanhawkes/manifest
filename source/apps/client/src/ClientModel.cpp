#include <manifest/client/ClientModel.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <numeric>
#include <span>

#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>

#include <manifest/company/Company.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/types/Decimal.h>
#include <manifest/world/State.h>

namespace Manifest::Client
{
namespace
{
using Command::ActorKind;
using Command::Receipt;
using Command::Result;

// Who provisions a player: the trusted local harness, as the console's host does.
const Command::Actor kHarness{ActorKind::Administrator, "Client"};

constexpr double kDaySeconds = 86400.0;
constexpr double kHourSeconds = 3600.0;
constexpr double kMinuteSeconds = 60.0;

// The longest a fast-forward asks the world for at once: a cruise between worlds takes days (D-090).
constexpr double kMostSeconds = 60 * kDaySeconds;

// The next journey's settings to begin with: the whole cruise thrust at 1 g, and their bounds; the
// world refuses an acceleration past what the ship's hull sustains.
constexpr std::int64_t kFullPercent = 100;
constexpr std::int64_t kTenthsPerG = 10;
constexpr std::int64_t kLeastTenths =
    Travel::kLeastAccelerationMilliG * kTenthsPerG / Travel::kMilliGPerG;
constexpr std::int64_t kMostTenths = 30;


template <typename Enum> std::string NameOf(Enum value)
{
	return std::string{Jettison::Reflect::EnumName(entt::meta_any{value})};
}


std::string Price(std::int64_t raw)
{
	return raw == 0 ? std::string{} : Core::Decimal::FromRaw(raw).ToString();
}


std::int64_t Whole(double value)
{
	return std::llround(value);
}


std::uint64_t Held(const Query::CompanyView &company, std::string_view venueId, std::string_view itemId)
{
	for (const World::Holding &holding : company.holdings)
		if (holding.venueId == venueId && holding.itemId == itemId)
			return holding.available;
	return 0;
}


std::uint64_t Aboard(const Query::ShipView &ship, std::string_view itemId)
{
	for (const World::Lot &lot : ship.cargo)
		if (lot.itemId == itemId)
			return lot.totes;
	return 0;
}


ShipRow MakeShip(const Query::ShipView &ship)
{
	ShipRow row;
	row.shipId = ship.shipId;
	row.hullId = ship.hullId;
	row.dockedAt = ship.dockedAt;
	if (ship.journey)
	{
		row.destination = ship.journey->venueId;
		row.leg = NameOf(ship.journey->leg);
		row.underWay = !ship.journey->Ended();
	}
	for (const World::Lot &lot : ship.cargo)
	{
		row.cargo.push_back({lot.itemId, lot.totes});
		row.aboard += lot.totes;
	}
	row.hold = ship.hold;
	row.reactionMass = Whole(ship.fuel.reactionMass);
	row.reactionMassMost = Whole(ship.fuelTanks.reactionMass);
	row.pellets = Whole(ship.fuel.pellets);
	row.pelletsMost = Whole(ship.fuelTanks.pellets);
	const World::LifeSupport &life = ship.lifeSupport;
	row.oxygen = Whole(std::accumulate(life.tanks.begin(), life.tanks.end(), 0.0));
	row.scrubbers = Whole(std::accumulate(life.cartridges.begin(), life.cartridges.end(), 0.0));
	row.connectedTo = life.connectedTo;
	return row;
}
} // namespace


void RegisterTypes()
{
	static const bool registered = []
	{
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		RegisterType<GoodRow>("Manifest.Client.GoodRow")
		    .Field<&GoodRow::itemId>("itemId", kSaved)
		    .Field<&GoodRow::bid>("bid", kSaved)
		    .Field<&GoodRow::bidRaw>("bidRaw", kSaved)
		    .Field<&GoodRow::bidTotes>("bidTotes", kSaved)
		    .Field<&GoodRow::ask>("ask", kSaved)
		    .Field<&GoodRow::askRaw>("askRaw", kSaved)
		    .Field<&GoodRow::askTotes>("askTotes", kSaved)
		    .Field<&GoodRow::held>("held", kSaved)
		    .Field<&GoodRow::aboard>("aboard", kSaved);
		RegisterType<LotRow>("Manifest.Client.LotRow")
		    .Field<&LotRow::itemId>("itemId", kSaved)
		    .Field<&LotRow::totes>("totes", kSaved);
		RegisterType<ShipRow>("Manifest.Client.ShipRow")
		    .Field<&ShipRow::shipId>("shipId", kSaved)
		    .Field<&ShipRow::hullId>("hullId", kSaved)
		    .Field<&ShipRow::dockedAt>("dockedAt", kSaved)
		    .Field<&ShipRow::destination>("destination", kSaved)
		    .Field<&ShipRow::leg>("leg", kSaved)
		    .Field<&ShipRow::underWay>("underWay", kSaved)
		    .Field<&ShipRow::cargo>("cargo", kSaved)
		    .Field<&ShipRow::aboard>("aboard", kSaved)
		    .Field<&ShipRow::hold>("hold", kSaved)
		    .Field<&ShipRow::reactionMass>("reactionMass", kSaved)
		    .Field<&ShipRow::reactionMassMost>("reactionMassMost", kSaved)
		    .Field<&ShipRow::pellets>("pellets", kSaved)
		    .Field<&ShipRow::pelletsMost>("pelletsMost", kSaved)
		    .Field<&ShipRow::oxygen>("oxygen", kSaved)
		    .Field<&ShipRow::scrubbers>("scrubbers", kSaved)
		    .Field<&ShipRow::connectedTo>("connectedTo", kSaved);
		RegisterType<HoldingRow>("Manifest.Client.HoldingRow")
		    .Field<&HoldingRow::venueId>("venueId", kSaved)
		    .Field<&HoldingRow::itemId>("itemId", kSaved)
		    .Field<&HoldingRow::available>("available", kSaved)
		    .Field<&HoldingRow::reserved>("reserved", kSaved);
		RegisterType<OrderRow>("Manifest.Client.OrderRow")
		    .Field<&OrderRow::orderId>("orderId", kSaved)
		    .Field<&OrderRow::venueId>("venueId", kSaved)
		    .Field<&OrderRow::itemId>("itemId", kSaved)
		    .Field<&OrderRow::side>("side", kSaved)
		    .Field<&OrderRow::price>("price", kSaved)
		    .Field<&OrderRow::remaining>("remaining", kSaved)
		    .Field<&OrderRow::totes>("totes", kSaved);
		RegisterType<Screen>("Manifest.Client.Screen")
		    .Field<&Screen::principal>("principal", kSaved)
		    .Field<&Screen::companyName>("companyName", kSaved)
		    .Field<&Screen::provisioned>("provisioned", kSaved)
		    .Field<&Screen::companyId>("companyId", kSaved)
		    .Field<&Screen::credits>("credits", kSaved)
		    .Field<&Screen::ships>("ships", kSaved)
		    .Field<&Screen::holdings>("holdings", kSaved)
		    .Field<&Screen::orders>("orders", kSaved)
		    .Field<&Screen::venueId>("venueId", kSaved)
		    .Field<&Screen::live>("live", kSaved)
		    .Field<&Screen::goods>("goods", kSaved)
		    .Field<&Screen::destinations>("destinations", kSaved)
		    .Field<&Screen::thrustPercent>("thrustPercent", kSaved)
		    .Field<&Screen::accelerationTenths>("accelerationTenths", kSaved)
		    .Field<&Screen::fastForward>("fastForward", kSaved)
		    .Field<&Screen::day>("day", kSaved)
		    .Field<&Screen::clock>("clock", kSaved)
		    .Field<&Screen::status>("status", kSaved);
		RegisterType<ClientState>("Manifest.Client.State")
		    .Field<&ClientState::principal>("principal", kSaved)
		    .Field<&ClientState::thrustPercent>("thrustPercent", kSaved)
		    .Field<&ClientState::accelerationTenths>("accelerationTenths", kSaved)
		    .Field<&ClientState::fastForward>("fastForward", kSaved);
		return true;
	}();
	(void)registered;
}


std::vector<std::byte> WriteState(const ClientState &state)
{
	RegisterTypes();
	return Jettison::Serialise::BinaryArchive::Write(
	    state, Jettison::Serialise::FormatVersion{kSaveVersion});
}


ClientState ReadState(std::span<const std::byte> bytes)
{
	RegisterTypes();
	return Jettison::Serialise::BinaryArchive::Read<ClientState>(
	    bytes, Jettison::Serialise::FormatVersion{kSaveVersion});
}


ClientModel::ClientModel(Jettison::Sim::SimWorld &world, const Host::Services &services)
    : m_world{world}, m_services{services}
{
	m_screen.thrustPercent = kFullPercent;
	m_screen.accelerationTenths = kTenthsPerG;
	MakeScreen();
}


void ClientModel::SetPrincipal(std::string principal)
{
	m_screen.principal = std::move(principal);
	Refresh();
}


ClientState ClientModel::State() const
{
	return {
	    m_screen.principal, m_screen.thrustPercent, m_screen.accelerationTenths, m_screen.fastForward};
}


void ClientModel::Restore(const ClientState &state)
{
	m_waiting.reset();
	m_last.reset();
	SetThrust(state.thrustPercent);
	SetAcceleration(state.accelerationTenths);
	m_screen.fastForward = state.fastForward;
	SetPrincipal(state.principal);
}


bool ClientModel::Refresh()
{
	if (m_waiting)
		if (const auto outcome = m_services.requests.Outcome(m_waiting->first, m_waiting->second))
		{
			m_last = outcome->result;
			m_waiting.reset();
		}
	m_snapshot = m_screen.principal.empty()
	                 ? Query::Snapshot{m_world.Clock().Step(), {}}
	                 : Query::Observe(
	                       m_world, m_services.identities, {ActorKind::Principal, m_screen.principal});
	const Screen before = m_screen;
	MakeScreen();
	return m_screen != before;
}


void ClientModel::MakeScreen()
{
	Screen &screen = m_screen;
	screen.provisioned = !m_snapshot.companies.empty();
	screen.companyId.clear();
	screen.credits = 0;
	screen.ships.clear();
	screen.holdings.clear();
	screen.orders.clear();
	screen.venueId.clear();
	screen.live = false;
	screen.goods.clear();
	screen.destinations.clear();

	const double seconds = static_cast<double>(m_snapshot.step) * m_world.FixedStep();
	screen.day = static_cast<std::uint64_t>(seconds / kDaySeconds) + 1;
	const double ofDay = std::fmod(seconds, kDaySeconds);
	screen.clock = std::format(
	    "{:02}:{:02}", static_cast<int>(ofDay / kHourSeconds),
	    static_cast<int>(std::fmod(ofDay, kHourSeconds) / kMinuteSeconds));
	if (!screen.provisioned)
		return;

	const Query::CompanyView &company = m_snapshot.companies.front();
	screen.companyId = company.companyId;
	screen.companyName = company.displayName;
	screen.credits = company.credits;
	for (const Query::ShipView &ship : company.ships)
		screen.ships.push_back(MakeShip(ship));
	for (const World::Holding &holding : company.holdings)
		screen.holdings.push_back(
		    {holding.venueId, holding.itemId, holding.available, holding.reserved});
	for (const Query::OrderView &order : company.orders)
		screen.orders.push_back(
		    {order.orderId, order.order.venueId, order.order.itemId, NameOf(order.order.side),
		     Price(order.order.limit), order.order.remaining, order.order.totes});

	const Query::ShipView *ship = Ship();
	const std::string docked = ship != nullptr ? ship->dockedAt : std::string{};
	for (const Query::MarketView &market : m_snapshot.markets)
	{
		if (market.report.venueId != docked)
		{
			screen.destinations.push_back(market.report.venueId);
			continue;
		}
		screen.venueId = docked;
		screen.live = market.live;
		for (const World::Line &line : market.report.lines)
		{
			GoodRow good;
			good.itemId = line.itemId;
			good.bidRaw = line.Bid();
			good.bid = Price(good.bidRaw);
			good.bidTotes = line.bids.empty() ? 0 : line.bids.front().totes;
			good.askRaw = line.Ask();
			good.ask = Price(good.askRaw);
			good.askTotes = line.asks.empty() ? 0 : line.asks.front().totes;
			good.held = Held(company, docked, line.itemId);
			good.aboard = Aboard(*ship, line.itemId);
			screen.goods.push_back(std::move(good));
		}
	}
}


const Query::ShipView *ClientModel::Ship() const
{
	if (m_snapshot.companies.empty() || m_snapshot.companies.front().ships.empty())
		return nullptr;
	return &m_snapshot.companies.front().ships.front();
}


Receipt ClientModel::Refuse(Result result)
{
	m_waiting.reset();
	m_last = result;
	return {Receipt::Status::Refused, result};
}


Receipt ClientModel::Admit(
    const Command::Actor &actor, std::string_view command, std::uint32_t version,
    std::span<const std::byte> payload)
{
	const std::uint64_t sequence = m_services.requests.NextSequence(actor);
	const Receipt receipt =
	    m_services.requests.Admit(m_world, actor, sequence, std::string{command}, version, payload);
	if (receipt.status == Receipt::Status::Refused)
		return Refuse(receipt.result);
	m_waiting.emplace(actor, sequence);
	m_last.reset();
	return receipt;
}


template <typename T>
Receipt ClientModel::Send(std::string_view command, std::uint32_t version, const T &payload)
{
	const std::vector<std::byte> bytes =
	    Jettison::Serialise::BinaryArchive::Write(payload, Jettison::Serialise::FormatVersion{version});
	return Admit({ActorKind::Principal, m_screen.principal}, command, version, bytes);
}


Receipt ClientModel::Provision(std::string principal, std::string companyName)
{
	if (m_screen.provisioned)
		return Refuse(Result::AlreadyProvisioned);
	const std::vector<std::byte> bytes = Jettison::Serialise::BinaryArchive::Write(
	    Company::Provision{principal, principal, companyName},
	    Jettison::Serialise::FormatVersion{Company::kVersion});
	const Receipt receipt = Admit(kHarness, Company::kProvision, Company::kVersion, bytes);
	if (receipt.status != Receipt::Status::Refused)
	{
		m_screen.principal = std::move(principal);
		m_screen.companyName = std::move(companyName);
	}
	return receipt;
}


Receipt ClientModel::Buy(std::string_view itemId)
{
	const auto good = std::ranges::find(m_screen.goods, itemId, &GoodRow::itemId);
	if (good == m_screen.goods.end())
		return Refuse(Result::NotDocked);
	return Send(
	    Trade::kPlace, Trade::kVersion,
	    Trade::Place{m_screen.venueId, good->itemId, World::Side::Buy, 1, good->askRaw});
}


Receipt ClientModel::Sell(std::string_view itemId)
{
	const auto good = std::ranges::find(m_screen.goods, itemId, &GoodRow::itemId);
	if (good == m_screen.goods.end())
		return Refuse(Result::NotDocked);
	return Send(
	    Trade::kPlace, Trade::kVersion,
	    Trade::Place{m_screen.venueId, good->itemId, World::Side::Sell, 1, good->bidRaw});
}


Receipt ClientModel::Cancel(std::string orderId)
{
	return Send(Trade::kCancel, Trade::kVersion, Trade::Cancel{std::move(orderId)});
}


Receipt ClientModel::Load(std::string_view itemId)
{
	const auto good = std::ranges::find(m_screen.goods, itemId, &GoodRow::itemId);
	if (good == m_screen.goods.end())
		return Refuse(Result::NotDocked);
	return Send(
	    Company::kLoad, Company::kVersion,
	    Company::Move{m_screen.ships.front().shipId, m_screen.venueId, good->itemId, good->held});
}


Receipt ClientModel::Unload(std::string_view itemId)
{
	const auto good = std::ranges::find(m_screen.goods, itemId, &GoodRow::itemId);
	if (good == m_screen.goods.end())
		return Refuse(Result::NotDocked);
	return Send(
	    Company::kUnload, Company::kVersion,
	    Company::Move{m_screen.ships.front().shipId, m_screen.venueId, good->itemId, good->aboard});
}


void ClientModel::SetThrust(std::int64_t percent)
{
	m_screen.thrustPercent = std::clamp<std::int64_t>(percent, 1, kFullPercent);
}


void ClientModel::SetAcceleration(std::int64_t tenths)
{
	m_screen.accelerationTenths = std::clamp(tenths, kLeastTenths, kMostTenths);
}


Receipt ClientModel::Travel(std::string venueId)
{
	if (m_screen.ships.empty())
		return Refuse(Result::UnknownTarget);
	namespace Journeys = Manifest::Travel;
	constexpr auto kBpsPerPercent = static_cast<std::int64_t>(Journeys::kFullThrustBps) / kFullPercent;
	constexpr auto kMilliGPerTenth = static_cast<std::int64_t>(Journeys::kMilliGPerG) / kTenthsPerG;
	return Send(
	    Journeys::kTravel, Journeys::kVersion,
	    Journeys::Travel{
	        m_screen.ships.front().shipId, std::move(venueId),
	        static_cast<std::uint32_t>(m_screen.thrustPercent * kBpsPerPercent),
	        static_cast<std::uint32_t>(m_screen.accelerationTenths * kMilliGPerTenth)});
}


Receipt ClientModel::Refill()
{
	if (m_screen.ships.empty())
		return Refuse(Result::UnknownTarget);
	return Send(Company::kRefill, Company::kVersion, Company::Service{m_screen.ships.front().shipId});
}


Receipt ClientModel::Replace()
{
	if (m_screen.ships.empty())
		return Refuse(Result::UnknownTarget);
	return Send(Company::kReplace, Company::kVersion, Company::Service{m_screen.ships.front().shipId});
}


Receipt ClientModel::SetRefuel(bool enabled)
{
	if (m_screen.ships.empty())
		return Refuse(Result::UnknownTarget);
	return Send(
	    Company::kSetRefuel, Company::kVersion,
	    Company::SetRefuel{m_screen.ships.front().shipId, enabled});
}


void ClientModel::SetFastForward(bool enabled)
{
	m_screen.fastForward = enabled;
}


bool ClientModel::UnderWay() const
{
	if (m_screen.companyId.empty())
		return false;
	const entt::registry &registry = m_world.Registry();
	for (const auto &[entity, ship, journey] :
	     registry.view<const World::Ship, const World::Journey>().each())
		if (ship.companyId == m_screen.companyId && !journey.Ended())
			return true;
	return false;
}


std::uint64_t ClientModel::FastForward(std::chrono::nanoseconds budget)
{
	if (!m_screen.fastForward)
		return 0;
	const auto until = std::chrono::steady_clock::now() + budget;
	const auto most = static_cast<std::uint64_t>(kMostSeconds / m_world.FixedStep());
	std::uint64_t taken = 0;
	while (UnderWay() && std::chrono::steady_clock::now() < until)
		taken += m_world.FastForward(m_world.Clock().Step() + most);
	return taken;
}
} // namespace Manifest::Client
