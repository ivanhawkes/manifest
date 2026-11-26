#include "TestHelpers.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/company/Company.h>
#include <manifest/sim/Simulation.h>
#include <manifest/trade/Economy.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/World.h>

using namespace Manifest::Trade;
using Jettison::Sim::SimWorld;
using Manifest::Content::ContentSet;
using Manifest::World::Kind;
namespace Content = Manifest::Content;
namespace World = Manifest::World;

namespace
{
constexpr std::string_view kOre = "VenueOreHighport";
constexpr std::string_view kBrine = "VenueBrineHighport";
constexpr std::string_view kOreHighlands = "RegionOreHighlands";
constexpr std::string_view kBrineCoast = "RegionBrineCoast";
constexpr std::string_view kMedical = "ItemBasicMedicalSupplies";


const ContentSet &Trading()
{
	static const ContentSet loaded = []
	{
		Jettison::Vfs::FileSystem files;
		files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
		Jettison::Sim::DefinitionRegistry definitions{files};
		return Content::LoadScenario(files, definitions, "trading");
	}();
	return loaded;
}


Content::ScenarioDefinition &ScenarioOf(ContentSet &content)
{
	return content.scenarios.at(content.scenario);
}


Content::MarketEntry &Entry(ContentSet &content, std::string_view venueId, std::string_view itemId)
{
	for (Content::VenueDefinition &venue : ScenarioOf(content).venues)
		if (venue.venueId == venueId)
			for (Content::MarketEntry &entry : venue.market)
				if (entry.itemId == itemId)
					return entry;
	FAIL("no such market entry");
	throw;
}


// One employer making one good, Brine's medical supplies, a tote every 600 s at the full rate for a
// wage bill of 164 credits (a quarter of the tote's 656.25 at base price, to the nearest), and no
// households: every other row's region cleared and both regions empty.
ContentSet OneBatch()
{
	ContentSet content = Trading();
	content.tuning.production.batchSeconds = 600.0;
	content.tuning.production.wageShare = 0.25;
	for (Content::RegionDefinition &region : ScenarioOf(content).regions)
		region.population = 0.0;
	for (Content::VenueDefinition &venue : ScenarioOf(content).venues)
		for (Content::MarketEntry &entry : venue.market)
			entry.regionId.clear();
	Content::MarketEntry &medical = Entry(content, kBrine, kMedical);
	medical.regionId = kBrineCoast;

	// 144 totes of 43.75 kg a day, wanted as much, so its price stays near its base, over its floor.
	medical.production = 6'300.0;
	medical.demand = 6'300.0;
	return content;
}


// The trading scenario as the host starts it, the brokers' first quotes made, on one-second steps.
struct Hosted
{
	explicit Hosted(ContentSet set = Trading())
	    : content{std::move(set)},
	      identities{
	          (Manifest::Company::RegisterTypes(), RegisterTypes(), World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, content);
		Manifest::Company::RegisterCommands(world, requests, identities, fleet, content);
		Register(world, requests, identities, content);
		World::CreateInitialState(world, identities, content);
		Manifest::Travel::OpenStations(world, fleet);
		Quote(world, identities, content);
	}


	void StepTo(std::uint64_t step)
	{
		while (world.Clock().Step() < step)
			world.Step();
	}


	template <typename T> const T &Get(Kind kind, std::string_view id) const
	{
		const entt::entity entity = identities.Find(kind, std::string{id});
		REQUIRE(entity != entt::null);
		return world.Registry().get<const T>(entity);
	}


	std::int64_t Balance(Kind kind, std::string_view id) const
	{
		return Get<World::Account>(kind, id).balance;
	}


	// The owner's totes of the item at the venue, available and reserved.
	std::uint64_t Totes(
	    Kind kind, std::string_view id, std::string_view venue, std::string_view item) const
	{
		for (const World::Holding &holding : Get<World::Stock>(kind, id).holdings)
			if (holding.venueId == venue && holding.itemId == item)
				return holding.available + holding.reserved;
		return 0;
	}


