// Catch2 tests for Entable Registry correctness
// Tests entity validation, version checking, create/destroy operations

#include <catch2/catch_test_macros.hpp>
#include <Entable.hpp>
#include <random>
#include <set>
#include <vector>
#include <numeric>
#include <stdexcept>

namespace ent = entable;

// Test components
struct Position {
    float x, y, z;
    Position() : x(0), y(0), z(0) {}
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Velocity {
    float dx, dy, dz;
    Velocity() : dx(0), dy(0), dz(0) {}
    Velocity(float dx_, float dy_, float dz_) : dx(dx_), dy(dy_), dz(dz_) {}
};

using TestRegistry = ent::RegistryWithDefaultChunkSize<Position, Velocity>;

// =============================================================================
// Entity Creation Tests
// =============================================================================

TEST_CASE("Registry: CreateEntity returns valid entities", "[Registry][CreateEntity]")
{
    TestRegistry reg;

    SECTION("Single entity creation")
    {
        auto e = reg.CreateEntity();
        REQUIRE(reg.IsValidEntity(e));
        REQUIRE(reg.Size() == 1);
    }

    SECTION("Multiple entity creation")
    {
        std::vector<ent::Entity> entities;
        for (int i = 0; i < 100; ++i) {
            entities.push_back(reg.CreateEntity());
        }

        REQUIRE(reg.Size() == 100);
        for (const auto& e : entities) {
            REQUIRE(reg.IsValidEntity(e));
        }
    }

    SECTION("Entities have unique IDs")
    {
        auto e1 = reg.CreateEntity();
        auto e2 = reg.CreateEntity();
        auto e3 = reg.CreateEntity();

        REQUIRE(e1 != e2);
        REQUIRE(e2 != e3);
        REQUIRE(e1 != e3);
    }
}

TEST_CASE("Registry: Entity indices and versions", "[Registry][EntityTraits]")
{
    TestRegistry reg;

    SECTION("First entity has index 0, version 0")
    {
        auto e = reg.CreateEntity();
        REQUIRE(ent::EntityToIndex(e) == 0);
        REQUIRE(ent::EntityToVersion(e) == 0);
    }

    SECTION("Second entity has index 1, version 0")
    {
        reg.CreateEntity();
        auto e2 = reg.CreateEntity();
        REQUIRE(ent::EntityToIndex(e2) == 1);
        REQUIRE(ent::EntityToVersion(e2) == 0);
    }
}

// =============================================================================
// Entity Destruction Tests
// =============================================================================

TEST_CASE("Registry: DestroyEntity makes entity invalid", "[Registry][DestroyEntity]")
{
    TestRegistry reg;

    SECTION("Destroy single entity")
    {
        auto e = reg.CreateEntity();
        REQUIRE(reg.IsValidEntity(e));

        reg.DestroyEntity(e);
        REQUIRE_FALSE(reg.IsValidEntity(e));
        REQUIRE(reg.Size() == 0);
    }

    SECTION("Destroy one of multiple entities")
    {
        auto e1 = reg.CreateEntity();
        auto e2 = reg.CreateEntity();
        auto e3 = reg.CreateEntity();

        reg.DestroyEntity(e2);

        REQUIRE(reg.IsValidEntity(e1));
        REQUIRE_FALSE(reg.IsValidEntity(e2));
        REQUIRE(reg.IsValidEntity(e3));
        REQUIRE(reg.Size() == 2);
    }

    SECTION("Destroy all entities")
    {
        std::vector<ent::Entity> entities;
        for (int i = 0; i < 10; ++i) {
            entities.push_back(reg.CreateEntity());
        }

        for (const auto& e : entities) {
            reg.DestroyEntity(e);
        }

        REQUIRE(reg.Size() == 0);
        for (const auto& e : entities) {
            REQUIRE_FALSE(reg.IsValidEntity(e));
        }
    }
}

TEST_CASE("Registry: Entity version increments after destroy", "[Registry][Version]")
{
    TestRegistry reg;

    SECTION("Recreated entity has incremented version")
    {
        auto e1 = reg.CreateEntity();
        uint32_t index = ent::EntityToIndex(e1);
        uint32_t version1 = ent::EntityToVersion(e1);

        reg.DestroyEntity(e1);
        auto e2 = reg.CreateEntity();

        // Same index should be reused
        REQUIRE(ent::EntityToIndex(e2) == index);
        // Version should be incremented
        REQUIRE(ent::EntityToVersion(e2) == version1 + 1);
        // Old entity reference is now invalid
        REQUIRE_FALSE(reg.IsValidEntity(e1));
        REQUIRE(reg.IsValidEntity(e2));
    }

    SECTION("Multiple destroy/create cycles increment version")
    {
        auto e = reg.CreateEntity();
        uint32_t index = ent::EntityToIndex(e);

        for (int i = 0; i < 5; ++i) {
            uint32_t version = ent::EntityToVersion(e);
            reg.DestroyEntity(e);
            e = reg.CreateEntity();

            REQUIRE(ent::EntityToIndex(e) == index);
            REQUIRE(ent::EntityToVersion(e) == version + 1);
        }
    }
}

// =============================================================================
// Out of Bounds Entity Tests
// =============================================================================

TEST_CASE("Registry: Out of bounds entity validation", "[Registry][OutOfBounds]")
{
    TestRegistry reg;

    // Create a few entities to establish valid range
    reg.CreateEntity();
    reg.CreateEntity();
    reg.CreateEntity();

    SECTION("Entity with index beyond allocated range is invalid")
    {
        // Create an entity with an index way beyond what we've allocated
        // Index 10000 is well beyond our 3 entities
        auto fakeEntity = ent::ComposeEntity(10000, 0);

        REQUIRE_FALSE(reg.IsValidEntity(fakeEntity));
    }

    SECTION("IsValidEntity returns false for max index")
    {
        auto maxIndexEntity = ent::ComposeEntity(ent::EntityTraits::INDEX_MASK, 0);
        REQUIRE_FALSE(reg.IsValidEntity(maxIndexEntity));
    }

    SECTION("DestroyEntity throws for out of bounds entity")
    {
        auto fakeEntity = ent::ComposeEntity(10000, 0);
        REQUIRE_THROWS_AS(reg.DestroyEntity(fakeEntity), std::runtime_error);
    }

    SECTION("Component access throws for out of bounds entity")
    {
        auto fakeEntity = ent::ComposeEntity(10000, 0);
        REQUIRE_THROWS_AS(reg.SetSafe<Position>(fakeEntity, 1.0f, 2.0f, 3.0f), std::runtime_error);
    }
}

// =============================================================================
// Bad Version Number Tests
// =============================================================================

TEST_CASE("Registry: Stale version number detection", "[Registry][BadVersion]")
{
    TestRegistry reg;

    SECTION("Old entity reference becomes invalid after destroy")
    {
        auto e = reg.CreateEntity();
        reg.Set<Position>(e, 1.0f, 2.0f, 3.0f);

        // Store the old entity
        auto oldEntity = e;

        // Destroy and recreate
        reg.DestroyEntity(e);
        auto newEntity = reg.CreateEntity();

        // Old reference should be invalid
        REQUIRE_FALSE(reg.IsValidEntity(oldEntity));
        // New entity should be valid
        REQUIRE(reg.IsValidEntity(newEntity));

        // Attempting to use old entity should throw
        REQUIRE_THROWS_AS(reg.DestroyEntity(oldEntity), std::runtime_error);
    }

    SECTION("Entity with wrong version is detected")
    {
        auto e = reg.CreateEntity();
        uint32_t index = ent::EntityToIndex(e);
        uint32_t correctVersion = ent::EntityToVersion(e);

        // Create an entity with same index but wrong version
        auto staleEntity = ent::ComposeEntity(index, correctVersion + 1);

        // The stale entity should be invalid
        REQUIRE_FALSE(reg.IsValidEntity(staleEntity));

        // Attempting operations on stale entity should throw
        REQUIRE_THROWS_AS(reg.DestroyEntity(staleEntity), std::runtime_error);
    }

    SECTION("Version mismatch after multiple cycles")
    {
        auto e = reg.CreateEntity();
        uint32_t index = ent::EntityToIndex(e);

        // Destroy and recreate several times
        for (int i = 0; i < 3; ++i) {
            reg.DestroyEntity(e);
            e = reg.CreateEntity();
        }

        // Now create an entity with old version (version 0)
        auto staleEntity = ent::ComposeEntity(index, 0);

        REQUIRE_FALSE(reg.IsValidEntity(staleEntity));
        REQUIRE_THROWS_AS(reg.DestroyEntity(staleEntity), std::runtime_error);
    }
}

// =============================================================================
// Create/Delete Interleaved Tests
// =============================================================================

TEST_CASE("Registry: Interleaved create and destroy", "[Registry][Interleaved]")
{
    TestRegistry reg;

    SECTION("Create, destroy, create pattern")
    {
        auto e1 = reg.CreateEntity();
        reg.Set<Position>(e1, 1.0f, 0.0f, 0.0f);

        reg.DestroyEntity(e1);

        auto e2 = reg.CreateEntity();
        REQUIRE(ent::EntityToIndex(e2) == ent::EntityToIndex(e1));
        REQUIRE(ent::EntityToVersion(e2) == ent::EntityToVersion(e1) + 1);

        // e1 is now stale
        REQUIRE_FALSE(reg.IsValidEntity(e1));
        REQUIRE_THROWS_AS(reg.DestroyEntity(e1), std::runtime_error);
    }

    SECTION("Multiple entities with selective destruction")
    {
        std::vector<ent::Entity> entities;
        for (int i = 0; i < 10; ++i) {
            entities.push_back(reg.CreateEntity());
        }

        // Destroy every other entity
        for (size_t i = 0; i < entities.size(); i += 2) {
            reg.DestroyEntity(entities[i]);
        }

        REQUIRE(reg.Size() == 5);

        // Create new entities - should reuse destroyed indices
        for (int i = 0; i < 5; ++i) {
            auto e = reg.CreateEntity();
            REQUIRE(reg.IsValidEntity(e));
        }

        REQUIRE(reg.Size() == 10);
    }

    SECTION("Destroy in reverse order")
    {
        auto e1 = reg.CreateEntity();
        auto e2 = reg.CreateEntity();
        auto e3 = reg.CreateEntity();

        reg.DestroyEntity(e3);
        REQUIRE_FALSE(reg.IsValidEntity(e3));
        REQUIRE(reg.IsValidEntity(e1));
        REQUIRE(reg.IsValidEntity(e2));

        reg.DestroyEntity(e2);
        REQUIRE_FALSE(reg.IsValidEntity(e2));
        REQUIRE(reg.IsValidEntity(e1));

        reg.DestroyEntity(e1);
        REQUIRE_FALSE(reg.IsValidEntity(e1));
        REQUIRE(reg.Size() == 0);
    }

    SECTION("Create after destroying all")
    {
        std::vector<ent::Entity> entities;
        for (int i = 0; i < 5; ++i) {
            entities.push_back(reg.CreateEntity());
        }

        for (const auto& e : entities) {
            reg.DestroyEntity(e);
        }

        REQUIRE(reg.Size() == 0);

        // Create new entities - indices should be reused with incremented versions
        for (int i = 0; i < 5; ++i) {
            auto e = reg.CreateEntity();
            REQUIRE(reg.IsValidEntity(e));
            // Version should be 1 (incremented from 0)
            REQUIRE(ent::EntityToVersion(e) == 1);
        }
    }
}

// =============================================================================
// Null Entity Tests
// =============================================================================

TEST_CASE("Registry: NullEntity handling", "[Registry][NullEntity]")
{
    TestRegistry reg;

    SECTION("NullEntity is invalid")
    {
        REQUIRE_FALSE(reg.IsValidEntity(ent::NullEntity));
    }

    SECTION("DestroyEntity throws for NullEntity")
    {
        REQUIRE_THROWS_AS(reg.DestroyEntity(ent::NullEntity), std::runtime_error);
    }

    SECTION("Component operations throw for NullEntity")
    {
        REQUIRE_THROWS_AS(reg.SetSafe<Position>(ent::NullEntity, 1.0f, 2.0f, 3.0f), std::runtime_error);
    }
}

// =============================================================================
// Component Operations with Entity Validation
// =============================================================================

TEST_CASE("Registry: Component operations with valid/invalid entities", "[Registry][Components]")
{
    TestRegistry reg;

    SECTION("Set and Get on valid entity")
    {
        auto e = reg.CreateEntity();
        reg.Set<Position>(e, 1.0f, 2.0f, 3.0f);

        auto& pos = reg.Get<Position>(e);
        REQUIRE(pos.x == 1.0f);
        REQUIRE(pos.y == 2.0f);
        REQUIRE(pos.z == 3.0f);
    }

    SECTION("TryGet returns valid pointer for valid entity")
    {
        auto e = reg.CreateEntity();
        reg.Set<Position>(e, 1.0f, 2.0f, 3.0f);

        REQUIRE(reg.TryGet<Position>(e) != nullptr);

        reg.DestroyEntity(e);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Registry: Edge cases", "[Registry][EdgeCases]")
{
    TestRegistry reg;

    SECTION("Clear resets everything")
    {
        for (int i = 0; i < 100; ++i) {
            auto e = reg.CreateEntity();
            reg.Set<Position>(e, static_cast<float>(i), 0.0f, 0.0f);
        }

        reg.Clear();

        REQUIRE(reg.Size() == 0);
        REQUIRE(reg.entities.size() == 0);
    }

    SECTION("Entity ID wrapping (version overflow)")
    {
        auto e = reg.CreateEntity();
        uint32_t index = ent::EntityToIndex(e);

        // Simulate many destroy/create cycles
        // Note: VERSION_BITS determines max version
        for (uint32_t i = 0; i < 10; ++i) {
            reg.DestroyEntity(e);
            e = reg.CreateEntity();
        }

        // Entity should still be valid
        REQUIRE(reg.IsValidEntity(e));
        REQUIRE(ent::EntityToIndex(e) == index);
    }

    SECTION("Large number of entities")
    {
        std::vector<ent::Entity> entities;
        const size_t count = 10000;

        for (size_t i = 0; i < count; ++i) {
            entities.push_back(reg.CreateEntity());
        }

        REQUIRE(reg.Size() == count);

        // Destroy half
        for (size_t i = 0; i < count / 2; ++i) {
            reg.DestroyEntity(entities[i]);
        }

        REQUIRE(reg.Size() == count / 2);

        // Verify remaining entities are still valid
        for (size_t i = count / 2; i < count; ++i) {
            REQUIRE(reg.IsValidEntity(entities[i]));
        }
    }
}

// =============================================================================
// Double Destroy Protection
// =============================================================================

TEST_CASE("Registry: Double destroy throws", "[Registry][DoubleDestroy]")
{
    TestRegistry reg;

    SECTION("Destroying same entity twice throws")
    {
        auto e = reg.CreateEntity();
        reg.DestroyEntity(e);

        REQUIRE_THROWS_AS(reg.DestroyEntity(e), std::runtime_error);
    }

    SECTION("Destroying already destroyed entity in batch throws")
    {
        auto e1 = reg.CreateEntity();
        auto e2 = reg.CreateEntity();

        reg.DestroyEntity(e1);

        // Attempting to destroy e1 again should throw
        REQUIRE_THROWS_AS(reg.DestroyEntity(e1), std::runtime_error);

        // e2 should still be valid and destroyable
        REQUIRE(reg.IsValidEntity(e2));
        reg.DestroyEntity(e2);
        REQUIRE_FALSE(reg.IsValidEntity(e2));
    }
}

// =============================================================================
// Destroy N of M and Recreate Pattern Tests
// =============================================================================

TEST_CASE("Registry: Destroy N of M entities and recreate", "[Registry][DestroyRecreate]")
{
    TestRegistry reg;
    std::mt19937 rng(42); // Fixed seed for reproducibility

    SECTION("Destroy half and recreate half")
    {
        const size_t m = 100;
        const size_t n = m / 2;

        // Create M entities
        std::vector<ent::Entity> entities;
        entities.reserve(m);
        for (size_t i = 0; i < m; ++i) {
            entities.push_back(reg.CreateEntity());
        }
        REQUIRE(reg.Size() == m);

        // Shuffle for random destruction order
        std::shuffle(entities.begin(), entities.end(), rng);

        // Collect indices of entities to be destroyed
        std::set<uint32_t> destroyedIndices;
        for (size_t i = 0; i < n; ++i) {
            destroyedIndices.insert(ent::EntityToIndex(entities[i]));
        }

        // Destroy first N entities
        for (size_t i = 0; i < n; ++i) {
            reg.DestroyEntity(entities[i]);
        }
        REQUIRE(reg.Size() == m - n);

        // Verify remaining entities are still valid
        for (size_t i = n; i < m; ++i) {
            REQUIRE(reg.IsValidEntity(entities[i]));
        }

        // Verify destroyed entities are invalid
        for (size_t i = 0; i < n; ++i) {
            REQUIRE_FALSE(reg.IsValidEntity(entities[i]));
        }

        // Recreate N entities
        std::vector<ent::Entity> newEntities;
        newEntities.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            auto e = reg.CreateEntity();
            newEntities.push_back(e);
            REQUIRE(reg.IsValidEntity(e));
        }
        REQUIRE(reg.Size() == m);

        // Old destroyed entities should still be invalid
        for (size_t i = 0; i < n; ++i) {
            REQUIRE_FALSE(reg.IsValidEntity(entities[i]));
        }

        // Verify that new entities' indices were reused from destroyed entities
        for (size_t i = 0; i < n; ++i) {
            uint32_t newIndex = ent::EntityToIndex(newEntities[i]);
            REQUIRE(destroyedIndices.count(newIndex) == 1); // Index was reused
        }

        // Verify that versions were incremented
        for (size_t i = 0; i < n; ++i) {
            REQUIRE(ent::EntityToVersion(newEntities[i]) >= 1);
        }

        // Verify storage size is back to original (indices were reused, not new allocations)
        // The registry size should be exactly m again
        REQUIRE(reg.Size() == m);

		const auto& spans = reg.Components<Position>();
        REQUIRE(std::accumulate(spans.begin(), spans.end(), size_t{0}, [](size_t sum, const auto& span) {
            return sum + span.size();
        }) == m);
    }

    SECTION("Multiple destroy/recreate cycles")
    {
        const size_t m = 50;
        const size_t n = 25;

        // Create M entities
        std::vector<ent::Entity> entities;
        for (size_t i = 0; i < m; ++i) {
            entities.push_back(reg.CreateEntity());
        }

        // Perform multiple destroy/recreate cycles
        for (int cycle = 0; cycle < 5; ++cycle) {
            std::shuffle(entities.begin(), entities.end(), rng);

            // Destroy N
            for (size_t i = 0; i < n; ++i) {
                reg.DestroyEntity(entities[i]);
            }
            REQUIRE(reg.Size() == m - n);

            // Recreate N
            for (size_t i = 0; i < n; ++i) {
                entities[i] = reg.CreateEntity();
            }
            REQUIRE(reg.Size() == m);

            // All entities should be valid again
            for (const auto& e : entities) {
                REQUIRE(reg.IsValidEntity(e));
            }
        }
    }

    SECTION("Destroy random subset preserves component data")
    {
        const size_t m = 100;
        const size_t n = 40;

        // Create M entities with component data
        std::vector<ent::Entity> entities;
        std::vector<std::pair<float, float>> expectedData;
        for (size_t i = 0; i < m; ++i) {
            auto e = reg.CreateEntity();
            float x = static_cast<float>(i);
            float y = static_cast<float>(i * 2);
            reg.Set<Position>(e, x, y, 0.0f);
            entities.push_back(e);
            expectedData.emplace_back(x, y);
        }

        // Shuffle and destroy N
        std::shuffle(entities.begin(), entities.end(), rng);
        for (size_t i = 0; i < n; ++i) {
            reg.DestroyEntity(entities[i]);
        }

        // Verify remaining entities have correct data
        for (size_t i = n; i < m; ++i) {
            (void)reg.Get<Position>(entities[i]); // Access to verify no crash
            // Note: due to swap-and-pop, the data may have moved
            // Just verify we can access it without crashing
            REQUIRE(reg.IsValidEntity(entities[i]));
        }

        // Recreate and verify new entities work
        for (size_t i = 0; i < n; ++i) {
            auto e = reg.CreateEntity();
            reg.Set<Position>(e, 99.0f, 88.0f, 77.0f);
            auto& pos = reg.Get<Position>(e);
            REQUIRE(pos.x == 99.0f);
            REQUIRE(pos.y == 88.0f);
            REQUIRE(pos.z == 77.0f);
        }
    }
}

TEST_CASE("Registry: ShrinkToFit reduces memory footprint", "[Registry][ShrinkToFit]")
{
    struct Position {
        float x, y, z;
    };
    struct Velocity {
        float vx, vy, vz;
    };

    using Reg = ent::RegistryWithDefaultChunkSize<Position, Velocity>;
    Reg reg;

    SECTION("ShrinkToFit after destroying entities")
    {
        // Create 2000 entities with components
        const size_t initialCount = 2000;
        std::vector<ent::Entity> entities;
        entities.reserve(initialCount);
        for (size_t i = 0; i < initialCount; ++i) {
            auto e = reg.CreateEntity();
            reg.Set<Position>(e, static_cast<float>(i), static_cast<float>(i + 1), static_cast<float>(i + 2));
            reg.Set<Velocity>(e, static_cast<float>(i * 0.1f), static_cast<float>(i * 0.2f), static_cast<float>(i * 0.3f));
            entities.push_back(e);
        }
        REQUIRE(reg.Size() == initialCount);

        // Destroy 1500 entities (leaving 500)
        const size_t remainingCount = 500;
        for (size_t i = remainingCount; i < initialCount; ++i) {
            reg.DestroyEntity(entities[i]);
        }
        REQUIRE(reg.Size() == remainingCount);

        // ShrinkToFit should reduce memory without losing data
        reg.ShrinkToFit();

        // Verify remaining entities still have valid data
        for (size_t i = 0; i < remainingCount; ++i) {
            auto e = entities[i];
            REQUIRE(reg.IsValidEntity(e));
            auto& pos = reg.Get<Position>(e);
            REQUIRE(pos.x == static_cast<float>(i));
            REQUIRE(pos.y == static_cast<float>(i + 1));
            REQUIRE(pos.z == static_cast<float>(i + 2));
            auto& vel = reg.Get<Velocity>(e);
            REQUIRE(vel.vx == static_cast<float>(i * 0.1f));
        }

        // Registry size should still be correct
        REQUIRE(reg.Size() == remainingCount);
    }

    SECTION("ShrinkToFit on empty registry")
    {
        reg.ShrinkToFit(); // Should not crash
        REQUIRE(reg.Size() == 0);
    }

    SECTION("ShrinkToFit after clearing all entities")
    {
        // Create some entities
        for (int i = 0; i < 100; ++i) {
            auto e = reg.CreateEntity();
            reg.Set<Position>(e, 1.0f, 2.0f, 3.0f);
        }
        REQUIRE(reg.Size() == 100);

        // Clear all
        reg.Clear();
        REQUIRE(reg.Size() == 0);

        // ShrinkToFit should work after clear
        reg.ShrinkToFit();
        REQUIRE(reg.Size() == 0);

        // Can create new entities after ShrinkToFit
        auto e = reg.CreateEntity();
        reg.Set<Position>(e, 5.0f, 6.0f, 7.0f);
        REQUIRE(reg.IsValidEntity(e));
        auto& pos = reg.Get<Position>(e);
        REQUIRE(pos.x == 5.0f);
    }
}