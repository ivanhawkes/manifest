#include "TestHelpers.h"

#include <string>
#include <vector>

#include <manifest/universe/Universe.h>


TEST_CASE("pools_are_held_in_sorted_order")
{
	Manifest::Core::UniverseNamePools pools;
	pools.AddPool("latin", {"Aaron", "Bolivar"});
	pools.AddPool("greek", {"Andreas", "Bessas"});

	CHECK_EQ(pools.PoolCount(), std::size_t(2));
	CHECK_EQ(pools.NameCount(), std::size_t(4));
	CHECK(!pools.Empty());

	const std::vector<std::string> names = pools.PoolNames();
	CHECK_EQ(names.size(), std::size_t(2));
	CHECK_EQ(names[0], std::string("greek"));
	CHECK_EQ(names[1], std::string("latin"));

	CHECK_EQ(pools.Find("latin").size(), std::size_t(2));
	CHECK_EQ(pools.Find("latin")[1], std::string("Bolivar"));

	// An unknown pool is empty rather than an error.
	CHECK(pools.Find("coptic").empty());
}


TEST_CASE("adding_a_pool_again_replaces_it")
{
	Manifest::Core::UniverseNamePools pools;
	pools.AddPool("latin", {"Aaron", "Abigail"});
	pools.AddPool("latin", {"Aaron", "Abigail"});
	CHECK_EQ(pools.NameCount(), std::size_t(2));
}