	double Opened(std::string_view region, std::string_view item) const
	{
		for (const World::Opened &opened : Get<World::Pantry>(Kind::Household, region).opened)
			if (opened.itemId == item)
				return opened.quantity;
		return 0.0;
	}

	ContentSet content;
	SimWorld world{1.0, 0};
	World::Identities &identities;
	Manifest::Command::Requests &requests;
};
} // namespace


TEST_CASE("The work rate falls with health and happiness, each from 1 to the floor")
{
	CHECK(WorkRate(100.0, 100.0, 0.5) == doctest::Approx(1.0));
	CHECK(WorkRate(50.0, 100.0, 0.5) == doctest::Approx(0.75));
	CHECK(WorkRate(100.0, 50.0, 0.5) == doctest::Approx(0.75));
	CHECK(WorkRate(50.0, 50.0, 0.5) == doctest::Approx(0.5625));
	CHECK(WorkRate(0.0, 0.0, 0.5) == doctest::Approx(0.25));
	for (double condition = 0.0; condition < 100.0; condition += 10.0)
	{
		CHECK(WorkRate(condition, 70.0, 0.5) < WorkRate(condition + 10.0, 70.0, 0.5));
		CHECK(WorkRate(70.0, condition, 0.5) < WorkRate(70.0, condition + 10.0, 0.5));
	}
}


TEST_CASE("Severity weighs each need's unmet share, capped, over the needs required")
{
	// Two needs, one met and one half met.
	const NeedMet half[]{{1.0, 10.0, 10.0}, {1.0, 4.0, 2.0}};
	CHECK(Severity(half) == doctest::Approx(0.25));

	// Food and water met, no medical supplies: a third; a surplus of food makes up for none of it.
	const NeedMet medical[]{{1.0, 8.68, 8.68}, {1.0, 52.08, 52.08}, {1.0, 0.087, 0.0}};
	CHECK(Severity(medical) == doctest::Approx(1.0 / 3.0));
	const NeedMet surplus[]{{1.0, 8.68, 100.0}, {1.0, 52.08, 52.08}, {1.0, 0.087, 0.0}};
	CHECK(Severity(surplus) == doctest::Approx(1.0 / 3.0));

	// A need with nothing required is left out, and with none required there is no shortage.
	const NeedMet unrequired[]{{1.0, 0.0, 0.0}, {1.0, 4.0, 4.0}};
	CHECK(Severity(unrequired) == 0.0);
	CHECK(Severity({}) == 0.0);

	// The weights, not the quantities, set importance.
	const NeedMet weighted[]{{3.0, 1.0, 0.0}, {1.0, 1'000.0, 1'000.0}};
	CHECK(Severity(weighted) == doctest::Approx(0.75));
}


TEST_CASE("An hour short of everything takes 100/100 to 94/88, and an hour of everything to 97/94")
{
	const Content::ConditionTuning &tuning = Trading().tuning.condition;
	Condition condition{100.0, 100.0};
	for (int interval = 0; interval < 60; ++interval)
		condition = Updated(condition, 1.0, 1.0 / 60.0, tuning);
	CHECK(condition.health == doctest::Approx(94.0));
	CHECK(condition.happiness == doctest::Approx(88.0));
	for (int interval = 0; interval < 60; ++interval)
		condition = Updated(condition, 0.0, 1.0 / 60.0, tuning);
	CHECK(condition.health == doctest::Approx(97.0));
	CHECK(condition.happiness == doctest::Approx(94.0));

	// A partial shortage loses in proportion, and both are held to [0, 100].
	CHECK(Updated({100.0, 100.0}, 0.25, 1.0, tuning).health == doctest::Approx(98.5));
	CHECK(Updated({100.0, 100.0}, 0.25, 1.0, tuning).happiness == doctest::Approx(97.0));
	CHECK(Updated({100.0, 100.0}, 0.0, 1.0, tuning).health == 100.0);
	CHECK(Updated({1.0, 1.0}, 1.0, 1.0, tuning).happiness == 0.0);
}


