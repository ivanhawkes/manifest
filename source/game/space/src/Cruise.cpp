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
#include <manifest/space/Orbit.h>
#include <manifest/space/Routes.h>
#include <manifest/space/Types.h>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
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


bool Solid(Space::BodyKind kind)
{
	return kind == Space::BodyKind::Star || kind == Space::BodyKind::Planet ||
	       kind == Space::BodyKind::Moon;
}


// A body's velocity relative to the primary, in the system's axes: its orbit's and each it orbits'.
glm::dvec3 VelocityOf(const Space::StarSystem &system, int body, double time)
{
	glm::dvec3 velocity{0.0};
	for (int at = body; at >= 0; at = system.bodies[static_cast<std::size_t>(at)].parentIndex)
		velocity +=
		    Space::EvaluateState(system.bodies[static_cast<std::size_t>(at)].orbit, time).velocity;
	return velocity;
}


// The frame a cruise to the body arrives in: the body's own, or for a jump point that keeps station by
// a station, the station's, as a lane's arrival is.
int ArrivalFrame(const Space::StarSystem &system, int target)
{
	const int parent = system.bodies[static_cast<std::size_t>(target)].parentIndex;
	const bool byStation =
	    system.bodies[static_cast<std::size_t>(target)].kind == Space::BodyKind::JumpPoint &&
	    parent >= 0 && system.bodies[static_cast<std::size_t>(parent)].kind == Space::BodyKind::Station;
	return byStation ? parent : target;
}


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
	if ((!(departure > 0.0) && !(arrival > 0.0)) || !(acceleration > 0.0f) || !(metres > 0.0))
		return std::nullopt;
	return Crossing{
	    {acceleration, 0.0f, acceleration, {}},
	    std::min(std::max(departure, 0.0), metres / 2.0),
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


// The way a course's first leg of any length runs, or its last's.
glm::dvec3 EndLeg(std::span<const glm::dvec3> course, bool last)
{
	for (std::size_t each = 1; each < course.size(); ++each)
	{
		const std::size_t to = last ? course.size() - each : each;
		const glm::dvec3 leg = course[to] - course[to - 1];
		if (glm::length(leg) > 0.0)
			return glm::normalize(leg);
	}
	return glm::dvec3{0.0, 0.0, -1.0};
}


// How a cruise covers its course (L37): eased along it, or as its profile does with its crossings,
// and on a profile off it by the drift (Transit::Drift) that carries what the profile's end speeds do
// not of the velocities it leaves and arrives with, relative to its carrier, so it joins its frames at
// each end moving as they do. A drive that crosses leaves and arrives at rest on its thrusters relative
// to the bodies it crosses by, so its drift carries each velocity whole; any other meets each
// velocity's speed along the course at its end (Transit::EndSpeeds) where its profile can
// (Transit::CheckEnds) and drifts the rest, or drifts it all where the profile cannot.
struct Motion
{
	double length{0.0};
	std::optional<Jettison::Transit::MotionProfile> profile;
	std::optional<Crossing> crossing;
	Jettison::Transit::EndSpeeds ends;
	glm::dvec3 leaving{0.0};
	glm::dvec3 arriving{0.0};

	// The profile's own time over the course, its crossings included; none eased.
	double seconds{0.0};


	static Motion Of(
	    const std::optional<Jettison::Transit::MotionProfile> &profile, double departureClearance,
	    double arrivalClearance, float crossingAcceleration, std::span<const glm::dvec3> course,
	    const glm::dvec3 &leaving, const glm::dvec3 &arriving)
	{
		Motion motion;
		motion.length = Jettison::Transit::Length(course);
		motion.profile = profile;
		if (!profile)
			return motion;
		motion.crossing =
		    CrossingOf(departureClearance, arrivalClearance, crossingAcceleration, motion.length);
		motion.leaving = leaving;
		motion.arriving = arriving;
		if (!motion.crossing && motion.length > 0.0)
		{
			const Jettison::Transit::Split out =
			    Jettison::Transit::SplitAlong(leaving, EndLeg(course, false));
			const Jettison::Transit::Split in =
			    Jettison::Transit::SplitAlong(arriving, EndLeg(course, true));
			const Jettison::Transit::EndSpeeds ends{out.along, in.along};
			if (!Jettison::Transit::CheckEnds(*profile, motion.length, ends))
			{
				motion.ends = ends;
				motion.leaving = out.across;
				motion.arriving = in.across;
			}
		}
		motion.seconds = motion.crossing
		                     ? CrossedSeconds(*profile, motion.crossing, motion.length)
		                     : Jettison::Transit::Seconds(*profile, motion.length, motion.ends);
		return motion;
	}


	// The share of the course covered at that share of the whole steps, slowed to them as
	// Transit::Underway's profile is.
	double Along(double share) const
	{
		if (!profile)
			return Jettison::Transit::Eased(share);
		if (!(length > 0.0))
			return 1.0;
		if (crossing)
			return CrossedProgress(*profile, *crossing, length, share * seconds).covered / length;
		return Jettison::Transit::Covered(*profile, length, share * seconds, ends) / length;
	}


	glm::dvec3 Drift(double share, double whole) const
	{
		return Jettison::Transit::Drift(leaving, arriving, whole, share * whole);
	}


	// Where it is relative to the carrier at that share of the whole time, in seconds, along that
	// course, and the way the leg it is on runs.
	std::pair<glm::dvec3, glm::dvec3> At(
	    std::span<const glm::dvec3> course, double share, double whole) const
	{
		const auto [at, leg] = Jettison::Transit::Along(course, Along(share));
		return {at + Drift(share, whole), leg};
	}
};


Motion MotionOf(const Cruising &cruising)
{
	return Motion::Of(
	    cruising.profile, cruising.departureClearance, cruising.arrivalClearance,
	    cruising.crossingAcceleration, cruising.course, cruising.departureVelocity,
	    cruising.arrivalVelocity);
}


// Where a cruising ship is as the step ending at that count ends, about its carrier's place then
// (origin), as its motion covers its course: with its end at that place instead, for a cruise to a
// ship, aimed again at where the ship is.
Jettison::Sim::Transform Underway(
    const Cruising &cruising, const glm::dvec3 &origin, std::uint64_t ending, double fixedStep,
    const std::optional<glm::dvec3> &end = std::nullopt)
{
	const std::uint64_t steps = cruising.arrivalStep - cruising.departureStep;
	const double share =
	    static_cast<double>(ending - cruising.departureStep) / static_cast<double>(steps);
	std::vector<glm::dvec3> aimed;
	std::span<const glm::dvec3> course = cruising.course;
	if (end)
	{
		aimed = cruising.course;
		aimed.back() = *end;
		course = aimed;
	}
	const auto [at, leg] = MotionOf(cruising).At(course, share, static_cast<double>(steps) * fixedStep);
	return {origin + at, Facing(leg)};
}


// A course's end aimed at a ship it cruises to, relative to the carrier: CruiseTravel::kShipStandoff
// off it back along the course's last leg.
glm::dvec3 Aimed(const Cruising &cruising, const glm::dvec3 &ship)
{
	return ship - (EndLeg(cruising.course, true) * CruiseTravel::kShipStandoff);
}


// A course's turns at its waypoints bent round (CruiseTravel::kBendPoints): each corner a quadratic
// Bézier from half-way along the leg into it, pulled by the corner, to half-way along the leg out, so
// two turns never overlap; cut nearer the corner by halves where a point or a chord between two would
// come within an obstacle's radius and the clearance that neither end lies within (Waypoints judges
// the legs at an end within one by the radius alone), and left a corner after kBendCuts.
Course Bend(
    Course course, std::span<const Sphere> obstacles, const glm::dvec3 &start, const glm::dvec3 &end)
{
	if (course.points.size() < 3)
		return course;
	const auto clear = [&](const glm::dvec3 &point)
	{
		for (const Sphere &obstacle : obstacles)
		{
			const double reach = obstacle.radius + CruiseTravel::kCourseClearance;
			if (glm::distance(start, obstacle.centre) >= reach &&
			    glm::distance(end, obstacle.centre) >= reach &&
			    glm::distance(point, obstacle.centre) < reach)
				return false;
		}
		return true;
	};
	std::vector<glm::dvec3> bent{course.points.front()};
	for (std::size_t at = 1; at + 1 < course.points.size(); ++at)
	{
		const glm::dvec3 &corner = course.points[at];
		const glm::dvec3 into = corner - course.points[at - 1];
		const glm::dvec3 out = course.points[at + 1] - corner;
		std::vector<glm::dvec3> curve;
		double cut = 1.0 / 2.0;
		for (int round = 0; round < CruiseTravel::kBendCuts && curve.empty(); ++round, cut /= 2.0)
		{
			const glm::dvec3 from = corner - (into * cut);
			const glm::dvec3 to = corner + (out * cut);
			bool kept = true;
			for (int point = 0; point < CruiseTravel::kBendPoints && kept; ++point)
			{
				const double t =
				    static_cast<double>(point) / static_cast<double>(CruiseTravel::kBendPoints - 1);
				const double rest = 1.0 - t;
				const glm::dvec3 place =
				    (rest * rest * from) + (2.0 * rest * t * corner) + (t * t * to);
				kept = clear(place) && (curve.empty() || clear((place + curve.back()) / 2.0));
				curve.push_back(place);
			}
			if (!kept)
				curve.clear();
		}
		if (curve.empty())
			bent.push_back(corner);
		else
			bent.insert(bent.end(), curve.begin(), curve.end());
	}
	bent.push_back(course.points.back());
	course.length = Jettison::Transit::Length(bent);
	course.points = std::move(bent);
	return course;
}


// The share of a cruise's time its sample (of CruiseTravel::kPassSamples) is taken at: closer together
// towards each end, where a ship crossing slowly beside the body it leaves or reaches moves across it
// most, a few minutes apart in a week's cruise, and further apart mid-course.
double PassShare(std::size_t sample)
{
	return (1.0 - std::cos(
	                  std::numbers::pi * static_cast<double>(sample) /
	                  static_cast<double>(CruiseTravel::kPassSamples))) /
	       2.0;
}


// What a course is planned and flown against: the system, the drive, the carrier and the two ends'
// chains, the target, and the velocities its ends carry.
struct Planner
{
	const Space::StarSystem &system;
	CruiseDrive drive;
	int carrier{-1};
	std::vector<std::size_t> departure;
	std::vector<std::size_t> arrival;

	// The body cruised to, or none for a ship; the body whose frame it arrives in (none for the
	// system's own), whose velocity it arrives with; and for a ship, its place relative to that
	// frame's body and its velocity relative to it, as the cruise sets out, which it is taken to hold.
	int target{-1};
	int frame{-1};
	glm::dvec3 offset{0.0};
	glm::dvec3 velocity{0.0};
	double standoff{0.0};
	double t0{0.0};

	// The velocity the ship leaves with, relative to the carrier.
	glm::dvec3 leaving{0.0};


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


	glm::dvec3 VelocityRelative(int body, double time) const
	{
		const glm::dvec3 carried = carrier < 0 ? glm::dvec3{0.0} : VelocityOf(system, carrier, time);
		return (body < 0 ? glm::dvec3{0.0} : VelocityOf(system, body, time)) - carried;
	}


	glm::dvec3 TargetAt(double time) const
	{
		if (target >= 0)
			return Relative(static_cast<std::size_t>(target), time);
		const glm::dvec3 base =
		    frame < 0 ? -CarrierAt(time) : Relative(static_cast<std::size_t>(frame), time);
		return base + offset + (velocity * (time - t0));
	}


	glm::dvec3 Arriving(double time) const
	{
		return VelocityRelative(frame, time) + (target >= 0 ? glm::dvec3{0.0} : velocity);
	}


	Motion MotionOver(std::span<const glm::dvec3> course, double t1) const
	{
		return Motion::Of(
		    drive.profile, drive.departureClearance, drive.arrivalClearance, drive.crossingAcceleration,
		    course, leaving, Arriving(t1));
	}


	// The time the drive takes over a course to arrive at t1.
	double Seconds(const Course &course, double t1) const
	{
		return drive.profile ? MotionOver(course.points, t1).seconds
		                     : CruiseSeconds(drive, course.length);
	}


	// A body where it is in the course's frame as the ship passes nearest it, sampled through the
	// cruise: its place relative to the carrier less the drift then, widened by how far that moves to
	// the samples either side. A body the ship sets out beside, or arrives beside, and passes nearest
	// there, is where it is then, as the ship leaves it or reaches it, and is not widened.
	Sphere Passing(
	    std::size_t body, std::span<const glm::dvec3> ship, std::span<const glm::dvec3> drift,
	    double t1, std::optional<std::size_t> beside) const
	{
		const double whole = t1 - t0;
		std::vector<glm::dvec3> places;
		places.reserve(ship.size());
		std::size_t nearest = 0;
		double least = std::numeric_limits<double>::infinity();
		for (std::size_t sample = 0; sample < ship.size(); ++sample)
		{
			const double time = sample + 1 == ship.size() ? t1 : t0 + (PassShare(sample) * whole);
			places.push_back(Relative(body, time) - drift[sample]);
			if (const double apart = glm::distance(places.back() + drift[sample], ship[sample]);
			    apart < least)
			{
				least = apart;
				nearest = sample;
			}
		}
		const double radius = system.bodies[body].radius;
		if (beside == nearest)
			return {places[nearest], radius};
		double widened = 0.0;
		if (nearest > 0)
			widened = glm::distance(places[nearest - 1], places[nearest]);
		if (nearest + 1 < places.size())
			widened = std::max(widened, glm::distance(places[nearest + 1], places[nearest]));
		return {places[nearest], radius + widened};
	}


	// The course from the start to the target's standoff on the side of `towards`, both relative to
	// the carrier, for a cruise arriving at t1, its bodies placed as the course last plotted (none, a
	// straight line) would pass them.
	std::optional<Course> Plot(
	    const glm::dvec3 &start, const glm::dvec3 &towards, double t1, const Course *flown) const
	{
		const glm::dvec3 at = TargetAt(t1);
		glm::dvec3 away = towards - at;
		if (glm::length(away) == 0.0)
			away = start - at;
		if (glm::length(away) == 0.0)
			away = glm::dvec3{0.0, 0.0, 1.0};
		const glm::dvec3 end = at + (glm::normalize(away) * standoff);

		// Where the ship is through the cruise, and how far the drift has it off its course.
		const std::vector<glm::dvec3> straight{start, end};
		const std::span<const glm::dvec3> way =
		    flown ? std::span<const glm::dvec3>{flown->points} : std::span<const glm::dvec3>{straight};
		const Motion motion = MotionOver(way, t1);
		const double whole = t1 - t0;
		std::vector<glm::dvec3> ship;
		std::vector<glm::dvec3> drift;
		for (std::size_t sample = 0; sample <= CruiseTravel::kPassSamples; ++sample)
		{
			const double share = PassShare(sample);
			ship.push_back(motion.At(way, share, whole).first);
			drift.push_back(motion.Drift(share, whole));
		}

		std::vector<Sphere> obstacles;
		for (std::size_t body = 0; body < system.bodies.size(); ++body)
		{
			if (!Solid(system.bodies[body].kind))
				continue;
			const bool left = Contains(departure, body);
			const bool reached = Contains(arrival, body);
			std::optional<std::size_t> beside;
			if (left && !reached)
				beside = 0;
			else if (reached && !left)
				beside = ship.size() - 1;
			obstacles.push_back(Passing(body, ship, drift, t1, beside));
		}
		std::optional<Course> course =
		    Waypoints{obstacles, CruiseTravel::kCourseClearance}.PlotCourse(start, end);
		if (!course)
			return std::nullopt;
		return Bend(*std::move(course), obstacles, start, end);
	}
};


// A course solved for a drive, and its whole steps.
struct Solved
{
	Course course;
	std::uint64_t steps{0};
};


// The course and its time from where the ship is, relative to the carrier, as a Cruise solves them.
std::optional<Solved> Solve(
    const Planner &planner, const glm::dvec3 &start, double step, double seconds)
{
	// The time from the course, and the course from the time: the power law's time rises as the
	// fourth root of the distance, so each round changes it by a quarter of the last's share, and the
	// rounds settle well within kSolveIterations.
	const auto arrives = [&](std::uint64_t count)
	{ return planner.t0 + (static_cast<double>(count) * step); };
	glm::dvec3 towards = start;
	std::optional<Course> course;
	std::uint64_t steps = 0;
	for (int round = 0; round <= CruiseTravel::kSolveIterations; ++round)
	{
		steps = Jettison::Transit::Steps(seconds, step);
		std::optional<Course> plotted =
		    planner.Plot(start, towards, arrives(steps), course ? &*course : nullptr);
		if (!plotted)
			return std::nullopt;
		course = std::move(plotted);
		towards = course->points[course->points.size() - 2];
		if (round < CruiseTravel::kSolveIterations)
			seconds = planner.Seconds(*course, arrives(steps));
	}

	// A profile's time rises as no more than the square root of the distance, and a slow ship
	// chasing a station round its planet may not settle, where the steps found would accelerate it
	// past its boost: so the fewest steps in which it covers the course they give, up from the
	// solve's by doubling and down by halving.
	if (planner.drive.profile)
	{
		// Each count's course placed by where the solve's course passes the bodies, so whether it
		// covers it depends on the count alone, whatever the search has tried.
		const Course solved = *course;
		const auto covers = [&](const Course &plotted, std::uint64_t count)
		{ return planner.Seconds(plotted, arrives(count)) <= static_cast<double>(count) * step; };
		const auto plot = [&](std::uint64_t count)
		{ return planner.Plot(start, towards, arrives(count), &solved); };
		std::uint64_t under = 0;
		for (int round = 0; !covers(*course, steps); ++round)
		{
			std::optional<Course> plotted = plot(steps * 2);
			if (round == CruiseTravel::kProfileDoublings || !plotted)
				return std::nullopt;
			course = std::move(plotted);
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


// What a crossing cruise's check found: whether its drive thrust within a keepout, and the clearances
// that reach past it.
struct Checked
{
	bool inside{false};
	double departure{0.0};
	double arrival{0.0};
};


// A keepout, by its body's index.
struct Kept
{
	std::size_t body{0};
	double radius{0.0};
};


// The solved cruise sampled as flown (SettleCruise).
Checked Check(const Planner &planner, const Solved &solved, const std::vector<Kept> &kept, double step)
{
	const CruiseDrive &drive = planner.drive;
	const double length = solved.course.length;
	const Crossing crossing = *CrossingOf(drive, length);
	const double whole = static_cast<double>(solved.steps) * step;
	const Motion motion = planner.MotionOver(solved.course.points, planner.t0 + whole);
	const double driven = length - (crossing.departure + crossing.arrival);
	Checked checked{false, crossing.departure, crossing.arrival};

	// Each keepout's place relative to the ship at the last sample.
	std::vector<std::optional<glm::dvec3>> lastKept(kept.size());
	const auto widened = [](std::optional<glm::dvec3> &last, const glm::dvec3 &now)
	{
		const double moved = last ? glm::length(now - *last) : 0.0;
		last = now;
		return moved;
	};

	// The next sample comes no later than its interval, nor than half the time the ship, at the speed
	// it last moved relative to each keepout's body, takes to close the margin it has from it, so a
	// fast ship near one is sampled finely and the widening stays within the margin; never sooner
	// than a step.
	double interval = 0.0;
	for (double elapsed = 0.0;;)
	{
		const double time = planner.t0 + elapsed;
		const double share = elapsed / whole;
		const Progress progress =
		    CrossedProgress(*drive.profile, crossing, length, share * motion.seconds);
		const glm::dvec3 ship =
		    planner.CarrierAt(time) + motion.At(solved.course.points, share, whole).first;
		const bool close = progress.thrusting || !progress.driving;
		double next = close ? CruiseTravel::kThrustSampleSeconds : CruiseTravel::kCoastSampleSeconds;
		for (std::size_t each = 0; each < kept.size(); ++each)
		{
			const glm::dvec3 relative = ship - planner.Where(kept[each].body, time);
			const double moved = widened(lastKept[each], relative);
			const double distance = glm::length(relative);
			if (const double margin = distance - kept[each].radius;
			    interval > 0.0 && moved > 0.0 && margin > 0.0)
				next = std::min(next, margin * interval / (2.0 * moved));
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
    const glm::dvec3 &velocity, const CruiseTarget &target, double now, double fixedStep, int rounds)
{
	const bool toBody = !target.body.empty();
	const int targetIndex = toBody ? system.Find(target.body) : -1;
	const int targetFrame = toBody ? (targetIndex < 0 ? -1 : ArrivalFrame(system, targetIndex))
	                               : (target.frame.empty() ? -1 : system.Find(target.frame));
	if ((toBody && targetIndex < 0) || (!toBody && !target.frame.empty() && targetFrame < 0))
		return {std::nullopt, Reason::NoSuchBody};
	std::vector<Kept> keeps;
	for (const Keepout &keepout : drive.keepouts)
	{
		const int body = system.Find(keepout.body);
		if (body < 0)
			return {std::nullopt, Reason::NoSuchBody};
		keeps.push_back({static_cast<std::size_t>(body), keepout.radius});
	}

	// The carrier: the nearest body both the frame left and the target (a ship's frame) orbit.
	const int frameIndex = frame.empty() ? -1 : system.Find(frame);
	Planner planner{
	    system,
	    drive,
	    -1,
	    Chain(system, frameIndex),
	    Chain(system, toBody ? targetIndex : targetFrame),
	    targetIndex,
	    targetFrame};
	for (const std::size_t orbited : planner.departure)
		if (Contains(planner.arrival, orbited))
		{
			planner.carrier = static_cast<int>(orbited);
			break;
		}
	planner.t0 = now;
	if (toBody)
		planner.standoff =
		    CruiseTravel::ArrivalDistance(system.bodies[static_cast<std::size_t>(targetIndex)]);
	else
	{
		planner.standoff = CruiseTravel::kShipStandoff;
		planner.offset = target.place -
		                 (targetFrame < 0 ? glm::dvec3{0.0}
		                                  : planner.Where(static_cast<std::size_t>(targetFrame), now));
		planner.velocity = target.velocity;
	}
	planner.leaving = planner.VelocityRelative(frameIndex, now) + velocity;
	const glm::dvec3 start = from - planner.CarrierAt(now);
	const double distance =
	    toBody ? glm::distance(from, planner.Where(static_cast<std::size_t>(targetIndex), now))
	           : glm::distance(from, target.place);
	const auto name = [&](int body)
	{ return body < 0 ? std::string{} : system.bodies[static_cast<std::size_t>(body)].name; };
	const auto settled = [&](Solved solved)
	{
		const double arrives = now + (static_cast<double>(solved.steps) * fixedStep);
		return Settlement{SettledCruise{
		    std::move(solved.course.points), solved.steps, name(planner.carrier),
		    planner.drive.departureClearance, planner.drive.arrivalClearance, solved.course.length,
		    planner.leaving, planner.Arriving(arrives)}};
	};

	const bool crosses = drive.profile && drive.crossingAcceleration > 0.0f &&
	                     (drive.departureClearance > 0.0 || drive.arrivalClearance > 0.0);
	for (int round = 0;; ++round)
	{
		std::optional<Solved> solved =
		    Solve(planner, start, fixedStep, CruiseSeconds(planner.drive, distance));
		if (!solved)
			return {std::nullopt, Reason::NoCourse};
		if (!crosses)
			return settled(*std::move(solved));
		const Checked checked = Check(planner, *solved, keeps, fixedStep);
		if (!checked.inside)
			return settled(*std::move(solved));
		if (round == rounds)
			return {std::nullopt, Reason::InsideRadius};
		planner.drive.departureClearance =
		    std::max(planner.drive.departureClearance, checked.departure);
		planner.drive.arrivalClearance = std::max(planner.drive.arrivalClearance, checked.arrival);
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
			    "Cruise drive '{}' has a keepout about '{}' whose radius is not finite and above 0",
			    name, keepout.body)};
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

	// Shared by the commands and the system, which the world keeps for as long as it lives.
	std::shared_ptr<CruiseTravel> cruises{new CruiseTravel{physics}};
	world.RegisterComponent<CruiseDrive>();
	world.RegisterComponent<Cruising>();
	world.RegisterComponent<CameOut>();
	world.ShowInSnapshots<Cruising>();
	world.RegisterCommand<Cruise>([cruises](Jettison::Sim::SimWorld &target, const Cruise &command)
	                              { cruises->Start(target, command); });
	world.RegisterCommand<BreakOffCruise>(
	    [cruises](Jettison::Sim::SimWorld &target, const BreakOffCruise &command)
	    { cruises->BreakOff(target, command); });
	world.RegisterSystem(
	    {std::string{kSystemName},
	     Jettison::Ecs::TickPhase::Simulation,
	     Jettison::Ecs::Access::Of<
	         CruiseDrive, Jettison::Sim::InFrame, Jettison::Physics::OnRails, Jettison::Ai::Steered>()
	         .Writing<
	             Cruising, CameOut, Jettison::Sim::Transform, Jettison::Physics::Body,
	             Jettison::Physics::LocalFrame, Jettison::Physics::Velocity,
	             Jettison::Physics::FlightControls, Destination>(),
	     {std::string{LaneTravel::kSystemName}},
	     [cruises, &world](entt::registry & /*registry*/, double /*fixedStep*/)
	     { cruises->Place(world); }},
	    // Until the step that places an arrival, each ship cruising to a body is where its course has
	    // it; one cruising to a ship is aimed where the ship is each step.
	    {.next =
	         [](const Jettison::Sim::SimWorld &target)
	     {
		     std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
		     for (const auto [ship, cruising] : target.Registry().view<const Cruising>().each())
			     next = cruising.targetShip == entt::null
			                ? std::min(
			                      next, cruising.arrivalStep -
			                                std::min<std::uint64_t>(cruising.arrivalStep, 1))
			                : std::min(next, target.Clock().Step());
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

	// What it cruises to: a body of its system, or a ship placed in it.
	CruiseTarget aim;
	const entt::entity targetShip = command.targetShip;
	int targetIndex = -1;
	if (targetShip != entt::null)
	{
		const auto *targetIn =
		    registry.valid(targetShip) ? registry.try_get<Jettison::Sim::InFrame>(targetShip) : nullptr;
		const auto *targetPlace =
		    targetIn ? registry.try_get<Jettison::Sim::Transform>(targetShip) : nullptr;
		if (targetShip == ship || !targetPlace || targetIn->frame != inSystem->frame)
			return refuse(Reason::NoSuchTarget);
		aim.place = targetPlace->position;
		if (const auto *targetFrame = registry.try_get<Jettison::Physics::LocalFrame>(targetShip))
			aim.frame = targetFrame->body;
		if (const auto *moving = registry.try_get<Jettison::Physics::Velocity>(targetShip))
			aim.velocity = glm::dvec3{moving->linear};
	}
	else
	{
		targetIndex = system.Find(command.body);
		if (targetIndex < 0)
			return refuse(
			    Reason::NoSuchBody, {Jettison::Sim::ReasonArgument::Identifier(system.name),
			                         Jettison::Sim::ReasonArgument::Identifier(command.body)});
		aim.body = command.body;
	}

	const double t0 = world.Clock().Now();
	const glm::dvec3 from = transform->position;

	// The reach measured where the bodies are as the ship's Transform has it
	// (Jettison::Physics::PlacedAt), a step on from t0 for a cruise applied after physics, as a
	// station's frame moves hundreds of metres a step with its planet. The course keeps t0, as the
	// cruise's steps count from it.
	const double placed = m_physics.PlacedAt().value_or(t0);
	const std::vector<Space::SystemPosition> placedBodies = Space::EvaluateSystem(system, placed);
	for (std::size_t station = 0; station < system.bodies.size(); ++station)
		if (system.bodies[station].kind == Space::BodyKind::Station &&
		    glm::distance(from, placedBodies[station].metres) <
		        system.bodies[station].radius + kStationReach)
			return refuse(
			    Reason::NearStation,
			    {Jettison::Sim::ReasonArgument::Identifier(system.bodies[station].name)});
	if (targetIndex >= 0 &&
	    glm::distance(from, placedBodies[static_cast<std::size_t>(targetIndex)].metres) <
	        ArrivalDistance(system.bodies[static_cast<std::size_t>(targetIndex)]) + kStationReach)
		return refuse(Reason::AlreadyThere, {Jettison::Sim::ReasonArgument::Identifier(command.body)});
	if (targetIndex < 0 && glm::distance(from, aim.place) < kShipStandoff + kStationReach)
		return refuse(Reason::AlreadyThere);

	const auto *frame = registry.try_get<Jettison::Physics::LocalFrame>(ship);
	const auto *moving = registry.try_get<Jettison::Physics::Velocity>(ship);
	const Settlement settlement = SettleCruise(
	    system, *drive, from, frame ? std::string_view{frame->body} : std::string_view{},
	    moving ? glm::dvec3{moving->linear} : glm::dvec3{0.0}, aim, t0, world.FixedStep());
	if (!settlement.cruise)
		return refuse(settlement.refusal, {Jettison::Sim::ReasonArgument::Identifier(command.body)});
	const SettledCruise &settled = *settlement.cruise;
	const std::uint64_t steps = settled.steps;

	Cruising cruising;
	cruising.target = aim.body;
	cruising.frame =
	    targetIndex >= 0
	        ? system.bodies[static_cast<std::size_t>(ArrivalFrame(system, targetIndex))].name
	        : aim.frame;
	cruising.targetShip = targetShip;
	cruising.carrier = settled.carrier;
	cruising.course = settled.course;
	cruising.departureStep = world.Clock().Step();
	cruising.arrivalStep = cruising.departureStep + steps;
	cruising.body = Jettison::Transit::TakeOut(registry, ship);
	cruising.profile = drive->profile;
	cruising.departureClearance = settled.departureClearance;
	cruising.arrivalClearance = settled.arrivalClearance;
	cruising.crossingAcceleration = drive->crossingAcceleration;
	cruising.departureVelocity = settled.departureVelocity;
	cruising.arrivalVelocity = settled.arrivalVelocity;
	registry.remove<CameOut>(ship);

	world.Events().Emit(
	    CruiseStarted{
	        ship, aim.body, static_cast<double>(steps) * world.FixedStep(), settled.Metres(),
	        cruising.arrivalStep});
	registry.emplace_or_replace<Cruising>(ship, std::move(cruising));
}


void CruiseTravel::BreakOff(Jettison::Sim::SimWorld &world, const BreakOffCruise &command) const
{
	entt::registry &registry = world.Registry();
	const auto refuse = [&](Reason reason)
	{
		world.Events().Emit(
		    CruiseRefused{command.ship, std::string{}, Jettison::Sim::MakeReason(reason, {})});
	};
	if (!registry.valid(command.ship))
		return refuse(Reason::NoSuchEntity);
	if (!Jettison::Sim::MayCommand(registry, command.ship, command.sender))
		return refuse(Reason::NotController);
	const auto *inSystem = registry.try_get<Jettison::Sim::InFrame>(command.ship);
	const auto found = inSystem ? m_systems.find(inSystem->frame) : m_systems.end();
	if (!registry.all_of<Cruising>(command.ship) || found == m_systems.end())
		return refuse(Reason::NotCruising);
	Leave(world, command.ship, found->second);
}


void CruiseTravel::Leave(
    Jettison::Sim::SimWorld &world, entt::entity ship, const Space::StarSystem &system) const
{
	entt::registry &registry = world.Registry();
	const Cruising cruising = registry.get<const Cruising>(ship);

	// Moving as its cruise had it over the last step, relative to its carrier, whose frame it comes
	// out in; still in the step it set out in, as it left.
	const int carrier = system.Find(cruising.carrier);
	const double step = world.FixedStep();
	const std::uint64_t ending = world.Clock().Step();
	const auto relative = [&](std::uint64_t at, double time)
	{
		const glm::dvec3 origin =
		    carrier < 0 ? glm::dvec3{0.0}
		                : Space::EvaluateBody(system, static_cast<std::size_t>(carrier), time).metres;
		return Underway(cruising, origin, at, step).position - origin;
	};
	glm::dvec3 velocity = cruising.departureVelocity;
	if (ending > cruising.departureStep)
		velocity =
		    (relative(ending, world.Clock().Now()) - relative(ending - 1, world.Clock().Now() - step)) /
		    step;
	Jettison::Transit::BringOut(
	    registry, ship, registry.get<const Jettison::Sim::Transform>(ship), cruising.body,
	    cruising.carrier);
	registry.get<Jettison::Physics::Velocity>(ship).linear = glm::vec3{velocity};
	registry.remove<Cruising>(ship);
	world.Events().Emit(CruiseBrokenOff{ship, cruising.target});
}


glm::dvec3 CruiseTravel::FrameOrigin(std::uint64_t system, const std::string &frame, double time) const
{
	return frame.empty() ? glm::dvec3{0.0} : m_physics.FrameOrigin(system, frame, time);
}


std::vector<entt::entity> CruiseTravel::Waiting(
    const Jettison::Sim::SimWorld &world, double endsAt) const
{
	const entt::registry &registry = world.Registry();
	std::vector<entt::entity> waiting;
	for (const auto [ship, cameOut, transform, inFrame, body] :
	     registry
	         .view<
	             const CameOut, const Jettison::Sim::Transform, const Jettison::Sim::InFrame,
	             const Jettison::Physics::Body>()
	         .each())
	{
		if (registry.any_of<Destination, Jettison::Physics::OnRails, Cruising>(ship) ||
		    !m_systems.contains(inFrame.frame))
			continue;
		const auto *frame = registry.try_get<Jettison::Physics::LocalFrame>(ship);
		const std::string local = frame ? frame->body : std::string{};
		if (local == cameOut.frame && !body.sensor &&
		    glm::distance(
		        transform.position - FrameOrigin(inFrame.frame, local, endsAt), cameOut.offset) <=
		        kStillThere)
			waiting.push_back(ship);
	}
	std::ranges::sort(waiting, {}, [](entt::entity ship) { return entt::to_integral(ship); });
	return waiting;
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
		const int carrier = starSystem.Find(cruising.carrier);
		const glm::dvec3 origin =
		    carrier < 0
		        ? glm::dvec3{0.0}
		        : Space::EvaluateBody(starSystem, static_cast<std::size_t>(carrier), endsAt).metres;
		registry.emplace_or_replace<Jettison::Sim::Transform>(
		    ship, Underway(cruising, origin, ending, world.FixedStep()));
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
	// the ships cruise in, and each arrival once it is placed; and the ships waiting where they came
	// out, which an arrival there moves off.
	std::optional<std::vector<Jettison::Transit::Occupant>> occupants;
	std::optional<std::vector<entt::entity>> waiting;
	for (const entt::entity ship : ships)
	{
		const Cruising &cruising = registry.get<const Cruising>(ship);
		const std::uint64_t system = registry.get<const Jettison::Sim::InFrame>(ship).frame;
		const auto found = m_systems.find(system);
		const int carrier = found == m_systems.end() ? -1 : found->second.Find(cruising.carrier);
		const int target = found == m_systems.end() ? -1 : found->second.Find(cruising.target);
		const bool toShip = cruising.targetShip != entt::null;
		if (found == m_systems.end() || (!toShip && target < 0) ||
		    (carrier < 0 && !cruising.carrier.empty()))
			throw std::invalid_argument{std::format(
			    "Ai: entity {:#x} cruises to '{}' by '{}' in a system {}", entt::to_integral(ship),
			    cruising.target, cruising.carrier,
			    found == m_systems.end() ? "not defined" : "without those bodies")};
		const Space::StarSystem &starSystem = found->second;
		const glm::dvec3 origin =
		    carrier < 0
		        ? glm::dvec3{0.0}
		        : Space::EvaluateBody(starSystem, static_cast<std::size_t>(carrier), endsAt).metres;

		// A ship cruised to is followed while it is placed in the system; once it is not, the cruise
		// is broken off where it is.
		std::optional<glm::dvec3> aimed;
		const Jettison::Sim::Transform *quarry = nullptr;
		if (toShip)
		{
			const entt::entity followed = cruising.targetShip;
			const auto *inFrame =
			    registry.valid(followed) ? registry.try_get<Jettison::Sim::InFrame>(followed) : nullptr;
			quarry = inFrame && inFrame->frame == system
			             ? registry.try_get<Jettison::Sim::Transform>(followed)
			             : nullptr;
			if (!quarry)
			{
				Leave(world, ship, starSystem);
				continue;
			}
			aimed = Aimed(cruising, quarry->position - origin);
		}

		if (ending < cruising.arrivalStep)
		{
			registry.emplace_or_replace<Jettison::Sim::Transform>(
			    ship, Underway(cruising, origin, ending, world.FixedStep(), aimed));
			continue;
		}

		if (!occupants)
		{
			std::vector<std::uint64_t> inSystems;
			inSystems.reserve(ships.size());
			for (const entt::entity other : ships)
				inSystems.push_back(registry.get<const Jettison::Sim::InFrame>(other).frame);
			occupants = Jettison::Transit::Occupants(registry, m_physics, inSystems);
			waiting = Waiting(world, endsAt);
		}
		const glm::dvec3 end = origin + (aimed ? *aimed : cruising.course.back());
		const glm::dvec3 ahead =
		    (quarry
		         ? quarry->position
		         : Space::EvaluateBody(starSystem, static_cast<std::size_t>(target), endsAt).metres) -
		    end;
		Jettison::Sim::Transform place{
		    end, Facing(glm::length(ahead) > 0.0 ? glm::normalize(ahead) : glm::dvec3{0.0, 0.0, -1.0})};
		const double radius = m_physics.ShapeRadius(cruising.body.shape);
		place.position = Jettison::Transit::ClearPlace(
		    *occupants, system, place, radius, LaneTravel::kArrivalClearance);
		occupants->push_back({system, place.position, radius});

		// Moving off: a ship waiting where it came out, in this one's place, is flown further from it.
		for (const entt::entity other : *waiting)
		{
			if (!registry.all_of<CameOut>(other) ||
			    registry.get<const Jettison::Sim::InFrame>(other).frame != system)
				continue;
			const glm::dvec3 at = registry.get<const Jettison::Sim::Transform>(other).position;
			const double reach =
			    radius +
			    m_physics.ShapeRadius(registry.get<const Jettison::Physics::Body>(other).shape) +
			    LaneTravel::kArrivalClearance;
			if (glm::distance(at, end) >= reach)
				continue;
			const glm::dvec3 away = at - place.position;
			const glm::dvec3 off = glm::length(away) > 0.0
			                           ? glm::normalize(away)
			                           : place.orientation * glm::dvec3{1.0, 0.0, 0.0};
			const auto *otherFrame = registry.try_get<Jettison::Physics::LocalFrame>(other);
			Destination clear;
			clear.offset = at + (off * kMoveOffDistance) -
			               FrameOrigin(system, otherFrame ? otherFrame->body : std::string{}, endsAt);
			registry.emplace_or_replace<Destination>(other, std::move(clear));
			registry.remove<CameOut>(other);
		}

		// In the frame it was cruised to, or a ship's frame now, moving as that ship does in it.
		std::string frame = cruising.frame;
		glm::vec3 velocity{0.0f};
		if (quarry)
		{
			const auto *quarryFrame =
			    registry.try_get<Jettison::Physics::LocalFrame>(cruising.targetShip);
			frame = quarryFrame ? quarryFrame->body : std::string{};
			if (const auto *moving = registry.try_get<Jettison::Physics::Velocity>(cruising.targetShip))
				velocity = moving->linear;
		}
		Jettison::Transit::BringOut(registry, ship, place, cruising.body, frame);
		registry.get<Jettison::Physics::Velocity>(ship).linear = velocity;
		registry.emplace_or_replace<CameOut>(
		    ship, CameOut{frame, place.position - FrameOrigin(system, frame, endsAt)});
		const std::string body = cruising.target;
		registry.remove<Cruising>(ship);
		world.Events().Emit(CruiseEnded{ship, body});
	}
}
} // namespace Manifest::Space
