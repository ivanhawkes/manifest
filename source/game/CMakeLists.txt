# Lowest tier first, as in Jettison's source/engine/CMakeLists.txt.
add_subdirectory(types)
add_subdirectory(config)
add_subdirectory(universe)
add_subdirectory(progression)
add_subdirectory(sim)
add_subdirectory(ecs)
add_subdirectory(effects)
add_subdirectory(market)
add_subdirectory(space)
add_subdirectory(content)
add_subdirectory(world)
add_subdirectory(command)
add_subdirectory(travel)
add_subdirectory(trade)
add_subdirectory(company)
add_subdirectory(policy)
add_subdirectory(query)

# The complete game model, analogous to Jettison::Engine. Retain the existing
# Manifest::Core consumer alias while exposing individual module targets too.
add_library(manifest_game INTERFACE)
add_library(Manifest::Game ALIAS manifest_game)
add_library(Manifest::Core ALIAS manifest_game)
target_link_libraries(manifest_game INTERFACE
    Manifest::Types Manifest::Config Manifest::Universe Manifest::Progression
    Manifest::Sim Manifest::Ecs Manifest::Effects Manifest::Market Manifest::Space Manifest::Content Manifest::World Manifest::Command Manifest::Travel Manifest::Company Manifest::Trade Manifest::Policy Manifest::Query)

add_subdirectory(host)

add_subdirectory(remote)
