#include "TestHelpers.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/SimWorld.h>

#include <manifest/client/ClientModel.h>
#include <manifest/host/Start.h>

using Manifest::Client::ClientModel;
using Manifest::Command::Receipt;
using Manifest::Command::Result;

namespace
{
// The staged content with the Manifest mode's cruise taken out, so the ship cruises by the power law
// (D-088): Ore to Brine in minutes of simulation rather than days, as the console's slice does.
class CompressedContent
{
  public:
	CompressedContent()
	{
		std::filesystem::remove_all(m_directory);
		std::filesystem::copy(
		    MANIFEST_MOD_DIRECTORY, m_directory, std::filesystem::copy_options::recursive);
		const std::filesystem::path mode =
		    m_directory / "manifest" / "definitions" / "game-modes" / "manifest.yaml";
		std::ifstream in{mode};
		std::string kept;
		bool cruise = false;
		for (std::string line; std::getline(in, line);)
		{
			cruise = line.starts_with("cruise:") || (cruise && line.starts_with(' '));
			if (!cruise)
				kept += line + "\n";
		}
		in.close();
		std::ofstream{mode} << kept;
	}


	~CompressedContent()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_directory, ignored);
	}


	CompressedContent(const CompressedContent &) = delete;


	CompressedContent &operator=(const CompressedContent &) = delete;


	std::filesystem::path Path() const
	{
		return m_directory;
	}

  private:
	Manifest::Test::TempFile m_marker{""};
	std::filesystem::path m_directory{m_marker.Path() + "-content"};
};


// The request just sent run by the next step, and its answer.
Result Answer(ClientModel &model, Jettison::Sim::SimWorld &world, const Receipt &receipt)
{
	REQUIRE(receipt.status == Receipt::Status::Queued);
	CHECK(model.Waiting());
	world.Step();
	model.Refresh();
	REQUIRE_FALSE(model.Waiting());
	REQUIRE(model.LastResult());
	return *model.LastResult();
}
} // namespace


