#include <manifest/space/Cruise.h>

#include <jettison/ai/Steering.h>
#include <jettison/physics/Flight.h>
#include <jettison/physics/PhysicsWorld.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/transit/Passage.h>
#include <jettison/transit/Path.h>
#include <manifest/space/Lanes.h>
#include <manifest/space/Routes.h>
#include <manifest/space/Types.h>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>


namespace Manifest::Space
{
using Jettison::Ai::Course;
using Jettison::Ai::Destination;
using Jettison::Ai::Sphere;
using Jettison::Ai::Steered;
using Jettison::Ai::Waypoints;
using Jettison::Transit::Facing;

namespace
{
// The body and every body it orbits, out to the primary.
std::vector<std::size_t> Chain(const Space::StarSystem &system, int body)
{
	std::vector<std::size_t> chain;
	for (int at = body; at >= 0; at = system.bodies[static_cast<std::size_t>(at)].parentIndex)
		chain.push_back(static_cast<std::size_t>(at));
	return chain;
}


bool Contains(const std::vector<std::size_t> &chain, std::size_t body)
{
	return std::ranges::find(chain, body) != chain.end();
}

// What a course is planned and flown against: the system, the carrier and the two ends' chains.
struct Planner
{
	const Space::StarSystem &system;
	int carrier{-1};
	std::vector<std::size_t> departure;
	std::vector<std::size_t> arrival;

	// Bodies a course flown was found too near (SettleCruise), at their places in its frame then.
	std::vector<Sphere> extra;


	glm::dvec3 Where(std::size_t body, double time) const
	{
		return Space::EvaluateBody(system, body, time).metres;
	}


	glm::dvec3 CarrierAt(double time) const
	{
		return carrier < 0 ? glm::dvec3{0.0} : Where(static_cast<std::size_t>(carrier), time);
	}


	glm::dvec3 Relative(std::size_t body, double time) const
	{
		return Where(body, time) - CarrierAt(time);
	}


	// The course from the start to the target's standoff on the side of `towards`, both relative to
	// the carrier, for a cruise from t0 to t1.
	std::optional<Course> Plot(
	    const glm::dvec3 &start, const glm::dvec3 &towards, std::size_t target, double t0,
	    double t1) const
	{
		const glm::dvec3 at = Relative(target, t1);
		glm::dvec3 away = towards - at;
		if (glm::length(away) == 0.0)
			away = start - at;
		if (glm::length(away) == 0.0)
			away = glm::dvec3{0.0, 0.0, 1.0};
		const glm::dvec3 end =
		    at + glm::normalize(away) * CruiseTravel::ArrivalDistance(system.bodies[target]);

		const double middle = t0 + ((t1 - t0) / 2.0);
		std::vector<Sphere> obstacles;
		for (std::size_t body = 0; body < system.bodies.size(); ++body)
		{
			const Space::BodyKind kind = system.bodies[body].kind;
			if (kind != Space::BodyKind::Star && kind != Space::BodyKind::Planet &&
			    kind != Space::BodyKind::Moon)
				continue;
			const bool left = Contains(departure, body);
			const bool reached = Contains(arrival, body);
			const double radius = system.bodies[body].radius;
			if (left && !reached)
				obstacles.push_back({Relative(body, t0), radius});
			else if (reached && !left)
				obstacles.push_back({Relative(body, t1), radius});
			else
				obstacles.push_back(
				    {Relative(body, middle),
				     radius + (glm::length(Relative(body, t1) - Relative(body, t0)) / 2.0)});
		}
		obstacles.insert(obstacles.end(), extra.begin(), extra.end());
		return Waypoints{std::move(obstacles), CruiseTravel::kCourseClearance}.PlotCourse(start, end);
	}
};


static_assert(Jettison::Physics::Handling{}.reverse == CruiseTravel::kCrossingAcceleration);


// A course's crossings at its ends of a drive's clearances: the thrusters from rest to rest over at
// most half the course each. None where the drive crosses nothing.
struct Crossing
{
	Jettison::Transit::MotionProfile thrusters;
	double departure{0.0};
	double arrival{0.0};
};


std::optional<Crossing> CrossingOf(double departure, double arrival, float acceleration, double metres)
{
	if (!(departure > 0.0 || arrival > 0.0) || !(acceleration > 0.0f) || !(metres > 0.0))
		return std::nullopt;
	return Crossing{
	    {acceleration, 0.0f, acceleration, {}}, std::min(std::max(departure, 0.0), metres / 2.0),
	    std::min(std::max(arrival, 0.0), metres / 2.0)};
}


std::optional<Crossing> CrossingOf(const CruiseDrive &drive, double metres)
{
	return drive.profile ? CrossingOf(
	                           drive.departureClearance, drive.arrivalClearance,
	                           drive.crossingAcceleration, metres)
	                     : std::nullopt;
}


double CrossedSeconds(
    const Jettison::Transit::MotionProfile &profile, const std::optional<Crossing> &crossing,
    double metres)
{
	if (!crossing)
		return Jettison::Transit::Seconds(profile, metres);
	return (Jettison::Transit::Seconds(crossing->thrusters, crossing->departure) +
	        Jettison::Transit::Seconds(crossing->thrusters, crossing->arrival)) +
	       Jettison::Transit::Seconds(profile, metres - (crossing->departure + crossing->arrival));
}


// Where a crossing cruise is that many of its profile's seconds after setting out: the metres covered,
// across the first crossing, along the drive's way, then across the last; and whether its drive
// thrusts there, and whether it is crossing.
struct Progress
{
	double covered{0.0};
	bool driving{false};
	bool thrusting{false};
};


Progress CrossedProgress(
    const Jettison::Transit::MotionProfile &profile, const Crossing &crossing, double metres,
    double seconds)
{
	const double across = Jettison::Transit::Seconds(crossing.thrusters, crossing.departure);
	const double driven = metres - (crossing.departure + crossing.arrival);
	const double driving = Jettison::Transit::Seconds(profile, driven);
	if (seconds < across)
		return {Jettison::Transit::Covered(crossing.thrusters, crossing.departure, seconds)};
	if (seconds < across + driving)
	{
		// The drive thrusts at each end of its way, boost first, and coasts between.
		const Jettison::Transit::ThrustSeconds thrust = Jettison::Transit::Thrust(profile, driven);
		const double each = thrust.boost + thrust.sustained;
		const double into = seconds - across;
		return {
		    crossing.departure + Jettison::Transit::Covered(profile, driven, into), true,
		    into < each || into > driving - each};
	}
	return {
	    crossing.departure + driven +
	    Jettison::Transit::Covered(crossing.thrusters, crossing.arrival, seconds - across - driving)};
}


// The body a crossing at an end is flown relative to: the body's own, or for a station, or a jump point
// keeping station by one, the body it orbits.
int Anchor(const Space::StarSystem &system, int body)
{
	while (body >= 0)
	{
		const Space::Body &each = system.bodies[static_cast<std::size_t>(body)];
		if ((each.kind != Space::BodyKind::Station && each.kind != Space::BodyKind::JumpPoint) ||
		    each.parentIndex < 0)
			break;
		body = each.parentIndex;
	}
	return body;
}


// A crossing cruise as it is flown: its crossings, its length, its carrier, the bodies its crossings
// are flown relative to, and where each was relative to the carrier as the cruise set out and as it
// arrives.
struct Flight
{
	Crossing crossing;
	double length{0.0};
	int carrier{-1};
	int departure{-1};
	int arrival{-1};
	glm::dvec3 departed{0.0};
	glm::dvec3 arrives{0.0};


