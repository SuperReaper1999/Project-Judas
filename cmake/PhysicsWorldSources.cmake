# One world coordinator and cohesive private implementation modules. Direct
# headless fixtures use this same list as the engine; none may compile only a
# partial world implementation.
set(JUDAS_PHYSICS_WORLD_SOURCES
    src/PhysicsWorld.cpp
    src/physics/WorldBodies.cpp
    src/physics/WorldBounds.cpp
    src/physics/WorldCCDSearch.cpp
    src/physics/WorldCCDDispatch.cpp
    src/physics/WorldContacts.cpp
    src/physics/WorldIncrementalImpact.cpp
    src/physics/WorldMk2Solver.cpp
    src/physics/WorldImpactEvents.cpp
    src/physics/WorldJoints.cpp
    src/physics/WorldParticles.cpp
    src/physics/WorldPersistence.cpp
    src/physics/WorldQueries.cpp
    src/physics/WorldSleep.cpp
)
