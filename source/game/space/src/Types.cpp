#include <manifest/space/Types.h>

#include <jettison/ai/Types.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/transit/Types.h>
#include <manifest/space/Cruise.h>
#include <manifest/space/Gates.h>
#include <manifest/space/Lanes.h>

namespace Manifest::Space
{
void RegisterTypes()
{
	static const bool registered = []
	{
		using namespace Jettison::Reflect;
		Jettison::Ai::RegisterTypes();
		Jettison::Transit::RegisterTypes();
		RegisterEnum<Reason>("Space.Reason")
		    .Value<Reason::NoSuchEntity>("no_such_entity")
		    .Value<Reason::OnLane>("on_lane")
		    .Value<Reason::InNoSystem>("in_no_system")
		    .Value<Reason::NoGalaxy>("no_galaxy")
		    .Value<Reason::SystemNotInGalaxy>("system_not_in_galaxy")
		    .Value<Reason::NoLane>("no_lane")
		    .Value<Reason::ArrivalUndefined>("arrival_undefined")
		    .Value<Reason::NotController>("not_controller")
		    .Value<Reason::AlreadyCruising>("already_cruising")
		    .Value<Reason::CannotCruise>("cannot_cruise")
		    .Value<Reason::SystemUndefined>("system_undefined")
		    .Value<Reason::NoSuchBody>("no_such_body")
		    .Value<Reason::NearStation>("near_station")
		    .Value<Reason::AlreadyThere>("already_there")
		    .Value<Reason::NoCourse>("no_course")
		    .Value<Reason::NotAGate>("not_a_gate")
		    .Value<Reason::CannotTransit>("cannot_transit")
		    .Value<Reason::NotAtGate>("not_at_gate")
		    .Value<Reason::AlreadyCleared>("already_cleared")
		    .Value<Reason::AlreadyQueued>("already_queued")
		    .Value<Reason::LeadsNowhere>("leads_nowhere")
		    .Value<Reason::NotCleared>("not_cleared")
		    .Value<Reason::HullTooLarge>("hull_too_large")
		    .Value<Reason::InsideRadius>("inside_radius");
		constexpr Attribute kDefined = Attribute::Saved | Attribute::Moddable;
		RegisterType<OnLane>("Space.OnLane")
		    .Field<&OnLane::from>("from", Attribute::Saved)
		    .Field<&OnLane::departure>("departure", Attribute::Saved)
		    .Field<&OnLane::to>("to", Attribute::Saved)
		    .Field<&OnLane::arrival>("arrival", Attribute::Saved)
		    .Field<&OnLane::arrivalStep>("arrivalStep", Attribute::Saved);
		RegisterType<EnterLane>("Space.EnterLane")
		    .Field<&EnterLane::sender>("sender", Attribute::Saved)
		    .Field<&EnterLane::entity>("entity", Attribute::Saved)
		    .Field<&EnterLane::jumpPoint>("jumpPoint", Attribute::Saved);
		RegisterType<LaneEntered>("Space.LaneEntered", Attribute::Script)
		    .Field<&LaneEntered::entity>("entity", Attribute::Saved)
		    .Field<&LaneEntered::lane>("lane", Attribute::Saved)
		    .Field<&LaneEntered::transitSeconds>("transitSeconds", Attribute::Saved);
		RegisterType<LaneLeft>("Space.LaneLeft", Attribute::Script)
		    .Field<&LaneLeft::entity>("entity", Attribute::Saved)
		    .Field<&LaneLeft::system>("system", Attribute::Saved)
		    .Field<&LaneLeft::arrival>("arrival", Attribute::Saved);
		RegisterType<LaneRefused>("Space.LaneRefused", Attribute::Script)
		    .Field<&LaneRefused::entity>("entity", Attribute::Saved)
		    .Field<&LaneRefused::jumpPoint>("jumpPoint", Attribute::Saved)
		    .Field<&LaneRefused::reason>("reason", Attribute::Saved);

		RegisterType<Gate>("Space.Gate")
		    .Field<&Gate::jumpPoint>("jumpPoint", Attribute::Saved)
		    .Field<&Gate::holder>("cleared", Attribute::Saved)
		    .Field<&Gate::lapses>("lapses", Attribute::Saved)
		    .Field<&Gate::queue>("queue", Attribute::Saved);
		RegisterType<Transit>("Space.Transit")
		    .Field<&Transit::slot>("gate", Attribute::Saved)
		    .Field<&Transit::held>("cleared", Attribute::Saved);
		RegisterType<RequestTransit>("Space.RequestTransit")
		    .Field<&RequestTransit::sender>("sender", Attribute::Saved)
		    .Field<&RequestTransit::ship>("ship", Attribute::Saved)
		    .Field<&RequestTransit::gate>("gate", Attribute::Saved);
		RegisterType<TransitCleared>("Space.TransitCleared", Attribute::Script)
		    .Field<&TransitCleared::ship>("ship", Attribute::Saved)
		    .Field<&TransitCleared::gate>("gate", Attribute::Saved)
		    .Field<&TransitCleared::toll>("toll", Attribute::Saved)
		    .Field<&TransitCleared::lapses>("lapses", Attribute::Saved);
		RegisterType<TransitQueued>("Space.TransitQueued", Attribute::Script)
		    .Field<&TransitQueued::ship>("ship", Attribute::Saved)
		    .Field<&TransitQueued::gate>("gate", Attribute::Saved)
		    .Field<&TransitQueued::place>("place", Attribute::Saved);
		RegisterType<TransitRefused>("Space.TransitRefused", Attribute::Script)
		    .Field<&TransitRefused::ship>("ship", Attribute::Saved)
		    .Field<&TransitRefused::gate>("gate", Attribute::Saved)
		    .Field<&TransitRefused::reason>("reason", Attribute::Saved);
		RegisterType<TransitLapsed>("Space.TransitLapsed", Attribute::Script)
		    .Field<&TransitLapsed::ship>("ship", Attribute::Saved)
		    .Field<&TransitLapsed::gate>("gate", Attribute::Saved);
		RegisterType<TransitSent>("Space.TransitSent", Attribute::Script)
		    .Field<&TransitSent::ship>("ship", Attribute::Saved)
		    .Field<&TransitSent::gate>("gate", Attribute::Saved)
		    .Field<&TransitSent::toll>("toll", Attribute::Saved);
		RegisterType<GatePassed>("Space.GatePassed", Attribute::Script)
		    .Field<&GatePassed::ship>("ship", Attribute::Saved)
		    .Field<&GatePassed::gate>("gate", Attribute::Saved)
		    .Field<&GatePassed::reason>("reason", Attribute::Saved);

		RegisterType<Keepout>("Space.Keepout", Attribute::Moddable)
		    .Field<&Keepout::body>("body", kDefined)
		    .Field<&Keepout::radius>("radius", kDefined);
		RegisterType<CruiseDrive>("Space.CruiseDrive", Attribute::Moddable)
		    .Field<&CruiseDrive::seconds>("seconds", kDefined)
		    .Field<&CruiseDrive::reference>("reference", kDefined)
		    .Field<&CruiseDrive::exponent>("exponent", kDefined)
		    .Field<&CruiseDrive::profile>("profile", kDefined)
		    .Field<&CruiseDrive::departureClearance>("departureClearance", kDefined)
		    .Field<&CruiseDrive::arrivalClearance>("arrivalClearance", kDefined)
		    .Field<&CruiseDrive::crossingAcceleration>("crossingAcceleration", kDefined)
		    .Field<&CruiseDrive::keepouts>("keepouts", kDefined);
		RegisterType<Cruising>("Space.Cruising")
		    .Field<&Cruising::target>("target", Attribute::Saved)
		    .Field<&Cruising::frame>("frame", Attribute::Saved)
		    .Field<&Cruising::carrier>("carrier", Attribute::Saved)
		    .Field<&Cruising::course>("course", Attribute::Saved)
		    .Field<&Cruising::departureStep>("departureStep", Attribute::Saved)
		    .Field<&Cruising::arrivalStep>("arrivalStep", Attribute::Saved)
		    .Field<&Cruising::body>("body", Attribute::Saved)
		    .Field<&Cruising::profile>("profile", Attribute::Saved)
		    .Field<&Cruising::departureClearance>("departureClearance", Attribute::Saved)
		    .Field<&Cruising::arrivalClearance>("arrivalClearance", Attribute::Saved)
		    .Field<&Cruising::crossingAcceleration>("crossingAcceleration", Attribute::Saved)
		    .Field<&Cruising::departureBody>("departureBody", Attribute::Saved)
		    .Field<&Cruising::arrivalBody>("arrivalBody", Attribute::Saved);
		RegisterType<Cruise>("Space.Cruise")
		    .Field<&Cruise::sender>("sender", Attribute::Saved)
		    .Field<&Cruise::ship>("ship", Attribute::Saved)
		    .Field<&Cruise::body>("body", Attribute::Saved);
		RegisterType<CruiseStarted>("Space.CruiseStarted", Attribute::Script)
		    .Field<&CruiseStarted::ship>("ship", Attribute::Saved)
		    .Field<&CruiseStarted::body>("body", Attribute::Saved)
		    .Field<&CruiseStarted::seconds>("seconds", Attribute::Saved)
		    .Field<&CruiseStarted::metres>("metres", Attribute::Saved)
		    .Field<&CruiseStarted::arrivalStep>("arrivalStep", Attribute::Saved);
		RegisterType<CruiseEnded>("Space.CruiseEnded", Attribute::Script)
		    .Field<&CruiseEnded::ship>("ship", Attribute::Saved)
		    .Field<&CruiseEnded::body>("body", Attribute::Saved);
		RegisterType<CruiseRefused>("Space.CruiseRefused", Attribute::Script)
		    .Field<&CruiseRefused::ship>("ship", Attribute::Saved)
		    .Field<&CruiseRefused::body>("body", Attribute::Saved)
		    .Field<&CruiseRefused::reason>("reason", Attribute::Saved);
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::Space
