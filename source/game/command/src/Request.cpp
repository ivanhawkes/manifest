#include <manifest/command/Request.h>

#include <jettison/reflect/Reflect.h>

#include <manifest/world/State.h>

namespace Manifest::Command
{
void RegisterTypes()
{
	static const bool registered = []
	{
		World::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;

		RegisterEnum<Result>("Manifest.Command.Result")
		    .Value<Result::Succeeded>("Succeeded")
		    .Value<Result::NotAuthorised>("NotAuthorised")
		    .Value<Result::UnsupportedCommand>("UnsupportedCommand")
		    .Value<Result::MalformedPayload>("MalformedPayload")
		    .Value<Result::PayloadTooLarge>("PayloadTooLarge")
		    .Value<Result::QueueFull>("QueueFull")
		    .Value<Result::SequenceGap>("SequenceGap")
		    .Value<Result::SequenceExhausted>("SequenceExhausted")
		    .Value<Result::RequestConflict>("RequestConflict")
		    .Value<Result::RequestTooOld>("RequestTooOld")
		    .Value<Result::UnknownTarget>("UnknownTarget")
		    .Value<Result::InsufficientFunds>("InsufficientFunds")
		    .Value<Result::AlreadyProvisioned>("AlreadyProvisioned")
		    .Value<Result::NotDocked>("NotDocked")
		    .Value<Result::WrongVenue>("WrongVenue")
		    .Value<Result::InsufficientStock>("InsufficientStock")
		    .Value<Result::CapacityExceeded>("CapacityExceeded")
		    .Value<Result::InvalidQuantity>("InvalidQuantity")
		    .Value<Result::InvalidPrice>("InvalidPrice")
		    .Value<Result::BookFull>("BookFull")
		    .Value<Result::NoPadFree>("NoPadFree")
		    .Value<Result::Travelling>("Travelling")
		    .Value<Result::AlreadyThere>("AlreadyThere")
		    .Value<Result::NoServiceFree>("NoServiceFree")
		    .Value<Result::AlreadyFitted>("AlreadyFitted")
		    .Value<Result::NotFittable>("NotFittable");
		RegisterEnum<ActorKind>("Manifest.Command.ActorKind")
		    .Value<ActorKind::Principal>("Principal")
		    .Value<ActorKind::Administrator>("Administrator");
		RegisterType<Request>("Manifest.Command.Request")
		    .Field<&Request::actorKind>("actorKind", kSaved)
		    .Field<&Request::actorId>("actorId", kSaved)
		    .Field<&Request::sequence>("sequence", kSaved)
		    .Field<&Request::order>("order", kSaved)
		    .Field<&Request::command>("command", kSaved)
		    .Field<&Request::version>("version", kSaved)
		    .Field<&Request::payload>("payload", kSaved);
		RegisterType<Completed>("Manifest.Command.Completed")
		    .Field<&Completed::request>("request", kSaved)
		    .Field<&Completed::step>("step", kSaved)
		    .Field<&Completed::result>("result", kSaved);
		RegisterType<ActorLedger>("Manifest.Command.ActorLedger")
		    .Field<&ActorLedger::kind>("kind", kSaved)
		    .Field<&ActorLedger::id>("id", kSaved)
		    .Field<&ActorLedger::next>("next", kSaved)
		    .Field<&ActorLedger::floor>("floor", kSaved)
		    .Field<&ActorLedger::completed>("completed", kSaved)
		    .Field<&ActorLedger::pending>("pending", kSaved);
		RegisterType<RequestLedger>("Manifest.Command.RequestLedger")
		    .Field<&RequestLedger::order>("order", kSaved)
		    .Field<&RequestLedger::actors>("actors", kSaved);
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::Command