TEST_CASE("The client's model plays the first loop: furs bought at Ore, flown to Brine and sold there")
{
	const CompressedContent content;
	const std::unique_ptr<Jettison::Host::Session> session =
	    Manifest::Host::Start({content.Path(), {}, "trading"});
	Jettison::Sim::SimWorld &world = session->World();
	ClientModel model{world, Manifest::Host::HostedServices(*session)};
	CHECK_FALSE(model.View().provisioned);
	CHECK(model.View().day == 1);
	CHECK(model.View().minutes == 0);

	// The start screen's provisioning names the player, and a second is refused before it is sent.
	CHECK(Answer(model, world, model.Provision("Ada", "Ada Co")) == Result::Succeeded);
	CHECK(model.Principal() == "Ada");
	CHECK(model.Provision("Ada", "Ada Co").status == Receipt::Status::Refused);
	CHECK(model.LastResult() == Result::AlreadyProvisioned);
	const Manifest::Client::Screen &view = model.View();
	REQUIRE(view.provisioned);
	CHECK(view.companyName == "Ada Co");
	REQUIRE(view.ships.size() == 1);
	CHECK(view.ships.front().dockedAt == "VenueOreHighport");
	CHECK(view.venueId == "VenueOreHighport");
	CHECK(view.live);
	const auto knows = [&view](std::string_view venue)
	{ return std::ranges::find(view.destinations, venue) != view.destinations.end(); };
	CHECK(knows("VenueBrineHighport"));
	CHECK_FALSE(knows("VenueOreHighport"));
	const std::int64_t starting = view.credits;

	// Two totes bought at the asks the market screen shows, then loaded.
	const auto furs = [&model]
	{
		const auto &goods = model.View().goods;
		const auto good = std::ranges::find(goods, "ItemFurs", &Manifest::Client::GoodRow::itemId);
		REQUIRE(good != goods.end());
		return *good;
	};
	CHECK(furs().ask > 0.0);
	const std::int64_t ask = furs().askRaw;
	CHECK(Answer(model, world, model.Buy("ItemFurs")) == Result::Succeeded);
	CHECK(Answer(model, world, model.Buy("ItemFurs")) == Result::Succeeded);
	CHECK(furs().held == 2);
	CHECK(view.credits < starting - 2 * (ask / 1'000'000));
	CHECK(Answer(model, world, model.Load("ItemFurs")) == Result::Succeeded);
	CHECK(furs().held == 0);
	CHECK(furs().aboard == 2);
	CHECK(view.ships.front().aboard == 2);
	const std::int64_t bought = view.credits;

	// The journey's settings are kept within their bounds; with fast-forward off nothing is passed.
	model.SetThrust(0);
	CHECK(view.thrustPercent == 1);
	model.SetThrust(100);
	model.SetAcceleration(0);
	CHECK(view.accelerationTenths == 1);
	model.SetAcceleration(10);
	CHECK(Answer(model, world, model.Travel("VenueBrineHighport")) == Result::Succeeded);
	CHECK(model.UnderWay());
	CHECK(view.ships.front().underWay);
	CHECK(view.ships.front().destination == "VenueBrineHighport");
	model.SetFastForward(false);
	CHECK(model.FastForward(std::chrono::seconds{1}) == 0);

	// Fast-forward passes the journey's steps a budget at a time, to its end.
	model.SetFastForward(true);
	std::uint64_t passed = 0;
	for (int frame = 0; frame < 10'000 && model.UnderWay(); ++frame)
		passed += model.FastForward(std::chrono::milliseconds{50});
	CHECK_FALSE(model.UnderWay());
	CHECK(passed > 0);
	model.Refresh();
	CHECK(view.ships.front().dockedAt == "VenueBrineHighport");
	CHECK(view.ships.front().leg == "Arrived");
	CHECK(view.venueId == "VenueBrineHighport");
	CHECK(view.live);
	CHECK(knows("VenueOreHighport"));
	CHECK_FALSE(knows("VenueBrineHighport"));
	CHECK(model.FastForward(std::chrono::seconds{1}) == 0);

	// Unloaded and sold a tote at a time at the bid, for more than they cost.
	CHECK(Answer(model, world, model.Unload("ItemFurs")) == Result::Succeeded);
	CHECK(furs().held == 2);
	CHECK(Answer(model, world, model.Sell("ItemFurs")) == Result::Succeeded);
	CHECK(Answer(model, world, model.Sell("ItemFurs")) == Result::Succeeded);
	CHECK(furs().held == 0);
	CHECK(view.credits > bought);

	// An order that is not there is answered as the world answers it; the service is answered too.
	CHECK(Answer(model, world, model.Cancel("Order999999")) == Result::UnknownTarget);
	CHECK(view.orders.empty());
	CHECK(Answer(model, world, model.SetRefuel(true)) == Result::Succeeded);
	(void)Answer(model, world, model.Refill());
	(void)Answer(model, world, model.Replace());
	CHECK(view.ships.front().oxygen > 0);
	CHECK(view.ships.front().reactionMassMost >= view.ships.front().reactionMass);

	// A save's section restores the player and the journey's settings into a model of the same world.
	model.SetThrust(40);
	model.SetAcceleration(5);
	model.SetFastForward(false);
	const Manifest::Client::ClientState state = model.State();
	CHECK(Manifest::Client::ReadState(Manifest::Client::WriteState(state)) == state);
	ClientModel restored{world, Manifest::Host::HostedServices(*session)};
	restored.Restore(state);
	CHECK(restored.View() == model.View());
	CHECK_THROWS_AS(
	    (void)Manifest::Client::ReadState(std::vector<std::byte>{std::byte{1}}),
	    Jettison::Serialise::FormatError);
}


TEST_CASE("The client's model refuses an action with nothing to act on before it is sent")
{
	const std::unique_ptr<Jettison::Host::Session> session =
	    Manifest::Host::Start({MANIFEST_MOD_DIRECTORY, {}, "trading"});
	ClientModel model{session->World(), Manifest::Host::HostedServices(*session)};
	CHECK(model.Buy("ItemFurs").status == Receipt::Status::Refused);
	CHECK(model.LastResult() == Result::NotDocked);
	CHECK(model.Travel("VenueBrineHighport").status == Receipt::Status::Refused);
	CHECK(model.LastResult() == Result::UnknownTarget);
	CHECK_FALSE(model.UnderWay());

	// A name the world does not have sends nothing it admits.
	model.SetPrincipal("Nobody");
	CHECK_FALSE(model.View().provisioned);
	CHECK(model.Cancel("Order1").status == Receipt::Status::Refused);
	CHECK(model.LastResult() == Result::NotAuthorised);
}