	static Flight Of(
	    const Space::StarSystem &system, const Crossing &crossing, double length, int carrier,
	    int departure, int arrival, double t0, double t1)
	{
		Flight flight{crossing, length, carrier, departure, arrival, glm::dvec3{0.0}, glm::dvec3{0.0}};
		const auto relative = [&](int body, double when)
		{
			if (body < 0)
				return glm::dvec3{0.0};
			const std::vector<Space::SystemPosition> bodies = Space::EvaluateSystem(system, when);
			return bodies[static_cast<std::size_t>(body)].metres -
			       (carrier < 0 ? glm::dvec3{0.0} : bodies[static_cast<std::size_t>(carrier)].metres);
		};
		flight.departed = relative(departure, t0);
		flight.arrives = relative(arrival, t1);
		return flight;
	}


	// What the course's frame is moved by where the bodies are now, that many metres along: the
	// departure body's motion relative to the carrier since setting out, by the share of the drive's
	// metres still to fly, and the arrival body's still to come before arriving, by the share flown; at
	// each crossing its own end's alone, so the frames join without a jump.
	glm::dvec3 Offset(const std::vector<Space::SystemPosition> &now, double covered) const
	{
		const glm::dvec3 origin =
		    carrier < 0 ? glm::dvec3{0.0} : now[static_cast<std::size_t>(carrier)].metres;
		const double driven = length - (crossing.departure + crossing.arrival);
		const double flown = driven > 0.0
		                         ? std::clamp((covered - crossing.departure) / driven, 0.0, 1.0)
		                         : (covered < length / 2.0 ? 0.0 : 1.0);
		glm::dvec3 offset{0.0};
		if (departure >= 0 && departure != carrier && flown < 1.0)
			offset += (1.0 - flown) *
			          ((now[static_cast<std::size_t>(departure)].metres - origin) - departed);
		if (arrival >= 0 && arrival != carrier && flown > 0.0)
			offset += flown * ((now[static_cast<std::size_t>(arrival)].metres - origin) - arrives);
		return offset;
	}
};


// Where a cruising ship is as the step ending at that count ends, at that time, as Transit::Underway
// places it, its crossings included and flown in their bodies' frames.
Jettison::Sim::Transform Underway(
    const Cruising &cruising, const Space::StarSystem &system, int carrier, std::uint64_t ending,
    double endsAt, double fixedStep)
{
	const std::vector<Space::SystemPosition> bodies = Space::EvaluateSystem(system, endsAt);
	const glm::dvec3 origin =
	    carrier < 0 ? glm::dvec3{0.0} : bodies[static_cast<std::size_t>(carrier)].metres;
	const double length = Jettison::Transit::Length(cruising.course);
	const std::optional<Crossing> crossing = CrossingOf(
	    cruising.departureClearance, cruising.arrivalClearance, cruising.crossingAcceleration, length);
	if (!cruising.profile || !crossing)
		return Jettison::Transit::Underway(
		    cruising.course, cruising.departureStep, cruising.arrivalStep, cruising.profile, origin,
		    ending);

	// Slowed to the whole steps it was given, as Transit::Underway's profile is.
	const double share = static_cast<double>(ending - cruising.departureStep) /
	                     static_cast<double>(cruising.arrivalStep - cruising.departureStep);
	const double seconds = CrossedSeconds(*cruising.profile, crossing, length);
	const double covered =
	    CrossedProgress(*cruising.profile, *crossing, length, share * seconds).covered;
	const auto [at, leg] = Jettison::Transit::Along(cruising.course, covered / length);
	if (cruising.departureBody.empty())
		return {origin + at, Facing(leg)};
	const Flight flight = Flight::Of(
	    system, *crossing, length, carrier, system.Find(cruising.departureBody),
	    Anchor(system, system.Find(cruising.target)),
	    endsAt - (static_cast<double>(ending - cruising.departureStep) * fixedStep),
	    endsAt + (static_cast<double>(cruising.arrivalStep - ending) * fixedStep));
	return {origin + at + flight.Offset(bodies, covered), Facing(leg)};
}

// A course solved for a drive, and its whole steps.
struct Solved
{
	Course course;
	std::uint64_t steps{0};
};


// The course and its time from where the ship is, relative to the carrier, as a Cruise solves them.
std::optional<Solved> Solve(
    const Planner &planner, const CruiseDrive &drive, const glm::dvec3 &start, std::size_t target,
    double t0, double step, double seconds)
{
	// The time from the course, and the course from the time: the power law's time rises as the
	// fourth root of the distance, so each round changes it by a quarter of the last's share, and the
	// rounds settle well within kSolveIterations.
	glm::dvec3 towards = start;
	std::optional<Course> course;
	std::uint64_t steps = 0;
	for (int round = 0; round <= CruiseTravel::kSolveIterations; ++round)
	{
		steps = Jettison::Transit::Steps(seconds, step);
		course = planner.Plot(start, towards, target, t0, t0 + (static_cast<double>(steps) * step));
		if (!course)
			return std::nullopt;
		towards = course->points[course->points.size() - 2];
		if (round < CruiseTravel::kSolveIterations)
			seconds = CruiseSeconds(drive, course->length);
	}

	// A profile's time rises as no more than the square root of the distance, and a slow ship
	// chasing a station round its planet may not settle, where the steps found would accelerate it
	// past its boost: so the fewest steps in which it covers the course they give, up from the
	// solve's by doubling and down by halving.
	if (drive.profile)
	{
		const auto covers = [&](const Course &plotted, std::uint64_t count)
		{ return CruiseSeconds(drive, plotted.length) <= static_cast<double>(count) * step; };
		const auto plot = [&](std::uint64_t count)
		{ return planner.Plot(start, towards, target, t0, t0 + (static_cast<double>(count) * step)); };
		std::uint64_t under = 0;
		for (int round = 0; !covers(*course, steps); ++round)
		{
			course = plot(steps * 2);
			if (round == CruiseTravel::kProfileDoublings || !course)
				return std::nullopt;
			under = steps;
			steps *= 2;
		}
		while (steps - under > 1)
		{
			const std::uint64_t middle = under + ((steps - under) / 2);
			if (std::optional<Course> plotted = plot(middle); plotted && covers(*plotted, middle))
			{
				steps = middle;
				course = std::move(plotted);
			}
			else
				under = middle;
		}
	}
	return Solved{*std::move(course), steps};
}


// What a crossing cruise's check found: where its drive thrust within a keepout, the clearances that
// reach past it, and the bodies it came within, at their places in its course's frame then.
struct Checked
{
	bool inside{false};
	double departure{0.0};
	double arrival{0.0};
	std::vector<Sphere> near;
};


// A keepout, by its body's index.
struct Kept
{
	std::size_t body{0};
	double radius{0.0};
};


// The solved cruise sampled as flown (SettleCruise).
Checked Check(
    const Planner &planner, const CruiseDrive &drive, const Solved &solved, const std::vector<Kept> &kept,
    int departure, int arrival, double t0, double step)
{
	const Space::StarSystem &system = planner.system;
	const double length = solved.course.length;
	const Crossing crossing = *CrossingOf(drive, length);
	const double whole = static_cast<double>(solved.steps) * step;
	const double seconds = CrossedSeconds(*drive.profile, crossing, length);
	const double driven = length - (crossing.departure + crossing.arrival);
	const Flight flight =
	    Flight::Of(system, crossing, length, planner.carrier, departure, arrival, t0, t0 + whole);
	Checked checked{false, crossing.departure, crossing.arrival, {}};

	// Each solid body's and each keepout's place relative to the ship at the last sample.
	std::vector<std::size_t> solids;
	for (std::size_t body = 0; body < system.bodies.size(); ++body)
		if (const Space::BodyKind kind = system.bodies[body].kind;
		    kind == Space::BodyKind::Star || kind == Space::BodyKind::Planet ||
		    kind == Space::BodyKind::Moon)
			solids.push_back(body);
	std::vector<std::optional<glm::dvec3>> lastSolid(solids.size());
	std::vector<std::optional<glm::dvec3>> lastKept(kept.size());
	const auto widened = [](std::optional<glm::dvec3> &last, const glm::dvec3 &now)
	{
		const double moved = last ? glm::length(now - *last) : 0.0;
		last = now;
		return moved;
	};

	// The next sample comes no later than its interval, nor than half the time the ship, at the speed
	// it last moved relative to each body, takes to close the margin it has from it, so a fast ship near
	// a body is sampled finely and the widening stays within the margin; never sooner than a step.
	double interval = 0.0;
	for (double elapsed = 0.0;;)
	{
		const double time = t0 + elapsed;
		const Progress progress =
		    CrossedProgress(*drive.profile, crossing, length, elapsed / whole * seconds);
		const std::vector<Space::SystemPosition> bodies = Space::EvaluateSystem(system, time);
		const glm::dvec3 origin = planner.carrier < 0
		                              ? glm::dvec3{0.0}
		                              : bodies[static_cast<std::size_t>(planner.carrier)].metres;
		const glm::dvec3 offset = flight.Offset(bodies, progress.covered);
		const glm::dvec3 ship =
		    origin + offset +
		    Jettison::Transit::Along(solved.course.points, progress.covered / length).first;
		const bool close = progress.thrusting || !progress.driving;
		double next =
		    close ? CruiseTravel::kThrustSampleSeconds : CruiseTravel::kCoastSampleSeconds;
		const auto closing = [&](double margin, double moved)
		{
			if (interval > 0.0 && moved > 0.0 && margin > 0.0)
				next = std::min(next, margin * interval / (2.0 * moved));
		};

		for (std::size_t each = 0; each < solids.size(); ++each)
		{
			const glm::dvec3 relative = ship - bodies[solids[each]].metres;
			const double radius = system.bodies[solids[each]].radius;
			const double moved = widened(lastSolid[each], relative);
			const double distance = glm::length(relative);
			if (distance < radius + moved + CruiseTravel::kCourseClearance)
				checked.near.push_back({bodies[solids[each]].metres - origin - offset, radius + moved});
			closing(distance - radius - CruiseTravel::kCourseClearance, moved);
		}
		for (std::size_t each = 0; each < kept.size(); ++each)
		{
			const glm::dvec3 relative = ship - bodies[kept[each].body].metres;
			const double moved = widened(lastKept[each], relative);
			const double distance = glm::length(relative);
			closing(distance - kept[each].radius, moved);
			const double deficit = kept[each].radius + moved - distance;
			if (!progress.thrusting || deficit <= 0.0)
				continue;

			// Past it, along the course from its end of the drive's metres.
			checked.inside = true;
			if (progress.covered - crossing.departure < driven / 2.0)
				checked.departure = std::max(checked.departure, progress.covered + deficit);
			else
				checked.arrival = std::max(checked.arrival, length - progress.covered + deficit);
		}

		if (elapsed >= whole)
			break;
		interval = std::min(whole - elapsed, std::max(next, step));
		elapsed += interval;
	}
	return checked;
}
} // namespace


double SettledCruise::Metres() const
{
	return length;
}


double SettledCruise::DriveMetres() const
{
	const double crossed =
	    std::min(departureClearance, length / 2.0) + std::min(arrivalClearance, length / 2.0);
	return std::max(0.0, length - crossed);
}


Settlement SettleCruise(
    const StarSystem &system, const CruiseDrive &drive, const glm::dvec3 &from, std::string_view frame,
    std::string_view target, double now, double fixedStep, int rounds)
{
	const int targetIndex = system.Find(target);
	if (targetIndex < 0)
		return {std::nullopt, Reason::NoSuchBody};
	std::vector<Kept> kept;
	for (const Keepout &keepout : drive.keepouts)
	{
		const int body = system.Find(keepout.body);
		if (body < 0)
			return {std::nullopt, Reason::NoSuchBody};
		kept.push_back({static_cast<std::size_t>(body), keepout.radius});
	}

	// The carrier: the nearest body both the frame left and the target orbit.
	const int frameIndex = frame.empty() ? -1 : system.Find(frame);
	Planner planner{system, -1, Chain(system, frameIndex), Chain(system, targetIndex), {}};
	for (const std::size_t orbited : planner.departure)
		if (Contains(planner.arrival, orbited))
		{
			planner.carrier = static_cast<int>(orbited);
			break;
		}
	const auto goal = static_cast<std::size_t>(targetIndex);
	const glm::dvec3 start = from - planner.CarrierAt(now);
	const std::vector<Space::SystemPosition> bodies = Space::EvaluateSystem(system, now);
	const double seconds = CruiseSeconds(drive, glm::distance(from, bodies[goal].metres));
	const auto settled = [&](Solved solved, const CruiseDrive &flown, std::string departureBody)
	{
		return Settlement{
		    SettledCruise{
		        std::move(solved.course.points), solved.steps,
		        planner.carrier < 0 ? std::string{}
		                            : system.bodies[static_cast<std::size_t>(planner.carrier)].name,
		        std::move(departureBody), flown.departureClearance, flown.arrivalClearance,
		        solved.course.length}};
	};

	CruiseDrive flown = drive;
	const bool crosses = drive.profile && drive.crossingAcceleration > 0.0f &&
	                     (drive.departureClearance > 0.0 || drive.arrivalClearance > 0.0);
	const int departure = Anchor(system, frameIndex);
	const int arrival = Anchor(system, targetIndex);
	for (int round = 0;; ++round)
	{
		std::optional<Solved> solved = Solve(
		    planner, flown, start, goal, now, fixedStep,
		    round == 0 ? seconds : CruiseSeconds(flown, glm::distance(from, bodies[goal].metres)));
		if (!solved)
			return {std::nullopt, Reason::NoCourse};
		if (!crosses)
			return settled(*std::move(solved), flown, {});
		const Checked checked = Check(planner, flown, *solved, kept, departure, arrival, now, fixedStep);
		if (!checked.inside && checked.near.empty())
			return settled(
			    *std::move(solved), flown,
			    departure < 0 ? std::string{} : system.bodies[static_cast<std::size_t>(departure)].name);
		if (round == rounds)
			return {std::nullopt, checked.inside ? Reason::InsideRadius : Reason::NoCourse};
		flown.departureClearance = std::max(flown.departureClearance, checked.departure);
		flown.arrivalClearance = std::max(flown.arrivalClearance, checked.arrival);
		planner.extra.insert(planner.extra.end(), checked.near.begin(), checked.near.end());
	}
}


double CruiseSeconds(const CruiseDrive &drive, double metres)
{
	if (metres <= 0.0)
		return 0.0;
	if (drive.profile)
		return CrossedSeconds(*drive.profile, CrossingOf(drive, metres), metres);
	return double{drive.seconds} * std::pow(metres / drive.reference, double{drive.exponent});
}


double DriveMetres(const CruiseDrive &drive, double metres)
{
	const std::optional<Crossing> crossing = CrossingOf(drive, metres);
	return crossing ? metres - (crossing->departure + crossing->arrival) : std::max(metres, 0.0);
}


void CheckCruiseDrive(std::string_view name, const CruiseDrive &drive)
{
	if (!(drive.seconds > 0.0f) || !std::isfinite(drive.seconds) || !(drive.reference > 0.0) ||
	    !std::isfinite(drive.reference) || !(drive.exponent > 0.0f) || !(drive.exponent <= 1.0f))
		throw Jettison::Serialise::FormatError{std::format(
		    "Cruise drive '{}' has seconds or a reference that is not positive, or an exponent that "
		    "is not above 0 and at most 1",
		    name)};
	if (drive.profile)
		Jettison::Transit::CheckProfile(std::format("Cruise drive '{}'", name), *drive.profile);
	const bool crosses = drive.crossingAcceleration > 0.0f && std::isfinite(drive.crossingAcceleration);
	for (const double clearance : {drive.departureClearance, drive.arrivalClearance})
		if (!std::isfinite(clearance) || clearance < 0.0 || (clearance > 0.0 && !crosses))
			throw Jettison::Serialise::FormatError{std::format(
			    "Cruise drive '{}' has a clearance that is not finite and at least 0, or one with no "
			    "crossing acceleration above 0",
			    name)};
	for (const Keepout &keepout : drive.keepouts)
		if (!std::isfinite(keepout.radius) || !(keepout.radius > 0.0))
			throw Jettison::Serialise::FormatError{std::format(
			    "Cruise drive '{}' has a keepout about '{}' whose radius is not finite and above 0", name,
			    keepout.body)};
}


double CruiseTravel::ArrivalDistance(const Space::Body &body)
{
	return std::max(
	    (body.radius * kArrivalRadiusShare) + kArrivalStandoff,
	    std::sqrt(body.gravitationalParameter / kArrivalGravity));
}


std::optional<Destination> ClearOf(
    const Jettison::Sim::SimWorld &world, const Jettison::Physics::PhysicsWorld &physics,
    const Space::StarSystem &system, entt::entity ship, entt::entity station, std::string_view body)
{
	const entt::registry &registry = world.Registry();
	const auto *inSystem = registry.try_get<Jettison::Sim::InFrame>(ship);
	const auto *frame = registry.try_get<Jettison::Physics::LocalFrame>(ship);
	const auto *transform = registry.try_get<Jettison::Sim::Transform>(ship);
	const auto *rails = registry.try_get<Jettison::Physics::OnRails>(station);
	const auto *stationBody = registry.try_get<Jettison::Physics::Body>(station);
	if (!inSystem || inSystem->frame != Jettison::Sim::FrameId(system.name) || !frame || !transform ||
	    !rails || !stationBody)
		return std::nullopt;
	const int left = system.Find(rails->body);
	const int target = system.Find(body);
	if (left < 0 || target < 0)
		return std::nullopt;

	// Where the bodies are as the ship's Transform has it, a step on from Now after physics.
	const double now = physics.PlacedAt().value_or(world.Clock().Now());
	const std::vector<Space::SystemPosition> bodies = Space::EvaluateSystem(system, now);
	const Space::Body &leaving = system.bodies[static_cast<std::size_t>(left)];
	const glm::dvec3 centre = bodies[static_cast<std::size_t>(left)].metres;
	const glm::dvec3 at = transform->position;
	if (glm::distance(at, centre) >= leaving.radius + CruiseTravel::kStationReach)
		return std::nullopt;

	// A point just beyond its reach, on the side of the body to cruise to.
	const glm::dvec3 way = bodies[static_cast<std::size_t>(target)].metres - centre;
	const glm::dvec3 out = glm::length(way) > 1.0 ? glm::normalize(way) : glm::dvec3{0.0, 0.0, 1.0};
	const glm::dvec3 clear =
	    centre + out * (leaving.radius + CruiseTravel::kStationReach + kReachMargin);
	const glm::dvec3 origin = physics.FrameOrigin(inSystem->frame, frame->body, now);
	Destination flight;
	flight.offset = clear - origin;
	const Sphere sphere{
	    registry.get<Jettison::Sim::Transform>(station).position,
	    physics.ShapeRadius(stationBody->shape)};
	const Waypoints waypoints{{sphere}, kReachClearance};
	if (const std::optional<Course> course = waypoints.PlotCourse(at, clear))
		for (std::size_t point = 1; point + 1 < course->points.size(); ++point)
			flight.waypoints.push_back(course->points[point] - origin);
	return flight;
}


CruiseTravel &CruiseTravel::Attach(
    Jettison::Sim::SimWorld &world, const Jettison::Physics::PhysicsWorld &physics)
{
	RegisterTypes();

	// Shared by the command and the system, which the world keeps for as long as it lives.
	std::shared_ptr<CruiseTravel> cruises{new CruiseTravel{physics}};
	world.RegisterComponent<CruiseDrive>();
	world.RegisterComponent<Cruising>();
	world.ShowInSnapshots<Cruising>();
	world.RegisterCommand<Cruise>([cruises](Jettison::Sim::SimWorld &target, const Cruise &command)
	                              { cruises->Start(target, command); });
	world.RegisterSystem(
	    {std::string{kSystemName},
	     Jettison::Ecs::TickPhase::Simulation,
	     Jettison::Ecs::Access::Of<CruiseDrive, Jettison::Sim::InFrame>()
	         .Writing<
	             Cruising, Jettison::Sim::Transform, Jettison::Physics::Body,
	             Jettison::Physics::LocalFrame, Jettison::Physics::Velocity,
	             Jettison::Physics::FlightControls>(),
	     {std::string{LaneTravel::kSystemName}},
	     [cruises, &world](entt::registry & /*registry*/, double /*fixedStep*/)
	     { cruises->Place(world); }},
	    // Until the step that places an arrival, each ship is where its course has it.
	    {.next =
	         [](const Jettison::Sim::SimWorld &target)
	     {
		     std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
		     for (const auto [ship, cruising] : target.Registry().view<const Cruising>().each())
			     next = std::min(
			         next, cruising.arrivalStep - std::min<std::uint64_t>(cruising.arrivalStep, 1));
		     return next;
	     },
	     .skip =
	         [cruises](Jettison::Sim::SimWorld &target, std::uint64_t /*passed*/, double /*lastBegan*/)
	     { cruises->Skip(target); }});
	return *cruises;
}


void CruiseTravel::DefineStarSystem(Space::StarSystem system)
{
	const std::uint64_t id = Jettison::Sim::FrameId(system.name);
	if (m_systems.contains(id))
		throw std::invalid_argument{
		    std::format("Ai: star system '{}' is already defined", system.name)};
	m_systems.emplace(id, std::move(system));
}


bool CruiseTravel::HasStarSystem(std::uint64_t system) const
{
	return m_systems.contains(system);
}


void CruiseTravel::Start(Jettison::Sim::SimWorld &world, const Cruise &command) const
{
	entt::registry &registry = world.Registry();
	const auto refuse = [&](Reason reason, std::vector<Jettison::Sim::ReasonArgument> arguments = {})
	{
		world.Events().Emit(
		    CruiseRefused{
		        command.ship, command.body, Jettison::Sim::MakeReason(reason, std::move(arguments))});
	};
	const entt::entity ship = command.ship;
	if (!registry.valid(ship))
		return refuse(Reason::NoSuchEntity);
	if (!Jettison::Sim::MayCommand(registry, ship, command.sender))
		return refuse(Reason::NotController);
	const auto *inSystem = registry.try_get<Jettison::Sim::InFrame>(ship);
	if (!inSystem || inSystem->frame == Jettison::Sim::kNoFrame)
		return refuse(registry.all_of<OnLane>(ship) ? Reason::OnLane : Reason::InNoSystem);
	if (registry.all_of<Cruising>(ship))
		return refuse(Reason::AlreadyCruising);
	const auto *body = registry.try_get<Jettison::Physics::Body>(ship);
	const auto *drive = registry.try_get<CruiseDrive>(ship);
	const auto *transform = registry.try_get<Jettison::Sim::Transform>(ship);
	if (!body || body->kind != Jettison::Physics::BodyKind::Dynamic || !drive || !transform ||
	    registry.all_of<Jettison::Physics::OnRails>(ship))
		return refuse(Reason::CannotCruise);
	const auto found = m_systems.find(inSystem->frame);
	if (found == m_systems.end())
		return refuse(Reason::SystemUndefined);
	const Space::StarSystem &system = found->second;
	const int targetIndex = system.Find(command.body);
	if (targetIndex < 0)
		return refuse(
		    Reason::NoSuchBody, {Jettison::Sim::ReasonArgument::Identifier(system.name),
		                         Jettison::Sim::ReasonArgument::Identifier(command.body)});
	const auto target = static_cast<std::size_t>(targetIndex);

	const double t0 = world.Clock().Now();
	const glm::dvec3 from = transform->position;
	const std::vector<Space::SystemPosition> bodies = Space::EvaluateSystem(system, t0);

	// The reach measured where the bodies are as the ship's Transform has it
	// (Jettison::Physics::PlacedAt), a step on from t0 for a cruise applied after physics, as a
	// station's frame moves hundreds of metres a step with its planet. The course keeps t0, as the
	// cruise's steps count from it.
	const double placed = m_physics.PlacedAt().value_or(t0);
	const std::vector<Space::SystemPosition> placedBodies =
	    placed == t0 ? bodies : Space::EvaluateSystem(system, placed);
	for (std::size_t station = 0; station < system.bodies.size(); ++station)
		if (system.bodies[station].kind == Space::BodyKind::Station &&
		    glm::distance(from, placedBodies[station].metres) <
		        system.bodies[station].radius + kStationReach)
			return refuse(
			    Reason::NearStation,
			    {Jettison::Sim::ReasonArgument::Identifier(system.bodies[station].name)});
	if (glm::distance(from, placedBodies[target].metres) <
	    ArrivalDistance(system.bodies[target]) + kStationReach)
		return refuse(Reason::AlreadyThere, {Jettison::Sim::ReasonArgument::Identifier(command.body)});

	const auto *frame = registry.try_get<Jettison::Physics::LocalFrame>(ship);
	const Settlement settlement = SettleCruise(
	    system, *drive, from, frame ? std::string_view{frame->body} : std::string_view{}, command.body,
	    t0, world.FixedStep());
	if (!settlement.cruise)
		return refuse(
		    settlement.refusal, {Jettison::Sim::ReasonArgument::Identifier(command.body)});
	const SettledCruise &settled = *settlement.cruise;
	const std::uint64_t steps = settled.steps;

	const int parent = system.bodies[target].parentIndex;
	const bool byStation =
	    system.bodies[target].kind == Space::BodyKind::JumpPoint && parent >= 0 &&
	    system.bodies[static_cast<std::size_t>(parent)].kind == Space::BodyKind::Station;
	Cruising cruising;
	cruising.target = command.body;
	cruising.frame = byStation ? system.bodies[static_cast<std::size_t>(parent)].name : command.body;
	cruising.carrier = settled.carrier;
	cruising.course = settled.course;
	cruising.departureStep = world.Clock().Step();
	cruising.arrivalStep = cruising.departureStep + steps;
	cruising.body = Jettison::Transit::TakeOut(registry, ship);
	cruising.profile = drive->profile;
	cruising.departureClearance = settled.departureClearance;
	cruising.arrivalClearance = settled.arrivalClearance;
	cruising.crossingAcceleration = drive->crossingAcceleration;
	cruising.departureBody = settled.departureBody;

	world.Events().Emit(
	    CruiseStarted{
	        ship, command.body, static_cast<double>(steps) * world.FixedStep(), settled.Metres(),
	        cruising.arrivalStep});
	registry.emplace_or_replace<Cruising>(ship, std::move(cruising));
}


void CruiseTravel::Skip(Jettison::Sim::SimWorld &world) const
{
	entt::registry &registry = world.Registry();

	// As the last step passed over placed them: it ended at the clock's count and time.
	const std::uint64_t ending = world.Clock().Step();
	const double endsAt = world.Clock().Now();
	for (const auto [ship, cruising, inFrame] :
	     registry.view<const Cruising, const Jettison::Sim::InFrame>().each())
	{
		const Space::StarSystem &starSystem = m_systems.at(inFrame.frame);
		registry.emplace_or_replace<Jettison::Sim::Transform>(
		    ship, Underway(
		              cruising, starSystem, starSystem.Find(cruising.carrier), ending, endsAt,
		              world.FixedStep()));
	}
}


void CruiseTravel::Place(Jettison::Sim::SimWorld &world) const
{
	entt::registry &registry = world.Registry();
	std::vector<entt::entity> ships;
	for (const entt::entity ship : registry.view<const Cruising>())
		ships.push_back(ship);
	if (ships.empty())
		return;
	std::ranges::sort(ships, {}, [](entt::entity ship) { return entt::to_integral(ship); });

	// The step being run ends at the next count, and at this time.
	const std::uint64_t ending = world.Clock().Step() + 1;
	const double endsAt = world.Clock().Now() + world.FixedStep();

	// What each arrival comes out clear of, gathered at the first: what is already in the systems
	// the ships cruise in, and each arrival once it is placed.
	std::optional<std::vector<Jettison::Transit::Occupant>> occupants;
	for (const entt::entity ship : ships)
	{
		const Cruising &cruising = registry.get<const Cruising>(ship);
		const std::uint64_t system = registry.get<const Jettison::Sim::InFrame>(ship).frame;
		const auto found = m_systems.find(system);
		const int carrier = found == m_systems.end() ? -1 : found->second.Find(cruising.carrier);
		const int target = found == m_systems.end() ? -1 : found->second.Find(cruising.target);
		const bool departed = cruising.departureBody.empty() ||
		                      (found != m_systems.end() && found->second.Find(cruising.departureBody) >= 0);
		if (target < 0 || (carrier < 0 && !cruising.carrier.empty()) || !departed)
			throw std::invalid_argument{std::format(
			    "Ai: entity {:#x} cruises to '{}' by '{}' from '{}' in a system {}",
			    entt::to_integral(ship), cruising.target, cruising.carrier, cruising.departureBody,
			    found == m_systems.end() ? "not defined" : "without those bodies")};
		const Space::StarSystem &starSystem = found->second;
		const glm::dvec3 origin =
		    carrier < 0
		        ? glm::dvec3{0.0}
		        : Space::EvaluateBody(starSystem, static_cast<std::size_t>(carrier), endsAt).metres;

		if (ending < cruising.arrivalStep)
		{
			registry.emplace_or_replace<Jettison::Sim::Transform>(
			    ship, Underway(cruising, starSystem, carrier, ending, endsAt, world.FixedStep()));
			continue;
		}

		if (!occupants)
		{
			std::vector<std::uint64_t> inSystems;
			inSystems.reserve(ships.size());
			for (const entt::entity other : ships)
				inSystems.push_back(registry.get<const Jettison::Sim::InFrame>(other).frame);
			occupants = Jettison::Transit::Occupants(registry, m_physics, inSystems);
		}
		const glm::dvec3 end = origin + cruising.course.back();
		const glm::dvec3 ahead =
		    Space::EvaluateBody(starSystem, static_cast<std::size_t>(target), endsAt).metres - end;
		Jettison::Sim::Transform place{
		    end, Facing(glm::length(ahead) > 0.0 ? glm::normalize(ahead) : glm::dvec3{0.0, 0.0, -1.0})};
		const double radius = m_physics.ShapeRadius(cruising.body.shape);
		place.position = Jettison::Transit::ClearPlace(
		    *occupants, system, place, radius, LaneTravel::kArrivalClearance);
		occupants->push_back({system, place.position, radius});
		Jettison::Transit::BringOut(registry, ship, place, cruising.body, cruising.frame);
		const std::string body = cruising.target;
		registry.remove<Cruising>(ship);
		world.Events().Emit(CruiseEnded{ship, body});
	}
}
} // namespace Manifest::Space
