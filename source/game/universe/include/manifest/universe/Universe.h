#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>


namespace Manifest::Core
{
// Name pools for the generated solar systems: the Manifest.NamePool definitions, authored in
// data/universe/solar-system-names.yaml, one pool of names (latin, greek, ...) to each key.
//
// The pools are stored in a map keyed by pool name so iteration order is the same on
// every platform: the simulation draws names from a seeded generator over this order.
class UniverseNamePools
{
  public:
	// Replaces any existing pool of the same name.
	void AddPool(const std::string &poolName, std::vector<std::string> names);

	// Names in a pool; empty when the pool does not exist.
	const std::vector<std::string> &Find(const std::string &poolName) const;

	// Pool names in sorted order.
	std::vector<std::string> PoolNames() const;

	std::size_t PoolCount() const;

	// Total names across every pool.
	std::size_t NameCount() const;

	bool Empty() const;

  private:
	std::map<std::string, std::vector<std::string>> m_pools;
};
} // namespace Manifest::Core
