#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <manifest/content/Definitions.h>

namespace Manifest::Trade
{
// The regional economy (W04), the step's second system (docs/stage-4-trading-plan.md, "The step, in
// order"), which Register adds. Each step every region's employer works its batches at its region's
// work rate, as its condition was at the end of the step before; on the last step of each economic
// turn it pays the wages earned, sells what it has made to the venue's broker at its bid, its
// households buy and consume their essentials and then, every essential met, their optional goods,
// and the region's health and happiness move with the interval's shortage.
inline constexpr std::string_view kEconomy = "manifest.economy";

// The share of its full rate a region's production works at: each condition's factor runs from the
// floor at 0 to 1 at 100, and the two multiply (docs/economy-and-development-plan.md, "Population
// health, happiness and production").
double WorkRate(double health, double happiness, double floor);

// One need's interval: its weight, what it required and what was consumed, in its unit.
struct NeedMet
{
	double weight{0.0};
	double required{0.0};
	double satisfied{0.0};
};

// The weighted share of the required needs gone unmet, each need's fulfilment capped at all of it so
// that a surplus of one cannot make up for another: 0 to 1, and 0 when nothing is required.
double Severity(std::span<const NeedMet> needs);

struct Condition
{
	double health{0.0};
	double happiness{0.0};
};

// The condition after an interval of the hours given: a loss scaled by a severity above 0, otherwise
// the recovery, each held to [0, 100].
Condition Updated(
    Condition condition, double severity, double hours, const Content::ConditionTuning &tuning);

// A gross wage payment split between the households and the treasury: a credit withheld for each whole
// perCredit of gross paid, the remainder carried to the next payment by the same employer, so that ten
// one-credit payments withhold what one ten-credit payment does. A perCredit of 0 withholds nothing.
struct Withholding
{
	std::int64_t households{0};
	std::int64_t treasury{0};
	std::int64_t carry{0};
};

Withholding Withhold(std::int64_t gross, std::int64_t carry, std::int64_t perCredit);

// The whole credits of a batch's gross wage bill its progress has earned, rounded down.
std::int64_t Earned(std::int64_t wages, double progress, double duration);

// The progress after the work, at least 0, is added to it the times given one addition at a time, as
// each step adds it: to the bit, the additions within a binary exponent's range worked out at once.
double Added(double progress, double work, std::uint64_t times);
} // namespace Manifest::Trade
