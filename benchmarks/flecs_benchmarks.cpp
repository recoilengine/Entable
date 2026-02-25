// Flecs vs Entable ECS Benchmark
// Compares entity creation, component addition, iteration, and deletion performance

#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <cstddef>

// Flecs includes
#define FLECS_IMPLEMENTATION
#include <flecs.h>

// Provide a default UserConfig so Entable trait probes are well-formed for local benchmark types.
namespace entable {
    template <typename T>
    struct UserConfig {};
}

#include <Entable.hpp>

namespace ent = entable;

// ============================================================================
// Component definitions - 60 bytes each (7 x 8-byte doubles = 56 bytes)
// ============================================================================

// Entable components (compile-time definition)
struct EC1 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct EC2 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct EC3 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct EC4 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct EC5 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct EC6 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct EC7 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct EC8 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };

// Verify component sizes (should be ~56 bytes, likely padded to 64)
static_assert(sizeof(EC1) >= 56 && sizeof(EC1) <= 64, "EC1 should be ~56-64 bytes");
static_assert(sizeof(EC8) >= 56 && sizeof(EC8) <= 64, "EC8 should be ~56-64 bytes");

// Entable registry with 8 components (compile-time)
using EntableRegistry8 = ent::Registry<ent::type_list_t<EC1, EC2, EC3, EC4, EC5, EC6, EC7, EC8>>;

// Entable registry with 4 components
using EntableRegistry4 = ent::Registry<ent::type_list_t<EC1, EC2, EC3, EC4>>;

// Entable registry with 2 components
using EntableRegistry2 = ent::Registry<ent::type_list_t<EC1, EC2>>;

// Entable registry with 1 component
using EntableRegistry1 = ent::Registry<ent::type_list_t<EC1>>;

// ============================================================================
// Flecs components (runtime definition) - structs must be plain data
// ============================================================================

struct FC1 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct FC2 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct FC3 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct FC4 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct FC5 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct FC6 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct FC7 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };
struct FC8 { double a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0; };

// ============================================================================
// Entity Creation Benchmarks
// ============================================================================

// Entable: Entity creation (with all 8 components pre-allocated)
static void BM_Entable_CreateEntities_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        EntableRegistry8 reg;
        for (size_t i = 0; i < n; ++i) {
            benchmark::DoNotOptimize(reg.CreateEntity());
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_CreateEntities_8Components)->Range(1024, 65536);

// Flecs: Entity creation with 8 components (runtime)
// Note: Components are automatically registered on first use
static void BM_Flecs_CreateEntities_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        flecs::world world;
        
        for (size_t i = 0; i < n; ++i) {
            auto e = world.entity();
            e.add<FC1>();
            e.add<FC2>();
            e.add<FC3>();
            e.add<FC4>();
            e.add<FC5>();
            e.add<FC6>();
            e.add<FC7>();
            e.add<FC8>();
            benchmark::DoNotOptimize(e);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_CreateEntities_8Components)->Range(1024, 65536);

// ============================================================================
// Component Addition Benchmarks (Setting components on existing entities)
// ============================================================================