TEST_CASE("Withholding is a credit in ten of the gross paid, carried between payments")
{
	const Withholding hundred = Withhold(100, 0, 10);
	CHECK(hundred.households == 90);
	CHECK(hundred.treasury == 10);
	CHECK(hundred.carry == 0);

	// Ten one-credit payments withhold what one of ten does.
	std::int64_t carry = 0;
	std::int64_t households = 0;
	std::int64_t treasury = 0;
	for (int payment = 0; payment < 10; ++payment)
	{
		const Withholding paid = Withhold(1, carry, 10);
		CHECK(paid.households + paid.treasury == 1);
		households += paid.households;
		treasury += paid.treasury;
		carry = paid.carry;
	}
	CHECK(households == 9);
	CHECK(treasury == 1);
	CHECK(carry == 0);
	CHECK(Withhold(7, 0, 0).households == 7);
}


TEST_CASE("A batch's wages are earned with its work, rounded down, and all of them at its end")
{
	CHECK(Earned(100, 0.0, 600.0) == 0);
	CHECK(Earned(100, 243.0, 600.0) == 40);
	CHECK(Earned(100, 240.0, 600.0) == 40);
	CHECK(Earned(100, 599.0, 600.0) == 99);
	CHECK(Earned(100, 600.0, 600.0) == 100);
}


