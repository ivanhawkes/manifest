#include <manifest/trade/Economy.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Manifest::Trade
{
namespace
{
// The bits of a double's mantissa after its leading one, and the points of its grid in one binary
// exponent's range.
constexpr int kMantissaBits = std::numeric_limits<double>::digits - 1;
constexpr double kGridPoints = 0x1p53;
} // namespace

double WorkRate(double health, double happiness, double floor)
{
	const auto factor = [floor](double condition)
	{ return floor + (1.0 - floor) * std::clamp(condition, 0.0, 100.0) / 100.0; };
	return factor(health) * factor(happiness);
}


double Severity(std::span<const NeedMet> needs)
{
	double weights = 0.0;
	double unmet = 0.0;
	for (const NeedMet &need : needs)
	{
		if (need.required <= 0.0)
			continue;
		weights += need.weight;
		unmet += need.weight * (1.0 - std::min(1.0, need.satisfied / need.required));
	}
	return weights > 0.0 ? unmet / weights : 0.0;
}


Condition Updated(
    Condition condition, double severity, double hours, const Content::ConditionTuning &tuning)
{
	if (severity > 0.0)
	{
		condition.health -= tuning.healthLossPerHour * severity * hours;
		condition.happiness -= tuning.happinessLossPerHour * severity * hours;
	}
	else
	{
		condition.health += tuning.healthRecoveryPerHour * hours;
		condition.happiness += tuning.happinessRecoveryPerHour * hours;
	}
	condition.health = std::clamp(condition.health, 0.0, 100.0);
	condition.happiness = std::clamp(condition.happiness, 0.0, 100.0);
	return condition;
}


Withholding Withhold(std::int64_t gross, std::int64_t carry, std::int64_t perCredit)
{
	if (perCredit <= 0)
		return {gross, 0, 0};
	// The carry is below perCredit, so the withheld credits cannot pass gross; the sum may pass the
	// credits' range, so in 128 bits.
	const __int128 total = static_cast<__int128>(carry) + gross;
	const auto withheld = static_cast<std::int64_t>(total / perCredit);
	return {gross - withheld, withheld, static_cast<std::int64_t>(total % perCredit)};
}


std::int64_t Earned(std::int64_t wages, double progress, double duration)
{
	if (wages <= 0 || duration <= 0.0 || progress <= 0.0)
		return 0;
	if (progress >= duration)
		return wages;
	// Compared as a double first: near the credits' limit the product can round to 2^63.
	const double earned = std::floor(static_cast<double>(wages) * (progress / duration));
	return earned < static_cast<double>(wages) ? static_cast<std::int64_t>(earned) : wages;
}


// Within one binary exponent's range a double's values are a grid of one spacing, so adding the same
// work to a point of it moves it by the same whole number of spacings each time, the work rounded to
// the grid, unless the work is exactly half a spacing over a whole number of them, when the rounding
// to even depends on the point. Those additions are made at once; a tie, an addition leaving the
// range and a subnormal progress are made one at a time.
double Added(double progress, double work, std::uint64_t times)
{
	while (times > 0)
	{
		const double next = progress + work;
		// The work rounds away, and so it does every time after.
		if (next == progress)
			return progress;
		if (!std::isnormal(progress))
		{
			progress = next;
			--times;
			continue;
		}
		const int exponent = std::ilogb(progress);
		const double spacing = std::ldexp(1.0, exponent - kMantissaBits);
		const double step = next - progress;
		if (next >= std::ldexp(1.0, exponent + 1) || 2.0 * std::abs(work - step) == spacing)
		{
			progress = next;
			--times;
			continue;
		}
		// In spacings: the point, the step and how many steps stay in the range.
		const double units = progress / spacing;
		const double stride = step / spacing;
		auto steps = static_cast<std::uint64_t>(std::floor((kGridPoints - 1.0 - units) / stride));
		steps = std::min(steps, times);
		while (steps > 0 && units + static_cast<double>(steps) * stride > kGridPoints - 1.0)
			--steps;
		if (steps == 0)
		{
			progress = next;
			--times;
			continue;
		}
		progress = (units + static_cast<double>(steps) * stride) * spacing;
		times -= steps;
	}
	return progress;
}
} // namespace Manifest::Trade
