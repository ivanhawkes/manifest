#include <manifest/remote/Protocol.h>

#include <jettison/reflect/Reflect.h>

#include <manifest/company/Company.h>
#include <manifest/query/Query.h>
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
		    .Field<&Query::ShipView::hullId>("hullId", kSaved)
		    .Field<&Query::ShipView::dockedAt>("dockedAt", kSaved)
		    .Field<&Query::ShipView::cargo>("cargo", kSaved)
		    .Field<&Query::ShipView::journey>("journey", kSaved);
		RegisterType<Query::OrderView>("Manifest.Query.OrderView")
		    .Field<&Query::OrderView::orderId>("orderId", kSaved)
		    .Field<&Query::OrderView::order>("order", kSaved);
		RegisterType<Query::CompanyView>("Manifest.Query.CompanyView")
		    .Field<&Query::CompanyView::companyId>("companyId", kSaved)
		    .Field<&Query::CompanyView::displayName>("displayName", kSaved)
		    .Field<&Query::CompanyView::credits>("credits", kSaved)
		    .Field<&Query::CompanyView::holdings>("holdings", kSaved)
		    .Field<&Query::CompanyView::ships>("ships", kSaved)
		    .Field<&Query::CompanyView::orders>("orders", kSaved);
		RegisterType<Query::Snapshot>("Manifest.Query.Snapshot")
		    .Field<&Query::Snapshot::step>("step", kSaved)
		    .Field<&Query::Snapshot::companies>("companies", kSaved);
		RegisterType<Welcome>("Manifest.Remote.Welcome")
		    .Field<&Welcome::actorKind>("actorKind", kSaved)
		    .Field<&Welcome::actorId>("actorId", kSaved)
		    .Field<&Welcome::nextSequence>("nextSequence", kSaved);
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
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::Remote
