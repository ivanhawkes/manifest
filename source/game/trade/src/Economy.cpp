#include <manifest/trade/Economy.h>

#include <algorithm>
#include <cmath>

namespace Manifest::Trade
{

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
	// The carry is below perCredit, so the sum cannot pass gross by more than it.
	const std::int64_t total = carry + gross;
	return {gross - total / perCredit, total / perCredit, total % perCredit};
}


std::int64_t Earned(std::int64_t wages, double progress, double duration)
{
	if (wages <= 0 || duration <= 0.0 || progress <= 0.0)
		return 0;
	if (progress >= duration)
		return wages;
	return std::min(
	    wages,
	    static_cast<std::int64_t>(std::floor(static_cast<double>(wages) * (progress / duration))));
}
} // namespace Manifest::Trade
