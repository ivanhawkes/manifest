#include <manifest/remote/Protocol.h>

#include <jettison/reflect/Reflect.h>

#include <manifest/company/Company.h>
#include <manifest/query/Query.h>
#include <manifest/remote/Keys.h>
#include <manifest/remote/Tokens.h>
#include <manifest/world/State.h>

namespace Manifest::Remote
{
void RegisterTypes()
{
	static const bool registered = []
	{
		World::RegisterTypes();
		Company::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		using Status = Command::Receipt::Status;
		RegisterEnum<Status>("Manifest.Command.ReceiptStatus")
		    .Value<Status::Queued>("Queued")
		    .Value<Status::Completed>("Completed")
		    .Value<Status::Refused>("Refused");
		RegisterType<Command::Receipt>("Manifest.Command.Receipt")
		    .Field<&Command::Receipt::status>("status", kSaved)
		    .Field<&Command::Receipt::result>("result", kSaved)
		    .Field<&Command::Receipt::order>("order", kSaved)
		    .Field<&Command::Receipt::step>("step", kSaved);
		RegisterType<Query::ShipView>("Manifest.Query.ShipView")
		    .Field<&Query::ShipView::shipId>("shipId", kSaved)
		    .Field<&Query::ShipView::displayName>("displayName", kSaved)
		    .Field<&Query::ShipView::hullId>("hullId", kSaved)
		    .Field<&Query::ShipView::dockedAt>("dockedAt", kSaved)
		    .Field<&Query::ShipView::cargo>("cargo", kSaved)
		    .Field<&Query::ShipView::journey>("journey", kSaved)
		    .Field<&Query::ShipView::opened>("opened", kSaved)
		    .Field<&Query::ShipView::lifeSupport>("lifeSupport", kSaved)
		    .Field<&Query::ShipView::hold>("hold", kSaved)
		    .Field<&Query::ShipView::systems>("systems", kSaved)
		    .Field<&Query::ShipView::policy>("policy", kSaved)
		    .Field<&Query::ShipView::job>("job", kSaved)
		    .Field<&Query::ShipView::fuel>("fuel", kSaved)
		    .Field<&Query::ShipView::fuelTanks>("fuelTanks", kSaved);
		RegisterType<Query::OrderView>("Manifest.Query.OrderView")
		    .Field<&Query::OrderView::orderId>("orderId", kSaved)
		    .Field<&Query::OrderView::order>("order", kSaved);
		RegisterType<Query::BidView>("Manifest.Query.BidView")
		    .Field<&Query::BidView::bidId>("bidId", kSaved)
		    .Field<&Query::BidView::bid>("bid", kSaved);
		RegisterType<Query::TenderView>("Manifest.Query.TenderView")
		    .Field<&Query::TenderView::tenderId>("tenderId", kSaved)
		    .Field<&Query::TenderView::tender>("tender", kSaved);
		RegisterType<Query::CompanyView>("Manifest.Query.CompanyView")
		    .Field<&Query::CompanyView::companyId>("companyId", kSaved)
		    .Field<&Query::CompanyView::displayName>("displayName", kSaved)
		    .Field<&Query::CompanyView::credits>("credits", kSaved)
		    .Field<&Query::CompanyView::holdings>("holdings", kSaved)
		    .Field<&Query::CompanyView::ships>("ships", kSaved)
		    .Field<&Query::CompanyView::orders>("orders", kSaved)
		    .Field<&Query::CompanyView::bids>("bids", kSaved)
		    .Field<&Query::CompanyView::licences>("licences", kSaved)
		    .Field<&Query::CompanyView::claims>("claims", kSaved);
		RegisterType<Query::MarketView>("Manifest.Query.MarketView")
		    .Field<&Query::MarketView::report>("report", kSaved)
		    .Field<&Query::MarketView::live>("live", kSaved)
		    .Field<&Query::MarketView::fees>("fees", kSaved);
		RegisterType<Query::ProjectView>("Manifest.Query.ProjectView")
		    .Field<&Query::ProjectView::project>("project", kSaved)
		    .Field<&Query::ProjectView::stock>("stock", kSaved);
		RegisterType<Query::RegionView>("Manifest.Query.RegionView")
		    .Field<&Query::RegionView::regionId>("regionId", kSaved)
		    .Field<&Query::RegionView::planetId>("planetId", kSaved)
		    .Field<&Query::RegionView::governorId>("governorId", kSaved)
		    .Field<&Query::RegionView::governorName>("governorName", kSaved)
		    .Field<&Query::RegionView::playerGoverned>("playerGoverned", kSaved)
		    .Field<&Query::RegionView::population>("population", kSaved)
		    .Field<&Query::RegionView::health>("health", kSaved)
		    .Field<&Query::RegionView::happiness>("happiness", kSaved)
		    .Field<&Query::RegionView::wageTaxBps>("wageTaxBps", kSaved)
		    .Field<&Query::RegionView::treasury>("treasury", kSaved)
		    .Field<&Query::RegionView::queue>("queue", kSaved)
		    .Field<&Query::RegionView::development>("development", kSaved);
		RegisterType<Query::Snapshot>("Manifest.Query.Snapshot")
		    .Field<&Query::Snapshot::step>("step", kSaved)
		    .Field<&Query::Snapshot::companies>("companies", kSaved)
		    .Field<&Query::Snapshot::markets>("markets", kSaved)
		    .Field<&Query::Snapshot::tenders>("tenders", kSaved)
		    .Field<&Query::Snapshot::regions>("regions", kSaved);
		RegisterType<Welcome>("Manifest.Remote.Welcome")
		    .Field<&Welcome::actorKind>("actorKind", kSaved)
		    .Field<&Welcome::actorId>("actorId", kSaved)
		    .Field<&Welcome::nextSequence>("nextSequence", kSaved)
		    .Field<&Welcome::content>("content", kSaved);
		RegisterType<Submit>("Manifest.Remote.Submit")
		    .Field<&Submit::sequence>("sequence", kSaved)
		    .Field<&Submit::command>("command", kSaved)
		    .Field<&Submit::version>("version", kSaved)
		    .Field<&Submit::payload>("payload", kSaved);
		RegisterType<Reply>("Manifest.Remote.Reply")
		    .Field<&Reply::sequence>("sequence", kSaved)
		    .Field<&Reply::receipt>("receipt", kSaved)
		    .Field<&Reply::nextSequence>("nextSequence", kSaved);
		RegisterType<Credential>("Manifest.Remote.Credential")
		    .Field<&Credential::digest>("digest", kSaved)
		    .Field<&Credential::kind>("kind", kSaved)
		    .Field<&Credential::id>("id", kSaved);
		RegisterType<TokenFile>("Manifest.Remote.TokenFile")
		    .Field<&TokenFile::tokens>("tokens", kSaved);
		RegisterType<PinnedServer>("Manifest.Remote.PinnedServer")
		    .Field<&PinnedServer::endpoint>("endpoint", kSaved)
		    .Field<&PinnedServer::key>("key", kSaved);
		RegisterType<ServerList>("Manifest.Remote.ServerList")
		    .Field<&ServerList::servers>("servers", kSaved);
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::Remote
