// SoA (registry of components) vs AoS (aggregate owning all components).
// Uses medium-sized components to keep AoS entity payload around 200-300 bytes.

#include <benchmark/benchmark.h>
#include <cstddef>
#include <random>
#include <type_traits>
#include <vector>

// Provide a default UserConfig so Entable trait probes are well-formed for local benchmark types.
namespace entable {
template <typename T>
struct UserConfig {};
}

#include <Entable.hpp>

namespace ent = entable;

// --- Component types (all non-empty, each 32 bytes) ---

struct C1 { double a = 0, b = 0, c = 0, d = 0; };
struct C2 { double a = 0, b = 0, c = 0, d = 0; };
struct C3 { double a = 0, b = 0, c = 0, d = 0; };
struct C4 { double a = 0, b = 0, c = 0, d = 0; };
struct C5 { double a = 0, b = 0, c = 0, d = 0; };
struct C6 { double a = 0, b = 0, c = 0, d = 0; };
struct C7 { double a = 0, b = 0, c = 0, d = 0; };
struct C8 { double a = 0, b = 0, c = 0, d = 0; };

using SoARegistry = ent::Registry<ent::type_list_t<C1, C2, C3, C4, C5, C6, C7, C8>>;

struct EntityData {
    C1 c1;
    C2 c2;
    C3 c3;
    C4 c4;
    C5 c5;
    C6 c6;
    C7 c7;
    C8 c8;
};
using AoSStorage = std::vector<EntityData>;

static_assert(sizeof(EntityData) >= 200 && sizeof(EntityData) <= 300, "EntityData should be ~200-300 bytes");
static_assert(!std::is_empty_v<C1> && !std::is_empty_v<C8>, "Benchmarks must use non-empty components");