// Entable: Set components on existing entities (already pre-allocated)
static void BM_Entable_SetComponents_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry8 reg;
    std::vector<ent::Entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        entities.push_back(reg.CreateEntity());
    }
    
    const EC1 c1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const EC2 c2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const EC3 c3{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const EC4 c4{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const EC5 c5{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const EC6 c6{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const EC7 c7{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const EC8 c8{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            reg.Set<EC1>(entities[i], c1);
            reg.Set<EC2>(entities[i], c2);
            reg.Set<EC3>(entities[i], c3);
            reg.Set<EC4>(entities[i], c4);
            reg.Set<EC5>(entities[i], c5);
            reg.Set<EC6>(entities[i], c6);
            reg.Set<EC7>(entities[i], c7);
            reg.Set<EC8>(entities[i], c8);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n * 8);
}
BENCHMARK(BM_Entable_SetComponents_8Components)->Range(1024, 65536);

// Flecs: Add components to entities (runtime component addition)
// Note: Components are registered automatically on first set(), but we register upfront
// to measure pure component addition performance (similar to Entable's compile-time setup)
static void BM_Flecs_SetComponents_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    // Pre-register components once (not timed - happens automatically on first use in real code)
    world.component<FC1>();
    world.component<FC2>();
    world.component<FC3>();
    world.component<FC4>();
    world.component<FC5>();
    world.component<FC6>();
    world.component<FC7>();
    world.component<FC8>();
    
    std::vector<flecs::entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        entities.push_back(world.entity());
    }
    
    const FC1 c1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const FC2 c2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const FC3 c3{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const FC4 c4{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const FC5 c5{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const FC6 c6{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const FC7 c7{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const FC8 c8{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            entities[i].set<FC1>(c1);
            entities[i].set<FC2>(c2);
            entities[i].set<FC3>(c3);
            entities[i].set<FC4>(c4);
            entities[i].set<FC5>(c5);
            entities[i].set<FC6>(c6);
            entities[i].set<FC7>(c7);
            entities[i].set<FC8>(c8);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n * 8);
}
BENCHMARK(BM_Flecs_SetComponents_8Components)->Range(1024, 65536);

// ============================================================================
// Sequential Iteration Benchmarks - 1 Component
// ============================================================================

// Entable: Sequential iteration - 1 component
static void BM_Entable_Iterate_1Component(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry1 reg;
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    for (auto _ : state) {
        reg.Each<EC1>([&](EC1& c1) {
            sum += c1.a + c1.b + c1.c + c1.d + c1.e + c1.f + c1.g;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_Iterate_1Component)->Range(1024, 65536);

// Flecs: Sequential iteration - 1 component (using each with single template param)
static void BM_Flecs_Iterate_1Component(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    
    for (size_t i = 0; i < n; ++i) {
        world.entity().set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    for (auto _ : state) {
        world.each<FC1>([&](flecs::entity e, FC1& c1) {
            (void)e;
            sum += c1.a + c1.b + c1.c + c1.d + c1.e + c1.f + c1.g;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_Iterate_1Component)->Range(1024, 65536);

// ============================================================================
// Sequential Iteration Benchmarks - 2 Components
// ============================================================================

// Entable: Sequential iteration - 2 components
static void BM_Entable_Iterate_2Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry2 reg;
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC2>(e, EC2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    for (auto _ : state) {
        reg.Each<EC1, EC2>([&](EC1& c1, EC2& c2) {
            sum += c1.a + c1.b + c2.a + c2.b;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_Iterate_2Components)->Range(1024, 65536);

// Flecs: Sequential iteration - 2 components (using query)
static void BM_Flecs_Iterate_2Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    world.component<FC2>();
    
    for (size_t i = 0; i < n; ++i) {
        world.entity()
            .set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC2>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    auto query = world.query<FC1, FC2>();
    for (auto _ : state) {
        query.each([&](flecs::entity e, FC1& c1, FC2& c2) {
            (void)e;
            sum += c1.a + c1.b + c2.a + c2.b;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_Iterate_2Components)->Range(1024, 65536);

// ============================================================================
// Sequential Iteration Benchmarks - 4 Components
// ============================================================================

// Entable: Sequential iteration - 4 components
static void BM_Entable_Iterate_4Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry4 reg;
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC2>(e, EC2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC3>(e, EC3{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC4>(e, EC4{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    for (auto _ : state) {
        reg.Each<EC1, EC2, EC3, EC4>([&](EC1& c1, EC2& c2, EC3& c3, EC4& c4) {
            sum += c1.a + c2.a + c3.a + c4.a;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_Iterate_4Components)->Range(1024, 65536);

// Flecs: Sequential iteration - 4 components (using query)
static void BM_Flecs_Iterate_4Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    world.component<FC2>();
    world.component<FC3>();
    world.component<FC4>();
    
    for (size_t i = 0; i < n; ++i) {
        world.entity()
            .set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC2>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC3>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC4>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    auto query = world.query<FC1, FC2, FC3, FC4>();
    for (auto _ : state) {
        query.each([&](flecs::entity e, FC1& c1, FC2& c2, FC3& c3, FC4& c4) {
            (void)e;
            sum += c1.a + c2.a + c3.a + c4.a;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_Iterate_4Components)->Range(1024, 65536);

// ============================================================================
// Sequential Iteration Benchmarks - 8 Components
// ============================================================================

// Entable: Sequential iteration - 8 components
static void BM_Entable_Iterate_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry8 reg;
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC2>(e, EC2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC3>(e, EC3{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC4>(e, EC4{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC5>(e, EC5{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC6>(e, EC6{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC7>(e, EC7{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC8>(e, EC8{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    for (auto _ : state) {
        reg.Each<EC1, EC2, EC3, EC4, EC5, EC6, EC7, EC8>([&](
            EC1& c1, EC2& c2, EC3& c3, EC4& c4,
            EC5& c5, EC6& c6, EC7& c7, EC8& c8) {
            sum += c1.a + c2.a + c3.a + c4.a + c5.a + c6.a + c7.a + c8.a;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_Iterate_8Components)->Range(1024, 65536);

// Flecs: Sequential iteration - 8 components (using query)
static void BM_Flecs_Iterate_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    world.component<FC2>();
    world.component<FC3>();
    world.component<FC4>();
    world.component<FC5>();
    world.component<FC6>();
    world.component<FC7>();
    world.component<FC8>();
    
    for (size_t i = 0; i < n; ++i) {
        world.entity()
            .set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC2>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC3>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC4>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC5>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC6>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC7>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC8>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
    }
    double sum = 0;
    auto query = world.query<FC1, FC2, FC3, FC4, FC5, FC6, FC7, FC8>();
    for (auto _ : state) {
        query.each([&](flecs::entity e, FC1& c1, FC2& c2, FC3& c3, FC4& c4,
            FC5& c5, FC6& c6, FC7& c7, FC8& c8) {
            (void)e;
            sum += c1.a + c2.a + c3.a + c4.a + c5.a + c6.a + c7.a + c8.a;
        });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_Iterate_8Components)->Range(1024, 65536);

// ============================================================================
// Random Read Benchmarks - 1 Component
// ============================================================================

// Entable: Random read - 1 component
static void BM_Entable_RandomRead_1Component(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry1 reg;
    std::vector<ent::Entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        entities.push_back(e);
    }
    
    // Generate random indices
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    size_t idx = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            auto& c = reg.Get<EC1>(entities[indices[i]]);
            sum += c.a;
        }
        benchmark::DoNotOptimize(sum);
        idx = (idx + 1) % n;
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_RandomRead_1Component)->Range(1024, 65536);

// Flecs: Random read - 1 component
static void BM_Flecs_RandomRead_1Component(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    
    std::vector<flecs::entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        entities.push_back(world.entity().set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}));
    }
    
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            const auto& c = entities[indices[i]].get<FC1>();
            sum += c.a;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_RandomRead_1Component)->Range(1024, 65536);

// ============================================================================
// Random Read Benchmarks - 2 Components
// ============================================================================

// Entable: Random read - 2 components
static void BM_Entable_RandomRead_2Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry2 reg;
    std::vector<ent::Entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC2>(e, EC2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        entities.push_back(e);
    }
    
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            auto [c1, c2] = reg.Get<EC1, EC2>(entities[indices[i]]);
            sum += c1.a + c2.a;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_RandomRead_2Components)->Range(1024, 65536);

// Flecs: Random read - 2 components
static void BM_Flecs_RandomRead_2Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    world.component<FC2>();
    
    std::vector<flecs::entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        entities.push_back(world.entity()
            .set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC2>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}));
    }
    
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            const auto& c1 = entities[indices[i]].get<FC1>();
            const auto& c2 = entities[indices[i]].get<FC2>();
            sum += c1.a + c2.a;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_RandomRead_2Components)->Range(1024, 65536);

// ============================================================================
// Random Read Benchmarks - 4 Components
// ============================================================================

// Entable: Random read - 4 components
static void BM_Entable_RandomRead_4Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry4 reg;
    std::vector<ent::Entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC2>(e, EC2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC3>(e, EC3{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC4>(e, EC4{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        entities.push_back(e);
    }
    
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            auto [c1, c2, c3, c4] = reg.Get<EC1, EC2, EC3, EC4>(entities[indices[i]]);
            sum += c1.a + c2.a + c3.a + c4.a;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_RandomRead_4Components)->Range(1024, 65536);

// Flecs: Random read - 4 components
static void BM_Flecs_RandomRead_4Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    world.component<FC2>();
    world.component<FC3>();
    world.component<FC4>();
    
    std::vector<flecs::entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        entities.push_back(world.entity()
            .set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC2>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC3>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC4>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}));
    }
    
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            const auto& c1 = entities[indices[i]].get<FC1>();
            const auto& c2 = entities[indices[i]].get<FC2>();
            const auto& c3 = entities[indices[i]].get<FC3>();
            const auto& c4 = entities[indices[i]].get<FC4>();
            sum += c1.a + c2.a + c3.a + c4.a;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_RandomRead_4Components)->Range(1024, 65536);

// ============================================================================
// Random Read Benchmarks - 8 Components
// ============================================================================

// Entable: Random read - 8 components
static void BM_Entable_RandomRead_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    EntableRegistry8 reg;
    std::vector<ent::Entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        auto e = reg.CreateEntity();
        reg.Set<EC1>(e, EC1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC2>(e, EC2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC3>(e, EC3{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC4>(e, EC4{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC5>(e, EC5{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC6>(e, EC6{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC7>(e, EC7{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        reg.Set<EC8>(e, EC8{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0});
        entities.push_back(e);
    }
    
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            auto [c1, c2, c3, c4, c5, c6, c7, c8] = reg.Get<EC1, EC2, EC3, EC4, EC5, EC6, EC7, EC8>(entities[indices[i]]);
            sum += c1.a + c2.a + c3.a + c4.a + c5.a + c6.a + c7.a + c8.a;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_RandomRead_8Components)->Range(1024, 65536);

// Flecs: Random read - 8 components
static void BM_Flecs_RandomRead_8Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    flecs::world world;
    world.component<FC1>();
    world.component<FC2>();
    world.component<FC3>();
    world.component<FC4>();
    world.component<FC5>();
    world.component<FC6>();
    world.component<FC7>();
    world.component<FC8>();
    
    std::vector<flecs::entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        entities.push_back(world.entity()
            .set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC2>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC3>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC4>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC5>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC6>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC7>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
            .set<FC8>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}));
    }
    
    std::mt19937 rng(42);
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    
    double sum = 0;
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            const auto& c1 = entities[indices[i]].get<FC1>();
            const auto& c2 = entities[indices[i]].get<FC2>();
            const auto& c3 = entities[indices[i]].get<FC3>();
            const auto& c4 = entities[indices[i]].get<FC4>();
            const auto& c5 = entities[indices[i]].get<FC5>();
            const auto& c6 = entities[indices[i]].get<FC6>();
            const auto& c7 = entities[indices[i]].get<FC7>();
            const auto& c8 = entities[indices[i]].get<FC8>();
            sum += c1.a + c2.a + c3.a + c4.a + c5.a + c6.a + c7.a + c8.a;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_RandomRead_8Components)->Range(1024, 65536);

// ============================================================================
// Entity Deletion Benchmarks
// ============================================================================

// Entable: Entity deletion (8 components)
static void BM_Entable_DeleteEntities(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::mt19937 rng(42);
    
    for (auto _ : state) {
        EntableRegistry8 reg;
        std::vector<ent::Entity> entities;
        entities.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            entities.push_back(reg.CreateEntity());
        }
        
        // Shuffle for random deletion order
        std::shuffle(entities.begin(), entities.end(), rng);
        
        for (size_t i = 0; i < n; ++i) {
            reg.DestroyEntity(entities[i]);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Entable_DeleteEntities)->Range(1024, 65536);

// Flecs: Entity deletion (8 components)
static void BM_Flecs_DeleteEntities(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::mt19937 rng(42);
    
    for (auto _ : state) {
        flecs::world world;
        world.component<FC1>();
        world.component<FC2>();
        world.component<FC3>();
        world.component<FC4>();
        world.component<FC5>();
        world.component<FC6>();
        world.component<FC7>();
        world.component<FC8>();
        
        std::vector<flecs::entity> entities;
        entities.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            entities.push_back(world.entity()
                .set<FC1>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
                .set<FC2>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
                .set<FC3>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
                .set<FC4>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
                .set<FC5>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
                .set<FC6>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
                .set<FC7>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0})
                .set<FC8>({1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}));
        }
        
        // Shuffle for random deletion order
        std::shuffle(entities.begin(), entities.end(), rng);
        
        for (size_t i = 0; i < n; ++i) {
            entities[i].destruct();
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Flecs_DeleteEntities)->Range(1024, 65536);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