TEST_CASE("Work added many times at once is to the bit what adding it a step at a time makes")
{
	const auto stepped = [](double progress, double work, std::uint64_t times)
	{
		for (std::uint64_t step = 0; step < times; ++step)
			progress += work;
		return progress;
	};

	// Across binary exponents, from 0, with work that rounds away, and ties to even at 2^53.
	for (const auto &[progress, work, times] : std::vector<std::tuple<double, double, std::uint64_t>>{
	         {0.0, 0.1, 10'000},
	         {0.0, 0.9375, 5'000},
	         {599.5, 0.5625, 3'600},
	         {1.0, 1.0 / 3.0, 100'000},
	         {1e6, 1e-11, 1'000},
	         {1e6, 0x1p-34, 4'000},
	         {0x1p53, 1.0, 7},
	         {0x1p52 + 1.0, 0.5, 9},
	         {12.25, 0.0, 50},
	         {3.0, 0.7071067811865476, 777'777}})
	{
		CAPTURE(progress);
		CAPTURE(work);
		CHECK(Added(progress, work, times) == stepped(progress, work, times));
	}
}


TEST_CASE("A batch reserves its wages, pays them as it works, and sells its goods to the broker")
{
	Hosted hosted{OneBatch()};
	const std::int64_t households = hosted.Balance(Kind::HouseholdAccount, kBrineCoast);
	const std::int64_t treasury = hosted.Balance(Kind::PublicAccount, kBrineCoast);
	const std::uint64_t brokered = hosted.Totes(Kind::Broker, kBrine, kBrine, kMedical);

	hosted.world.Step();
	const auto &production = hosted.Get<World::Production>(Kind::Employer, kBrineCoast);
	REQUIRE(production.batches.size() == 1);
	CHECK(production.batches[0].totes == 1);
	CHECK(production.batches[0].duration == 600.0);
	CHECK(production.batches[0].wages == 164);
	CHECK(production.batches[0].progress == 1.0);
	CHECK(hosted.Balance(Kind::EmployerAccount, kBrineCoast) == 1'000'000 - 164);

	// Ore's employer makes nothing, and reserves nothing.
	CHECK(hosted.Get<World::Production>(Kind::Employer, kOreHighlands).batches.empty());
	CHECK(hosted.Balance(Kind::EmployerAccount, kOreHighlands) == 1'000'000);

	// The turn's last step pays what 60 s of 600 have earned, 16 credits: 1 withheld, 6 carried.
	hosted.StepTo(60);
	CHECK(production.batches[0].paid == 16);
	CHECK(production.taxCarry == 6);
	CHECK(hosted.Balance(Kind::HouseholdAccount, kBrineCoast) == households + 15);
	CHECK(hosted.Balance(Kind::PublicAccount, kBrineCoast) == treasury + 1);

	// At 600 s the tote is made and the rest is paid: 164 gross in all, 16 of it withheld and 4
	// carried, however the payments split. The tote is sold to the broker at its bid that turn.
	hosted.StepTo(599);
	REQUIRE(production.batches.size() == 1);
	hosted.world.Step();
	CHECK(production.batches.empty());
	CHECK(production.taxCarry == 4);
	CHECK(hosted.Balance(Kind::HouseholdAccount, kBrineCoast) == households + 148);
	CHECK(hosted.Balance(Kind::PublicAccount, kBrineCoast) == treasury + 16);
	CHECK(hosted.Totes(Kind::Employer, kBrineCoast, kBrine, kMedical) == 0);
	CHECK(hosted.Totes(Kind::Broker, kBrine, kBrine, kMedical) == brokered + 1);
	CHECK(hosted.Balance(Kind::EmployerAccount, kBrineCoast) > 1'000'000 - 164);

	// The next batch starts on the next step.
	hosted.world.Step();
	CHECK(production.batches.size() == 1);
}


TEST_CASE("An employer keeps what its broker's bids do not pay its wages for")
{
	ContentSet content = OneBatch();

	// Wanted nowhere, its native price is a fiftieth of its base, under its wages.
	Entry(content, kBrine, kMedical).demand = 0.0;
	Hosted hosted{std::move(content)};
	const std::uint64_t brokered = hosted.Totes(Kind::Broker, kBrine, kBrine, kMedical);
	hosted.StepTo(700);
	CHECK(hosted.Totes(Kind::Employer, kBrineCoast, kBrine, kMedical) == 1);
	CHECK(hosted.Totes(Kind::Broker, kBrine, kBrine, kMedical) == brokered);
	for (const auto [entity, order] : hosted.world.Registry().view<const World::Order>().each())
		CHECK(order.ownerKind != Kind::Employer);
}


TEST_CASE("At half the work rate a batch takes twice as long, for the same goods and wages")
{
	ContentSet content = OneBatch();

	// Health at 0 halves the rate; a recovery too small to move it keeps it there.
	for (Content::RegionDefinition &region : ScenarioOf(content).regions)
		region.health = 0.0;
	content.tuning.condition.healthRecoveryPerHour = 1e-300;
	Hosted hosted{std::move(content)};
	const std::int64_t households = hosted.Balance(Kind::HouseholdAccount, kBrineCoast);
	hosted.StepTo(1'199);
	const auto &production = hosted.Get<World::Production>(Kind::Employer, kBrineCoast);
	REQUIRE(production.batches.size() == 1);
	CHECK(production.batches[0].progress == doctest::Approx(599.5));
	hosted.world.Step();
	CHECK(production.batches.empty());
	CHECK(production.taxCarry == 4);
	CHECK(hosted.Balance(Kind::HouseholdAccount, kBrineCoast) == households + 148);
}


TEST_CASE("An employer that cannot fund a batch's wages starts none")
{
	ContentSet content = OneBatch();
	for (Content::RegionDefinition &region : ScenarioOf(content).regions)
		region.employerFunds = 163;
	Hosted hosted{std::move(content)};
	hosted.StepTo(120);
	CHECK(hosted.Get<World::Production>(Kind::Employer, kBrineCoast).batches.empty());
	CHECK(hosted.Balance(Kind::EmployerAccount, kBrineCoast) == 163);
}


TEST_CASE("A tote of drinking water is a credit at base price, to a part in a thousand (D-093)")
{
	const auto &items = Trading().gameMode.items;
	const auto water =
	    std::ranges::find(items, std::string{"ItemDrinkingWater"}, &Manifest::Core::ItemNode::itemId);
	REQUIRE(water != items.end());
	MANIFEST_CHECK_NEAR(
	    Manifest::Core::Simulation::BasePrice(*water).ToDouble() * water->unitsPerCubicMetre /
	        static_cast<double>(Manifest::Core::kTotesPerIbc),
	    1.0, 1e-3);
}


TEST_CASE("Households buy their essentials through the book and consume each interval's need")
{
	Hosted hosted;
	const std::int64_t funds = hosted.Balance(Kind::HouseholdAccount, kOreHighlands);
	hosted.StepTo(60);
	const auto &pantry = hosted.Get<World::Pantry>(Kind::Household, kOreHighlands);
	CHECK(pantry.severity == 0.0);
	CHECK(pantry.unmet.empty());
	CHECK(pantry.shortIntervals == 0);

	// A tote of medical supplies opened, 43.75 kg, less 25,000 people's minute: 125 kg a day.
	CHECK(hosted.Opened(kOreHighlands, kMedical) == doctest::Approx(43.75 - 125.0 / 1'440.0));

	// A tote of water opened, 0.09375 kL (an eighth of a packed cube), less 75 kL a day's minute.
	CHECK(
	    hosted.Opened(kOreHighlands, "ItemDrinkingWater") == doctest::Approx(0.09375 - 75.0 / 1'440.0));

	// The food is one need: one grain's tote, the cheapest, opened, and no other.
	double grain = 0.0;
	int grains = 0;
	for (const std::string_view item : {"ItemRice", "ItemWheat", "ItemOats", "ItemBarley"})
		if (const double opened = hosted.Opened(kOreHighlands, item); opened > 0.0)
		{
			grain += opened;
			++grains;
		}
	CHECK(grains == 1);
	CHECK(grain > 0.0);
	CHECK(hosted.Balance(Kind::HouseholdAccount, kOreHighlands) < funds);

	// Every essential met, the optional goods are bought too: Ore's households want its glassware.
	CHECK(hosted.Opened(kOreHighlands, "ItemGlassware") > 0.0);
	CHECK(hosted.Get<World::Region>(Kind::Region, kOreHighlands).health == 100.0);

	// No bid of theirs rests.
	for (const auto [entity, order] : hosted.world.Registry().view<const World::Order>().each())
		CHECK(order.ownerKind != Kind::Household);
}


TEST_CASE("An essential that cannot be bought is recorded, lowers condition and blocks optional goods")
{
	ContentSet content = Trading();
	Entry(content, kOre, kMedical).stock = 0;
	Hosted hosted{std::move(content)};
	hosted.StepTo(60);
	const auto &pantry = hosted.Get<World::Pantry>(Kind::Household, kOreHighlands);

	// One of the needs' 4.5 weights, the medical supplies' 1.
	CHECK(pantry.severity == doctest::Approx(1.0 / 4.5));
	CHECK(pantry.shortIntervals == 1);
	REQUIRE(pantry.unmet.size() == 1);
	CHECK(pantry.unmet[0].needId == "NeedBasicMedicalSupplies");
	CHECK(pantry.unmet[0].quantity == doctest::Approx(125.0 / 1'440.0));

	// All of it unmet, smoothed over an hour of minutes.
	CHECK(pantry.unmet[0].pressure == doctest::Approx(1.0 / 60.0));
	const auto &region = hosted.Get<World::Region>(Kind::Region, kOreHighlands);
	CHECK(region.health == doctest::Approx(100.0 - 6.0 / 4.5 / 60.0));
	CHECK(region.happiness == doctest::Approx(100.0 - 12.0 / 4.5 / 60.0));
	CHECK(hosted.Opened(kOreHighlands, "ItemGlassware") == 0.0);

	// Brine, supplied, is untouched by Ore's shortage.
	CHECK(hosted.Get<World::Region>(Kind::Region, kBrineCoast).health == 100.0);

	// A fresh interval wants a fresh minute's need, not the missed one again.
	hosted.StepTo(120);
	CHECK(pantry.shortIntervals == 2);
	CHECK(pantry.unmet[0].quantity == doctest::Approx(2.0 * 125.0 / 1'440.0));
	CHECK(pantry.unmet[0].pressure == doctest::Approx(1.0 / 60.0 + (1.0 - 1.0 / 60.0) / 60.0));
}


TEST_CASE("A need's pressure past a quarter calls a relief tender for the day's shortfall")
{
	ContentSet content = Trading();
	Entry(content, kOre, kMedical).stock = 0;
	Hosted hosted{std::move(content)};

	// Each short minute adds a sixtieth of the rest: 17 leave the pressure under a quarter, 18 past it.
	hosted.StepTo(std::uint64_t{60} * 17);
	CHECK(Reliefs(hosted.world, hosted.identities, hosted.content).empty());
	hosted.StepTo(std::uint64_t{60} * 18);
	const std::vector<Relief> reliefs = Reliefs(hosted.world, hosted.identities, hosted.content);
	REQUIRE(reliefs.size() == 1);
	const Relief &relief = reliefs[0];
	CHECK(relief.regionId == kOreHighlands);
	CHECK(relief.needId == "NeedBasicMedicalSupplies");
	CHECK(relief.venueId == kOre);
	CHECK(relief.itemId == kMedical);
	const auto &pantry = hosted.Get<World::Pantry>(Kind::Household, kOreHighlands);
	const auto medical = std::ranges::find(pantry.unmet, relief.needId, &World::Unmet::needId);
	REQUIRE(medical != pantry.unmet.end());
	const double pressure = medical->pressure;
	CHECK(pressure > kReliefPressure);

	// 125 kg a day, short by the pressure, is a tote of 43.75 kg, rounded up.
	CHECK(relief.totes == 1);

	// Nothing is offered at Ore, so the tote at its native price, three times the pressure over it.
	const auto price = TenderPrice(hosted.world, hosted.identities, hosted.content, kOre, kMedical);
	REQUIRE(price);
	CHECK(relief.budget == Budget(*price, 43.75, kReliefUrgency * pressure));
	CHECK(relief.budget > *Budget(*price, 43.75, 0.0));
}


TEST_CASE("A need's pressure raises the households' bid until they buy what the broker asks")
{
	ContentSet content = Trading();

	// A tote of medical supplies an interval, 43.75 kg a minute: 125 kg a day times 504.
	for (Content::NeedDefinition &need : content.tuning.needs)
		if (need.needId == "NeedBasicMedicalSupplies")
			need.perThousandPerDay *= 504.0;
	Hosted hosted{std::move(content)};
	const std::uint64_t seeded = hosted.Totes(Kind::Broker, kOre, kOre, kMedical);
	const auto &pantry = hosted.Get<World::Pantry>(Kind::Household, kOreHighlands);

	// At its target the broker asks the reference, and the households buy.
	hosted.StepTo(60);
	CHECK(hosted.Totes(Kind::Broker, kOre, kOre, kMedical) == seeded - 1);
	CHECK(pantry.unmet.empty());

	// Below it the broker asks more, and they go short until their pressure meets its ask.
	std::uint64_t step = 60;
	while (hosted.Totes(Kind::Broker, kOre, kOre, kMedical) == seeded - 1 && step < 6'000)
		hosted.StepTo(step += 60);

	// Two intervals short: the ask at 9 totes of 10 is about 9% over the reference.
	CHECK(step == 240);
	CHECK(pantry.shortIntervals == 2);
	const auto medical =
	    std::ranges::find(pantry.unmet, std::string{"NeedBasicMedicalSupplies"}, &World::Unmet::needId);
	REQUIRE(medical != pantry.unmet.end());
	CHECK(medical->pressure > 0.029);
}


TEST_CASE("A save part-way through a batch and a turn carries on as the world it came from")
{
	Hosted hosted{OneBatch()};
	hosted.StepTo(90);
	const std::vector<std::byte> save = hosted.world.Save();
	Hosted loaded{OneBatch()};
	loaded.world.Load(save);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	hosted.StepTo(700);
	loaded.StepTo(700);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());

	// A batch that has paid more than its wages is refused.
	hosted.world.Registry()
	    .get<World::Production>(hosted.identities.Find(Kind::Employer, std::string{kBrineCoast}))
	    .batches.at(0)
	    .paid = 165;
	CHECK_THROWS_WITH_AS(
	    loaded.world.Load(hosted.world.Save()), doctest::Contains("paid more than its wages"),
	    Jettison::Serialise::FormatError);
}