static void BM_SoA_CreateEntities(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        SoARegistry reg;
        for (size_t i = 0; i < n; ++i) {
            benchmark::DoNotOptimize(reg.CreateEntity());
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_AoS_CreateEntities(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        AoSStorage vec;
        vec.resize(n);
        benchmark::DoNotOptimize(vec.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_SoA_DestroyEntities(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::mt19937 rng(42); // Fixed seed for reproducibility
    for (auto _ : state) {
        state.PauseTiming();
        SoARegistry reg;
        std::vector<ent::Entity> entities;
        entities.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            entities.push_back(reg.CreateEntity());
        }
        // Shuffle for random destruction order
        std::shuffle(entities.begin(), entities.end(), rng);
        state.ResumeTiming();
        for (size_t i = 0; i < n; ++i) {
            reg.DestroyEntity(entities[i]);
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_AoS_DestroyEntities(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::vector<size_t> indexMapping(n); // Maps original index to current position
    for (auto _ : state) {
        state.PauseTiming();
        AoSStorage vec(n);
        // Create index array and shuffle for random destruction order
        std::vector<size_t> indices(n);
        for (size_t i = 0; i < n; ++i) {
            indices[i] = i;
            indexMapping[i] = i; // Initially each index maps to itself
        }
        std::shuffle(indices.begin(), indices.end(), rng);
        state.ResumeTiming();
        // Swap-and-pop erase in random order
        for (size_t i = 0; i < n; ++i) {
            size_t originalIdx = indices[i];
            size_t currentIdx = indexMapping[originalIdx];
            size_t lastIdx = vec.size() - 1;

            if (currentIdx != lastIdx) {
                // Move last element to the position being removed
                vec[currentIdx] = std::move(vec[lastIdx]);
                // Update mapping for the element that was moved
                // Find which original index is now at lastIdx and update its mapping
                for (size_t j = 0; j < n; ++j) {
                    if (indexMapping[j] == lastIdx) {
                        indexMapping[j] = currentIdx;
                        break;
                    }
                }
            }
            vec.pop_back();
            indexMapping[originalIdx] = SIZE_MAX; // Mark as removed
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// Benchmark: Destroy N of M entities in random order, then recreate N entities
// Tests the destroy/recreate pattern common in ECS systems
static void BM_SoA_DestroyAndRecreate_NofM(benchmark::State& state) {
    const size_t m = static_cast<size_t>(state.range(0)); // Total entities
    const size_t n = m / 2; // Destroy half, then recreate half
    std::mt19937 rng(42); // Fixed seed for reproducibility

    for (auto _ : state) {
        state.PauseTiming();
        SoARegistry reg;
        std::vector<ent::Entity> entities;
        entities.reserve(m);
        for (size_t i = 0; i < m; ++i) {
            entities.push_back(reg.CreateEntity());
        }
        // Shuffle for random destruction order
        std::shuffle(entities.begin(), entities.end(), rng);
        state.ResumeTiming();

        // Destroy first N entities (random order)
        for (size_t i = 0; i < n; ++i) {
            reg.DestroyEntity(entities[i]);
        }

        // Recreate N entities (will reuse indices with incremented versions)
        for (size_t i = 0; i < n; ++i) {
            benchmark::DoNotOptimize(reg.CreateEntity());
        }

        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n * 2); // n destroys + n creates
}

// Vector benchmark: Destroy N of M by index (swap-and-pop), no recreate
// For comparison - shows overhead of index tracking
static void BM_AoS_Destroy_NofM(benchmark::State& state) {
    const size_t m = static_cast<size_t>(state.range(0)); // Total entities
    const size_t n = m / 2; // Destroy half
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::vector<size_t> indexMapping(m); // Maps original index to current position

    for (auto _ : state) {
        state.PauseTiming();
        AoSStorage vec(m);
        // Create index array and shuffle for random destruction order
        std::vector<size_t> indices(m);
        for (size_t i = 0; i < m; ++i) {
            indices[i] = i;
            indexMapping[i] = i;
        }
        // Only shuffle first n indices (the ones we'll destroy)
        std::shuffle(indices.begin(), indices.begin() + n, rng);
        state.ResumeTiming();

        // Swap-and-pop erase first N indices in random order
        for (size_t i = 0; i < n; ++i) {
            size_t originalIdx = indices[i];
            size_t currentIdx = indexMapping[originalIdx];
            size_t lastIdx = vec.size() - 1;

            if (currentIdx != lastIdx) {
                vec[currentIdx] = std::move(vec[lastIdx]);
                for (size_t j = 0; j < m; ++j) {
                    if (indexMapping[j] == lastIdx) {
                        indexMapping[j] = currentIdx;
                        break;
                    }
                }
            }
            vec.pop_back();
            indexMapping[originalIdx] = SIZE_MAX;
        }

        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_SoA_UpdateOneByIndex(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    std::vector<ent::Entity> entities;
    entities.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        entities.push_back(reg.CreateEntity());
    }
    const C1 value{1.0, 2.0, 3.0, 4.0};
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            reg.Set<C1>(entities[i], value);
        }
        benchmark::DoNotOptimize(reg.RawSize());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_AoS_UpdateOneByIndex(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    const C1 value{1.0, 2.0, 3.0, 4.0};
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i) {
            vec[i].c1 = value;
        }
        benchmark::DoNotOptimize(vec.data());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_SoA_BatchUpdate_1Component(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    for (auto _ : state) {
        reg.Each<C1>([&](C1& x1) {
            x1 = c1;
        });
        benchmark::DoNotOptimize(reg.RawSize());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_AoS_BatchUpdate_1Field(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    for (auto _ : state) {
        for (auto& e : vec) {
            e.c1 = c1;
        }
        benchmark::DoNotOptimize(vec.data());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_SoA_BatchUpdate_2Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    const C2 c2{5.0, 6.0, 7.0, 8.0};
    for (auto _ : state) {
        reg.Each<C1, C2>([&](C1& x1, C2& x2) {
            x1 = c1;
            x2 = c2;
        });
        benchmark::DoNotOptimize(reg.RawSize());
    }
    state.SetItemsProcessed(state.iterations() * n * 2);
}

static void BM_AoS_BatchUpdate_2Fields(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    const C2 c2{5.0, 6.0, 7.0, 8.0};
    for (auto _ : state) {
        for (auto& e : vec) {
            e.c1 = c1;
            e.c2 = c2;
        }
        benchmark::DoNotOptimize(vec.data());
    }
    state.SetItemsProcessed(state.iterations() * n * 2);
}

static void BM_SoA_BatchUpdate_4Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    const C2 c2{5.0, 6.0, 7.0, 8.0};
    const C3 c3{9.0, 10.0, 11.0, 12.0};
    const C4 c4{13.0, 14.0, 15.0, 16.0};
    for (auto _ : state) {
        reg.Each<C1, C2, C3, C4>([&](C1& x1, C2& x2, C3& x3, C4& x4) {
            x1 = c1;
            x2 = c2;
            x3 = c3;
            x4 = c4;
        });
        benchmark::DoNotOptimize(reg.RawSize());
    }
    state.SetItemsProcessed(state.iterations() * n * 4);
}

static void BM_AoS_BatchUpdate_4Fields(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    const C2 c2{5.0, 6.0, 7.0, 8.0};
    const C3 c3{9.0, 10.0, 11.0, 12.0};
    const C4 c4{13.0, 14.0, 15.0, 16.0};
    for (auto _ : state) {
        for (auto& e : vec) {
            e.c1 = c1;
            e.c2 = c2;
            e.c3 = c3;
            e.c4 = c4;
        }
        benchmark::DoNotOptimize(vec.data());
    }
    state.SetItemsProcessed(state.iterations() * n * 4);
}

static void BM_SoA_BatchUpdate_AllComponents(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    const C2 c2{5.0, 6.0, 7.0, 8.0};
    const C3 c3{9.0, 10.0, 11.0, 12.0};
    const C4 c4{13.0, 14.0, 15.0, 16.0};
    const C5 c5{17.0, 18.0, 19.0, 20.0};
    const C6 c6{21.0, 22.0, 23.0, 24.0};
    const C7 c7{25.0, 26.0, 27.0, 28.0};
    const C8 c8{29.0, 30.0, 31.0, 32.0};
    for (auto _ : state) {
        reg.Each<C1, C2, C3, C4, C5, C6, C7, C8>(
            [&](C1& x1, C2& x2, C3& x3, C4& x4, C5& x5, C6& x6, C7& x7, C8& x8) {
                x1 = c1;
                x2 = c2;
                x3 = c3;
                x4 = c4;
                x5 = c5;
                x6 = c6;
                x7 = c7;
                x8 = c8;
            });
        benchmark::DoNotOptimize(reg.RawSize());
    }
    state.SetItemsProcessed(state.iterations() * n * 8);
}

static void BM_AoS_BatchUpdate_AllFields(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    const C1 c1{1.0, 2.0, 3.0, 4.0};
    const C2 c2{5.0, 6.0, 7.0, 8.0};
    const C3 c3{9.0, 10.0, 11.0, 12.0};
    const C4 c4{13.0, 14.0, 15.0, 16.0};
    const C5 c5{17.0, 18.0, 19.0, 20.0};
    const C6 c6{21.0, 22.0, 23.0, 24.0};
    const C7 c7{25.0, 26.0, 27.0, 28.0};
    const C8 c8{29.0, 30.0, 31.0, 32.0};
    for (auto _ : state) {
        for (auto& e : vec) {
            e.c1 = c1;
            e.c2 = c2;
            e.c3 = c3;
            e.c4 = c4;
            e.c5 = c5;
            e.c6 = c6;
            e.c7 = c7;
            e.c8 = c8;
        }
        benchmark::DoNotOptimize(vec.data());
    }
    state.SetItemsProcessed(state.iterations() * n * 8);
}

static void BM_SoA_BatchRead_1Component(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        reg.Each<C1>([&](const C1& c) {
            acc += c.a + c.b + c.c + c.d;
        });
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_AoS_BatchRead_1Field(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        for (const auto& e : vec) {
            acc += e.c1.a + e.c1.b + e.c1.c + e.c1.d;
        }
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

static void BM_SoA_BatchRead_2Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        reg.Each<C1, C2>([&](const C1& c1, const C2& c2) {
            acc += c1.a + c1.b + c1.c + c1.d;
            acc += c2.a + c2.b + c2.c + c2.d;
        });
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n * 2);
}

static void BM_AoS_BatchRead_2Fields(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        for (const auto& e : vec) {
            acc += e.c1.a + e.c1.b + e.c1.c + e.c1.d;
            acc += e.c2.a + e.c2.b + e.c2.c + e.c2.d;
        }
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n * 2);
}

static void BM_SoA_BatchRead_4Components(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        reg.Each<C1, C2, C3, C4>([&](const C1& c1, const C2& c2, const C3& c3, const C4& c4) {
            acc += c1.a + c1.b + c1.c + c1.d;
            acc += c2.a + c2.b + c2.c + c2.d;
            acc += c3.a + c3.b + c3.c + c3.d;
            acc += c4.a + c4.b + c4.c + c4.d;
        });
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n * 4);
}

static void BM_AoS_BatchRead_4Fields(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        for (const auto& e : vec) {
            acc += e.c1.a + e.c1.b + e.c1.c + e.c1.d;
            acc += e.c2.a + e.c2.b + e.c2.c + e.c2.d;
            acc += e.c3.a + e.c3.b + e.c3.c + e.c3.d;
            acc += e.c4.a + e.c4.b + e.c4.c + e.c4.d;
        }
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n * 4);
}

static void BM_SoA_BatchRead_AllComponents(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    SoARegistry reg;
    for (size_t i = 0; i < n; ++i) {
        reg.CreateEntity();
    }
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        reg.Each<C1, C2, C3, C4, C5, C6, C7, C8>(
            [&](const C1& c1, const C2& c2, const C3& c3, const C4& c4, const C5& c5, const C6& c6, const C7& c7, const C8& c8) {
            acc += c1.a + c2.a + c3.a + c4.a + c5.a + c6.a + c7.a + c8.a;
            });
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n * 8);
}

static void BM_AoS_BatchRead_AllFields(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    AoSStorage vec(n);
    double acc = 0.0;
    for (auto _ : state) {
        acc = 0.0;
        for (const auto& e : vec) {
            acc += e.c1.a + e.c2.a + e.c3.a + e.c4.a;
            acc += e.c5.a + e.c6.a + e.c7.a + e.c8.a;
        }
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * n * 8);
}

#define ARGS_ENTITY_COUNTS ->Arg(256)->Arg(1024)->Arg(4096)->Arg(16384)->Arg(65536)

BENCHMARK(BM_SoA_CreateEntities) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_CreateEntities) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_DestroyEntities) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_DestroyEntities) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_DestroyAndRecreate_NofM) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_Destroy_NofM) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_UpdateOneByIndex) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_UpdateOneByIndex) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchUpdate_1Component) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchUpdate_1Field) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchUpdate_2Components) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchUpdate_2Fields) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchUpdate_4Components) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchUpdate_4Fields) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchUpdate_AllComponents) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchUpdate_AllFields) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchRead_1Component) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchRead_1Field) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchRead_2Components) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchRead_2Fields) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchRead_4Components) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchRead_4Fields) ARGS_ENTITY_COUNTS;

BENCHMARK(BM_SoA_BatchRead_AllComponents) ARGS_ENTITY_COUNTS;
BENCHMARK(BM_AoS_BatchRead_AllFields) ARGS_ENTITY_COUNTS;

BENCHMARK_MAIN();
