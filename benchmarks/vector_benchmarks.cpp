// Benchmarks: entable::ChunkedArray vs std::vector for normal (non-empty) types.
// Compile and run in Release for meaningful results.

#include <benchmark/benchmark.h>
#include <ChunkedArray.hpp>
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace ent = entable;

static constexpr size_t kChunkSize = 256;

static std::vector<size_t> MakeShuffledIndices(size_t n) {
    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), size_t{0});
    // Fixed seed keeps benchmark runs reproducible while preserving random access order.
    std::mt19937 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);
    return indices;
}

// --- Type fixtures: int, int64_t, small struct ---

struct Point3d {
    double x = 0, y = 0, z = 0;
    Point3d() = default;
    Point3d(double a, double b, double c) : x(a), y(b), z(c) {}
    Point3d operator+(const Point3d& o) const {
        return Point3d(x + o.x, y + o.y, z + o.z);
    }
};

// --- PushBack: repeated push_back to build container ---

template <typename T>
static void BM_Vector_PushBack(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    T value{};
    for (auto _ : state) {
        std::vector<T> v;
        // No reserve - measure growth cost
        for (size_t i = 0; i < n; ++i)
            v.push_back(value);
        benchmark::DoNotOptimize(v.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename T>
static void BM_ChunkedArray_PushBack(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    T value{};
    for (auto _ : state) {
        ent::ChunkedArray<T, kChunkSize> v;
        // No reserve - measure growth cost
        for (size_t i = 0; i < n; ++i)
            v.push_back(value);
        // Use same strength sink as vector: access first element pointer
        benchmark::DoNotOptimize(n > 0 ? &v[0] : nullptr);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// --- Random access read: sum over operator[] ---

template <typename T>
static void BM_Vector_RandomAccessRead(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    std::vector<T> v(n, T{});
    const std::vector<size_t> indices = MakeShuffledIndices(n);
    for (auto _ : state) {
        T sum{};
        for (size_t i = 0; i < n; ++i)
            sum = static_cast<T>(sum + v[indices[i]]);  // avoid unused for non-numeric
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename T>
static void BM_ChunkedArray_RandomAccessRead(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    ent::ChunkedArray<T, kChunkSize> v;
    v.ensure_size(n);
    const std::vector<size_t> indices = MakeShuffledIndices(n);
    for (auto _ : state) {
        T sum{};
        for (size_t i = 0; i < n; ++i)
            sum = static_cast<T>(sum + v[indices[i]]);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// --- Random access write: fill via operator[] ---

template <typename T>
static void BM_Vector_RandomAccessWrite(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    std::vector<T> v(n);
    T value{};
    const std::vector<size_t> indices = MakeShuffledIndices(n);
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i)
            v[indices[i]] = value;
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename T>
static void BM_ChunkedArray_RandomAccessWrite(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    ent::ChunkedArray<T, kChunkSize> v;
    v.ensure_size(n);
    T value{};
    const std::vector<size_t> indices = MakeShuffledIndices(n);
    for (auto _ : state) {
        for (size_t i = 0; i < n; ++i)
            v[indices[i]] = value;
        benchmark::DoNotOptimize(n > 0 ? &v[0] : nullptr);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// --- Iteration: range-for over container ---

template <typename T>
static void BM_Vector_Iteration(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    std::vector<T> v(n, T{});
    for (auto _ : state) {
        T sum{};
        for (const auto& x : v)
            sum = static_cast<T>(sum + x);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename T>
static void BM_ChunkedArray_Iteration(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    ent::ChunkedArray<T, kChunkSize> v;
    v.ensure_size(n);
    for (auto _ : state) {
        T sum{};
        for (const auto& x : v)
            sum = static_cast<T>(sum + x);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// --- ReserveThenPushBack (no per-iteration reserve): measures growth cost ---

template <typename T>
static void BM_Vector_ReserveThenPushBack(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    T value{};
    for (auto _ : state) {
        std::vector<T> v;
        v.reserve(n);
        for (size_t i = 0; i < n; ++i)
            v.push_back(value);
        benchmark::DoNotOptimize(v.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename T>
static void BM_ChunkedArray_ReserveThenPushBack(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    T value{};
    for (auto _ : state) {
        ent::ChunkedArray<T, kChunkSize> v;
        v.reserve(n);
        for (size_t i = 0; i < n; ++i)
            v.push_back(value);
        benchmark::DoNotOptimize(n > 0 ? &v[0] : nullptr);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// --- Resize / ensure_size then fill ---

template <typename T>
static void BM_Vector_ResizeThenFill(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    T value{};
    for (auto _ : state) {
        std::vector<T> v;
        v.resize(n, value);
        for (size_t i = 0; i < n; ++i)
            v[i] = value;
        benchmark::DoNotOptimize(v.data());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename T>
static void BM_ChunkedArray_EnsureSizeThenFill(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    T value{};
    for (auto _ : state) {
        ent::ChunkedArray<T, kChunkSize> v;
        v.ensure_size(n);
        for (size_t i = 0; i < n; ++i)
            v[i] = value;
        benchmark::DoNotOptimize(n > 0 ? &v[0] : nullptr);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// --- Back: repeated back() read ---

template <typename T>
static void BM_Vector_Back(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    std::vector<T> v(n, T{});
    for (auto _ : state) {
        T x{};
        for (size_t i = 0; i < n; ++i)
            x = v.back();
        benchmark::DoNotOptimize(x);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename T>
static void BM_ChunkedArray_Back(benchmark::State& state) {
    size_t n = static_cast<size_t>(state.range(0));
    ent::ChunkedArray<T, kChunkSize> v;
    v.ensure_size(n);
    for (auto _ : state) {
        T x{};
        for (size_t i = 0; i < n; ++i)
            x = v.back();
        benchmark::DoNotOptimize(x);
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// --- Register benchmarks grouped by data type ---

#define REGISTER_BENCHMARK_FOR_TYPE(Suite, Type) \
    BENCHMARK_TEMPLATE(Suite, Type)->Arg(64)->Arg(256)->Arg(1024)->Arg(4096)->Arg(16384)->Arg(65536);

#define REGISTER_TYPE_BENCHMARKS(Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_Vector_PushBack, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_ChunkedArray_PushBack, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_Vector_RandomAccessRead, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_ChunkedArray_RandomAccessRead, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_Vector_RandomAccessWrite, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_ChunkedArray_RandomAccessWrite, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_Vector_Iteration, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_ChunkedArray_Iteration, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_Vector_ReserveThenPushBack, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_ChunkedArray_ReserveThenPushBack, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_Vector_ResizeThenFill, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_ChunkedArray_EnsureSizeThenFill, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_Vector_Back, Type) \
    REGISTER_BENCHMARK_FOR_TYPE(BM_ChunkedArray_Back, Type)

// int64_t
REGISTER_TYPE_BENCHMARKS(int64_t)

// Point3d
REGISTER_TYPE_BENCHMARKS(Point3d)

BENCHMARK_MAIN();
