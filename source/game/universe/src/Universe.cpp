#include <manifest/universe/Universe.h>

#include <string>
#include <utility>


namespace Manifest::Core
{
void UniverseNamePools::AddPool(const std::string &poolName, std::vector<std::string> names)
{
	m_pools[poolName] = std::move(names);
}


const std::vector<std::string> &UniverseNamePools::Find(const std::string &poolName) const
{
	static const std::vector<std::string> noNames;

	const auto entry = m_pools.find(poolName);

	return entry != m_pools.end() ? entry->second : noNames;
}


std::vector<std::string> UniverseNamePools::PoolNames() const
{
	std::vector<std::string> names;
	names.reserve(m_pools.size());

	for (const auto &entry : m_pools)
	{
		names.push_back(entry.first);
	}

	return names;
}


std::size_t UniverseNamePools::PoolCount() const
{
	return m_pools.size();
}


std::size_t UniverseNamePools::NameCount() const
{
	std::size_t total = 0;

	for (const auto &entry : m_pools)
	{
		total += entry.second.size();
	}

	return total;
}


bool UniverseNamePools::Empty() const
{
	return m_pools.empty();
}
} // namespace Manifest::Core
