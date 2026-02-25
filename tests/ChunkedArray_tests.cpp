// Catch2 tests for ChunkedArray correctness

#include <catch2/catch_test_macros.hpp>
#include <ChunkedArray.hpp>
#include <algorithm>
#include <vector>
#include <string>
#include <stdexcept>

namespace ent = entable;

static constexpr size_t kChunkSize = 256;

// =============================================================================
// Non-Empty Type Tests (comparing against std::vector)
// =============================================================================

TEST_CASE("ChunkedArray<int>: push_back matches vector", "[ChunkedArray][non-empty][push_back]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;

    SECTION("Empty after construction")
    {
        REQUIRE(chunked.empty());
        REQUIRE(chunked.size() == 0);
        REQUIRE(reference.empty());
        REQUIRE(reference.size() == 0);
    }

    SECTION("Single push_back")
    {
        chunked.push_back(42);
        reference.push_back(42);

        REQUIRE(chunked.size() == reference.size());
        REQUIRE(chunked[0] == reference[0]);
        REQUIRE(chunked.back() == reference.back());
    }

    SECTION("Multiple push_back within single chunk")
    {
        for (int i = 0; i < 100; ++i)
        {
            chunked.push_back(i * 2);
            reference.push_back(i * 2);
        }

        REQUIRE(chunked.size() == reference.size());
        for (size_t i = 0; i < reference.size(); ++i)
        {
            REQUIRE(chunked[i] == reference[i]);
        }
        REQUIRE(chunked.back() == reference.back());
    }

    SECTION("push_back across chunk boundary")
    {
        // Push more than one chunk worth
        for (int i = 0; i < static_cast<int>(kChunkSize) + 50; ++i)
        {
            chunked.push_back(i);
            reference.push_back(i);
        }

        REQUIRE(chunked.size() == reference.size());
        REQUIRE(chunked.chunk_count() == 2);

        for (size_t i = 0; i < reference.size(); ++i)
        {
            REQUIRE(chunked[i] == reference[i]);
        }
    }

    SECTION("push_back many chunks")
    {
        const size_t numElements = kChunkSize * 5 + 123;
        for (size_t i = 0; i < numElements; ++i)
        {
            chunked.push_back(static_cast<int>(i));
            reference.push_back(static_cast<int>(i));
        }

        REQUIRE(chunked.size() == reference.size());
        REQUIRE(chunked.chunk_count() == 6);

        for (size_t i = 0; i < reference.size(); ++i)
        {
            REQUIRE(chunked[i] == reference[i]);
        }
    }
}

TEST_CASE("ChunkedArray<int>: emplace_back matches vector", "[ChunkedArray][non-empty][emplace_back]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;

    SECTION("emplace_back int")
    {
        for (int i = 0; i < 500; ++i)
        {
            chunked.emplace_back(i * 3);
            reference.emplace_back(i * 3);
        }

        REQUIRE(chunked.size() == reference.size());
        for (size_t i = 0; i < reference.size(); ++i)
        {
            REQUIRE(chunked[i] == reference[i]);
        }
    }
}

TEST_CASE("ChunkedArray<string>: emplace_back with args", "[ChunkedArray][non-empty][emplace_back]")
{
    ent::ChunkedArray<std::string, kChunkSize> chunked;
    std::vector<std::string> reference;

    SECTION("emplace_back string with constructor args")
    {
        for (int i = 0; i < 100; ++i)
        {
            chunked.emplace_back("test", 0UL, 2UL);  // "te"
            reference.emplace_back("test", 0UL, 2UL);
        }

        REQUIRE(chunked.size() == reference.size());
        for (size_t i = 0; i < reference.size(); ++i)
        {
            REQUIRE(chunked[i] == reference[i]);
            REQUIRE(chunked[i] == "te");
        }
    }
}

TEST_CASE("ChunkedArray<int>: operator subscript read/write", "[ChunkedArray][non-empty][subscript]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;

    const size_t numElements = kChunkSize * 3 + 50;
    chunked.ensure_size(numElements);
    reference.resize(numElements);

    SECTION("Write via operator[]")
    {
        for (size_t i = 0; i < numElements; ++i)
        {
            chunked[i] = static_cast<int>(i * 7);
            reference[i] = static_cast<int>(i * 7);
        }

        for (size_t i = 0; i < numElements; ++i)
        {
            REQUIRE(chunked[i] == reference[i]);
        }
    }

    SECTION("Read after write")
    {
        for (size_t i = 0; i < numElements; ++i)
        {
            chunked[i] = static_cast<int>(i);
            reference[i] = static_cast<int>(i);
        }

        int sumChunked = 0;
        int sumReference = 0;
        for (size_t i = 0; i < numElements; ++i)
        {
            sumChunked += chunked[i];
            sumReference += reference[i];
        }
        REQUIRE(sumChunked == sumReference);
    }
}

TEST_CASE("ChunkedArray<int>: at() with bounds checking", "[ChunkedArray][non-empty][at]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    chunked.push_back(10);
    chunked.push_back(20);
    chunked.push_back(30);

    SECTION("Valid access")
    {
        REQUIRE(chunked.at(0) == 10);
        REQUIRE(chunked.at(1) == 20);
        REQUIRE(chunked.at(2) == 30);
    }

    SECTION("Out of range throws")
    {
        REQUIRE_THROWS_AS(chunked.at(3), std::out_of_range);
        REQUIRE_THROWS_AS(chunked.at(100), std::out_of_range);
    }

    SECTION("const at() out of range throws")
    {
        const auto& constChunked = chunked;
        REQUIRE_THROWS_AS(constChunked.at(3), std::out_of_range);
    }
}

TEST_CASE("ChunkedArray<int>: ensure_size", "[ChunkedArray][non-empty][ensure_size]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    SECTION("ensure_size grows container")
    {
        chunked.ensure_size(500);
        REQUIRE(chunked.size() == 500);
        REQUIRE(chunked.chunk_count() == 2);  // ceil(500/256) = 2
    }

    SECTION("ensure_size does not shrink")
    {
        chunked.ensure_size(500);
        chunked.ensure_size(100);
        REQUIRE(chunked.size() == 500);  // Should not shrink
    }

    SECTION("ensure_size zero does nothing")
    {
        chunked.ensure_size(0);
        REQUIRE(chunked.empty());
    }
}

TEST_CASE("ChunkedArray<int>: reserve", "[ChunkedArray][non-empty][reserve]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    SECTION("reserve does not change size")
    {
        chunked.reserve(1000);
        REQUIRE(chunked.empty());
        REQUIRE(chunked.size() == 0);
    }

    SECTION("reserve then push_back")
    {
        chunked.reserve(500);
        for (int i = 0; i < 500; ++i)
        {
            chunked.push_back(i);
        }
        REQUIRE(chunked.size() == 500);
    }
}

TEST_CASE("ChunkedArray<int>: clear", "[ChunkedArray][non-empty][clear]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    for (int i = 0; i < 500; ++i)
    {
        chunked.push_back(i);
    }

    REQUIRE(chunked.size() == 500);
    REQUIRE(chunked.chunk_count() == 2);

    chunked.clear();

    REQUIRE(chunked.empty());
    REQUIRE(chunked.size() == 0);
    REQUIRE(chunked.chunk_count() == 0);
}

TEST_CASE("ChunkedArray<int>: shrink_to_fit", "[ChunkedArray][non-empty][shrink_to_fit]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    SECTION("Shrink after ensure_size")
    {
        chunked.ensure_size(1000);  // Allocates 4 chunks
        REQUIRE(chunked.chunk_count() == 4);

        chunked.ensure_size(100);   // Size stays 1000
        chunked.shrink_to_fit();    // Should shrink to 4 chunks (size still 1000)
        REQUIRE(chunked.chunk_count() == 4);
    }

    SECTION("Shrink empty")
    {
        chunked.shrink_to_fit();
        REQUIRE(chunked.empty());
    }
}

TEST_CASE("ChunkedArray<int>: back() and pop_back()", "[ChunkedArray][non-empty][back][pop_back]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;

    for (int i = 0; i < 500; ++i)
    {
        chunked.push_back(i);
        reference.push_back(i);
    }

    SECTION("back() returns last element")
    {
        REQUIRE(chunked.back() == reference.back());
        REQUIRE(chunked.back() == 499);
    }

    SECTION("pop_back() removes elements")
    {
        while (!reference.empty())
        {
            REQUIRE(chunked.back() == reference.back());
            chunked.pop_back();
            reference.pop_back();
            REQUIRE(chunked.size() == reference.size());
        }
        REQUIRE(chunked.empty());
    }
}

TEST_CASE("ChunkedArray<int>: iteration", "[ChunkedArray][non-empty][iterator]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;

    const size_t numElements = kChunkSize * 3 + 50;
    for (size_t i = 0; i < numElements; ++i)
    {
        chunked.push_back(static_cast<int>(i));
        reference.push_back(static_cast<int>(i));
    }

    SECTION("Forward iteration")
    {
        auto chunkedIt = chunked.begin();
        auto refIt = reference.begin();
        while (chunkedIt != chunked.end() && refIt != reference.end())
        {
            REQUIRE(*chunkedIt == *refIt);
            ++chunkedIt;
            ++refIt;
        }
        REQUIRE(chunkedIt == chunked.end());
        REQUIRE(refIt == reference.end());
    }

    SECTION("Range-for iteration")
    {
        std::vector<int> collected;
        for (int val : chunked)
        {
            collected.push_back(val);
        }
        REQUIRE(collected == reference);
    }

    SECTION("const iteration")
    {
        const auto& constChunked = chunked;
        std::vector<int> collected;
        for (int val : constChunked)
        {
            collected.push_back(val);
        }
        REQUIRE(collected == reference);
    }

    SECTION("cbegin/cend iteration")
    {
        std::vector<int> collected;
        for (auto it = chunked.cbegin(); it != chunked.cend(); ++it)
        {
            collected.push_back(*it);
        }
        REQUIRE(collected == reference);
    }
}

TEST_CASE("ChunkedArray<int>: reverse iteration", "[ChunkedArray][non-empty][iterator][reverse]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;

    const size_t numElements = kChunkSize * 2 + 100;
    for (size_t i = 0; i < numElements; ++i)
    {
        chunked.push_back(static_cast<int>(i));
        reference.push_back(static_cast<int>(i));
    }

    SECTION("Decrement from end")
    {
        auto it = chunked.end();
        auto refIt = reference.end();

        while (it != chunked.begin())
        {
            --it;
            --refIt;
            REQUIRE(*it == *refIt);
        }
    }

    SECTION("Post-decrement")
    {
        auto it = chunked.end();
        --it;  // Now at last element

        auto refIt = reference.end();
        --refIt;

        while (it != chunked.begin())
        {
            auto val = *it--;
            auto refVal = *refIt--;
            REQUIRE(val == refVal);
        }
    }
}

TEST_CASE("ChunkedArray<int>: iterator random access", "[ChunkedArray][non-empty][iterator][random-access]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;

    const size_t numElements = kChunkSize * 3 + 50;
    for (size_t i = 0; i < numElements; ++i)
    {
        chunked.push_back(static_cast<int>(i));
        reference.push_back(static_cast<int>(i));
    }

    SECTION("Iterator arithmetic +=")
    {
        auto it = chunked.begin();
        auto refIt = reference.begin();

        it += 100;
        refIt += 100;
        REQUIRE(*it == *refIt);

        it += kChunkSize;  // Cross chunk boundary
        refIt += kChunkSize;
        REQUIRE(*it == *refIt);
    }

    SECTION("Iterator arithmetic -=")
    {
        auto it = chunked.end();
        auto refIt = reference.end();

        it -= 50;
        refIt -= 50;
        REQUIRE(*it == *refIt);

        it -= kChunkSize;  // Cross chunk boundary
        refIt -= kChunkSize;
        REQUIRE(*it == *refIt);
    }

    SECTION("Iterator difference")
    {
        auto start = chunked.begin();
        auto end = chunked.end();
        REQUIRE((end - start) == static_cast<std::ptrdiff_t>(numElements));

        auto mid = start + 100;
        REQUIRE((mid - start) == 100);
        REQUIRE((end - mid) == static_cast<std::ptrdiff_t>(numElements - 100));
    }

    SECTION("Iterator comparison")
    {
        auto it1 = chunked.begin();
        auto it2 = chunked.begin() + 100;
        auto it3 = chunked.end();

        REQUIRE(it1 < it2);
        REQUIRE(it2 < it3);
        REQUIRE(it1 <= it2);
        REQUIRE(it2 <= it2);
        REQUIRE(it3 > it2);
        REQUIRE(it3 >= it3);
        REQUIRE(it1 == it1);
        REQUIRE(it1 != it2);
    }

    SECTION("Iterator operator[]")
    {
        auto it = chunked.begin();
        for (size_t i = 0; i < 10; ++i)
        {
            REQUIRE(it[static_cast<std::ptrdiff_t>(i)] == static_cast<int>(i));
        }
    }
}

TEST_CASE("ChunkedArray<int>: GetChunkSpan", "[ChunkedArray][non-empty][GetChunkSpan]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    SECTION("Empty container returns empty span")
    {
        auto span = chunked.GetChunkSpan(0);
        REQUIRE(span.empty());
    }

    SECTION("Invalid chunk index returns empty span")
    {
        chunked.ensure_size(100);
        auto span = chunked.GetChunkSpan(100);
        REQUIRE(span.empty());
    }

    SECTION("Full chunk span")
    {
        chunked.ensure_size(kChunkSize * 2);
        auto span0 = chunked.GetChunkSpan(0);
        REQUIRE(span0.size() == kChunkSize);

        // First chunk should be full
        for (size_t i = 0; i < kChunkSize; ++i)
        {
            span0[static_cast<std::ptrdiff_t>(i)] = static_cast<int>(i);
        }
        for (size_t i = 0; i < kChunkSize; ++i)
        {
            REQUIRE(chunked[i] == static_cast<int>(i));
        }
    }

    SECTION("Partial last chunk span")
    {
        const size_t numElements = kChunkSize + 100;
        chunked.ensure_size(numElements);

        auto span0 = chunked.GetChunkSpan(0);
        REQUIRE(span0.size() == kChunkSize);

        auto span1 = chunked.GetChunkSpan(1);
        REQUIRE(span1.size() == 100);  // Only 100 elements in last chunk
    }
}

TEST_CASE("ChunkedArray<struct>: complex types", "[ChunkedArray][non-empty][complex]")
{
    struct Point3d {
        double x = 0, y = 0, z = 0;
        Point3d() = default;
        Point3d(double a, double b, double c) : x(a), y(b), z(c) {}
        double sum() const { return x + y + z; }
    };

    ent::ChunkedArray<Point3d, kChunkSize> chunked;
    std::vector<Point3d> reference;

    SECTION("push_back and access")
    {
        for (int i = 0; i < 500; ++i)
        {
            Point3d p(i * 1.0, i * 2.0, i * 3.0);
            chunked.push_back(p);
            reference.push_back(p);
        }

        REQUIRE(chunked.size() == reference.size());
        for (size_t i = 0; i < reference.size(); ++i)
        {
            REQUIRE(chunked[i].x == reference[i].x);
            REQUIRE(chunked[i].y == reference[i].y);
            REQUIRE(chunked[i].z == reference[i].z);
        }
    }

    SECTION("emplace_back with args")
    {
        for (int i = 0; i < 500; ++i)
        {
            chunked.emplace_back(i * 1.0, i * 2.0, i * 3.0);
            reference.emplace_back(i * 1.0, i * 2.0, i * 3.0);
        }

        REQUIRE(chunked.size() == reference.size());
        for (size_t i = 0; i < reference.size(); ++i)
        {
            REQUIRE(chunked[i].sum() == reference[i].sum());
        }
    }
}

// =============================================================================
// resize()
// =============================================================================

TEST_CASE("ChunkedArray<int>: resize()", "[ChunkedArray][non-empty][resize]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    SECTION("resize to 0 on empty is no-op")
    {
        chunked.resize(0);
        REQUIRE(chunked.empty());
    }

    SECTION("resize grow from empty")
    {
        chunked.resize(500);
        REQUIRE(chunked.size() == 500);
        REQUIRE(chunked.chunk_count() == 2);
    }

    SECTION("resize grow preserves existing elements")
    {
        for (int i = 0; i < 100; ++i) chunked.push_back(i);
        chunked.resize(500);
        REQUIRE(chunked.size() == 500);
        for (int i = 0; i < 100; ++i) REQUIRE(chunked[i] == i);
    }

    SECTION("resize shrink reduces size and subsequent push_back works")
    {
        for (int i = 0; i < 500; ++i) chunked.push_back(i);
        chunked.resize(100);
        REQUIRE(chunked.size() == 100);
        for (int i = 0; i < 100; ++i) REQUIRE(chunked[i] == i);

        chunked.push_back(999);
        REQUIRE(chunked.size() == 101);
        REQUIRE(chunked[100] == 999);
    }

    SECTION("resize to exact chunk boundary then push_back stays in existing chunk")
    {
        for (size_t i = 0; i < kChunkSize + kChunkSize / 2; ++i)
            chunked.push_back(static_cast<int>(i));
        chunked.resize(kChunkSize); // shrink to exact chunk boundary
        REQUIRE(chunked.size() == kChunkSize);
        chunked.push_back(777);
        REQUIRE(chunked.size() == kChunkSize + 1);
        REQUIRE(chunked[kChunkSize] == 777);
        REQUIRE(chunked.chunk_count() == 2); // no new chunk allocated
    }

    SECTION("resize same size is no-op")
    {
        for (int i = 0; i < 200; ++i) chunked.push_back(i);
        chunked.resize(200);
        REQUIRE(chunked.size() == 200);
        for (int i = 0; i < 200; ++i) REQUIRE(chunked[i] == i);
    }

    SECTION("resize(n, value) grow fills new slots with value")
    {
        for (int i = 0; i < 50; ++i) chunked.push_back(i);
        chunked.resize(200, -1);
        REQUIRE(chunked.size() == 200);
        for (int i = 0; i < 50; ++i)  REQUIRE(chunked[i] == i);
        for (int i = 50; i < 200; ++i) REQUIRE(chunked[i] == -1);
    }

    SECTION("resize(n, value) grow across multiple chunk boundaries fills correctly")
    {
        chunked.resize(static_cast<size_t>(kChunkSize) * 3, 42);
        REQUIRE(chunked.size() == kChunkSize * 3);
        for (size_t i = 0; i < kChunkSize * 3; ++i) REQUIRE(chunked[i] == 42);
    }

    SECTION("resize(n, value) shrink ignores value and preserves leading elements")
    {
        for (int i = 0; i < 200; ++i) chunked.push_back(i);
        chunked.resize(50, -999);
        REQUIRE(chunked.size() == 50);
        for (int i = 0; i < 50; ++i) REQUIRE(chunked[i] == i);
    }

    SECTION("resize shrink then resize grow re-expands and new slots get value")
    {
        for (size_t i = 0; i < 300; ++i) chunked.push_back(static_cast<int>(i));
        chunked.resize(100);
        chunked.resize(300, 0);
        REQUIRE(chunked.size() == 300);
        for (int i = 0; i < 100; ++i) REQUIRE(chunked[i] == i);
        for (int i = 100; i < 300; ++i) REQUIRE(chunked[i] == 0);
    }
}

// =============================================================================
// Iterator correctness: empty container
// =============================================================================

TEST_CASE("ChunkedArray<int> iterator: empty container", "[ChunkedArray][non-empty][iterator][empty]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    SECTION("begin() == end() on empty container")
    {
        REQUIRE(chunked.begin() == chunked.end());
        REQUIRE(chunked.cbegin() == chunked.cend());
    }

    SECTION("end() - begin() == 0 on empty container")
    {
        REQUIRE((chunked.end() - chunked.begin()) == 0);
    }

    SECTION("default-constructed iterator is well-formed (compiles and is not UB)")
    {
        ent::ChunkedArray<int, kChunkSize>::iterator defaultIt;
        ent::ChunkedArray<int, kChunkSize>::const_iterator defaultCIt;
        (void)defaultIt;
        (void)defaultCIt;
    }
}

// =============================================================================
// Iterator correctness: operator== / operator< consistency
// =============================================================================

TEST_CASE("ChunkedArray<int> iterator: operator== consistent with operator<", "[ChunkedArray][non-empty][iterator][ordering]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    for (int i = 0; i < static_cast<int>(kChunkSize) * 3; ++i) chunked.push_back(i);

    SECTION("Equal iterators: a == b and neither a < b nor b < a")
    {
        auto it1 = chunked.begin() + 100;
        auto it2 = chunked.begin() + 100;
        REQUIRE(it1 == it2);
        REQUIRE(!(it1 < it2));
        REQUIRE(!(it2 < it1));
    }

    SECTION("Unequal iterators: a != b and exactly one of a < b or b < a")
    {
        auto it1 = chunked.begin() + 100;
        auto it2 = chunked.begin() + 200;
        REQUIRE(it1 != it2);
        REQUIRE(it1 < it2);
        REQUIRE(!(it2 < it1));
    }

    SECTION("end() == end() (same container)")
    {
        auto e1 = chunked.end();
        auto e2 = chunked.end();
        REQUIRE(e1 == e2);
        REQUIRE(!(e1 < e2));
        REQUIRE(!(e2 < e1));
    }

    SECTION("end() - begin() equals size()")
    {
        REQUIRE((chunked.end() - chunked.begin()) == static_cast<std::ptrdiff_t>(chunked.size()));
    }
}

// =============================================================================
// Iterator correctness: position invariant under increment / decrement
// =============================================================================

TEST_CASE("ChunkedArray<int> iterator: position stays consistent", "[ChunkedArray][non-empty][iterator][position]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    const int N = static_cast<int>(kChunkSize) * 3 + 50;
    for (int i = 0; i < N; ++i) chunked.push_back(i);

    SECTION("After n pre-increments from begin(), it - begin() == n")
    {
        auto it = chunked.begin();
        for (int n = 0; n < N; ++n)
        {
            REQUIRE((it - chunked.begin()) == static_cast<std::ptrdiff_t>(n));
            ++it;
        }
        REQUIRE(it == chunked.end());
    }

    SECTION("After n post-increments from begin(), it - begin() == n")
    {
        auto it = chunked.begin();
        for (int n = 0; n < N; ++n)
        {
            auto old = it++;
            REQUIRE((old - chunked.begin()) == static_cast<std::ptrdiff_t>(n));
        }
        REQUIRE(it == chunked.end());
    }

    SECTION("After n pre-decrements from end(), end() - it == n")
    {
        auto it = chunked.end();
        for (int n = 1; n <= N; ++n)
        {
            --it;
            REQUIRE((it - chunked.begin()) == static_cast<std::ptrdiff_t>(N - n));
        }
        REQUIRE(it == chunked.begin());
    }

    SECTION("--end() dereferences to back()")
    {
        auto it = chunked.end();
        --it;
        REQUIRE(*it == chunked.back());
        REQUIRE((it - chunked.begin()) == static_cast<std::ptrdiff_t>(N - 1));
    }

    SECTION("operator+= then operator-= returns to original position")
    {
        auto it = chunked.begin() + 50;
        it += static_cast<std::ptrdiff_t>(kChunkSize) * 2;
        it -= static_cast<std::ptrdiff_t>(kChunkSize) * 2;
        REQUIRE((it - chunked.begin()) == 50);
        REQUIRE(*it == 50);
    }
}

// =============================================================================
// Iterator correctness: decrement across chunk boundaries
// =============================================================================

TEST_CASE("ChunkedArray<int> iterator: decrement across chunk boundaries", "[ChunkedArray][non-empty][iterator][boundary]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    const size_t N = kChunkSize * 3;
    for (size_t i = 0; i < N; ++i) chunked.push_back(static_cast<int>(i));

    SECTION("Decrement from first element of chunk 1 lands on last element of chunk 0")
    {
        auto it = chunked.begin() + static_cast<std::ptrdiff_t>(kChunkSize);
        REQUIRE(*it == static_cast<int>(kChunkSize));
        --it;
        REQUIRE(*it == static_cast<int>(kChunkSize - 1));
        REQUIRE((it - chunked.begin()) == static_cast<std::ptrdiff_t>(kChunkSize - 1));
    }

    SECTION("Decrement from first element of chunk 2 lands on last element of chunk 1")
    {
        auto it = chunked.begin() + static_cast<std::ptrdiff_t>(kChunkSize * 2);
        REQUIRE(*it == static_cast<int>(kChunkSize * 2));
        --it;
        REQUIRE(*it == static_cast<int>(kChunkSize * 2 - 1));
        REQUIRE((it - chunked.begin()) == static_cast<std::ptrdiff_t>(kChunkSize * 2 - 1));
    }

    SECTION("Increment from last element of chunk 0 lands on first element of chunk 1")
    {
        auto it = chunked.begin() + static_cast<std::ptrdiff_t>(kChunkSize - 1);
        REQUIRE(*it == static_cast<int>(kChunkSize - 1));
        ++it;
        REQUIRE(*it == static_cast<int>(kChunkSize));
        REQUIRE((it - chunked.begin()) == static_cast<std::ptrdiff_t>(kChunkSize));
    }

    SECTION("operator[] on iterator spanning chunk boundaries matches operator[]")
    {
        auto it = chunked.begin();
        for (size_t i = 0; i < N; ++i)
        {
            REQUIRE(it[static_cast<std::ptrdiff_t>(i)] == chunked[i]);
        }
    }
}

// =============================================================================
// Iterator correctness: commutative operator+ and const_iterator conversion
// =============================================================================

TEST_CASE("ChunkedArray<int> iterator: commutative n + it == it + n", "[ChunkedArray][non-empty][iterator][operator+]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    for (int i = 0; i < 300; ++i) chunked.push_back(i);

    auto it = chunked.begin();
    auto it1 = it + 100;
    auto it2 = 100 + it;
    REQUIRE(it1 == it2);
    REQUIRE(*it1 == *it2);
    REQUIRE(*it1 == 100);
}

TEST_CASE("ChunkedArray<int> iterator: const_iterator from mutable iterator", "[ChunkedArray][non-empty][iterator][const]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    for (int i = 0; i < 300; ++i) chunked.push_back(i);

    SECTION("const_iterator constructed from iterator keeps same position")
    {
        ent::ChunkedArray<int, kChunkSize>::iterator it = chunked.begin() + 100;
        ent::ChunkedArray<int, kChunkSize>::const_iterator cit = it;
        REQUIRE(*cit == *it);
        REQUIRE((cit - chunked.cbegin()) == 100);
    }

    SECTION("iterator and const_iterator at same position compare equal")
    {
        auto it  = chunked.begin() + 50;
        auto cit = chunked.cbegin() + 50;
        REQUIRE(it == cit);
        REQUIRE(!(it < cit));
        REQUIRE(!(cit < it));
        REQUIRE((it - cit) == 0);
    }

    SECTION("const_iterator arithmetic is independent of mutable iterator")
    {
        ent::ChunkedArray<int, kChunkSize>::iterator it = chunked.begin();
        ent::ChunkedArray<int, kChunkSize>::const_iterator cit = it;
        it += 100;
        REQUIRE((it - chunked.begin()) == 100);
        REQUIRE((cit - chunked.cbegin()) == 0); // cit unaffected
    }
}

// =============================================================================
// Standard algorithm compatibility
// =============================================================================

TEST_CASE("ChunkedArray<int> iterator: std::distance", "[ChunkedArray][non-empty][iterator][std]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    const size_t N = kChunkSize * 4 + 77;
    for (size_t i = 0; i < N; ++i) chunked.push_back(static_cast<int>(i));

    SECTION("std::distance from begin() to end() equals size()")
    {
        REQUIRE(static_cast<size_t>(std::distance(chunked.begin(), chunked.end())) == N);
    }

    SECTION("std::distance between arbitrary iterators across chunks")
    {
        auto it1 = chunked.begin() + 50;
        auto it2 = chunked.begin() + static_cast<std::ptrdiff_t>(kChunkSize) + 50;
        REQUIRE(std::distance(it1, it2) == static_cast<std::ptrdiff_t>(kChunkSize));
    }
}

TEST_CASE("ChunkedArray<int> iterator: std::sort", "[ChunkedArray][non-empty][iterator][std]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    std::vector<int> reference;
    const int N = static_cast<int>(kChunkSize) * 3 + 50;
    for (int i = N - 1; i >= 0; --i)
    {
        chunked.push_back(i);
        reference.push_back(i);
    }

    SECTION("std::sort produces same order as sorting a reference vector")
    {
        std::sort(chunked.begin(), chunked.end());
        std::sort(reference.begin(), reference.end());
        REQUIRE(chunked.size() == reference.size());
        for (size_t i = 0; i < reference.size(); ++i)
            REQUIRE(chunked[i] == reference[i]);
    }

    SECTION("std::is_sorted after std::sort")
    {
        std::sort(chunked.begin(), chunked.end());
        REQUIRE(std::is_sorted(chunked.begin(), chunked.end()));
    }
}

TEST_CASE("ChunkedArray<int> iterator: std::find and std::reverse", "[ChunkedArray][non-empty][iterator][std]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;
    const int N = static_cast<int>(kChunkSize) * 2 + 50;
    for (int i = 0; i < N; ++i) chunked.push_back(i);

    SECTION("std::find locates element in first chunk")
    {
        auto it = std::find(chunked.begin(), chunked.end(), 100);
        REQUIRE(it != chunked.end());
        REQUIRE(*it == 100);
        REQUIRE((it - chunked.begin()) == 100);
    }

    SECTION("std::find locates element in second chunk")
    {
        int target = static_cast<int>(kChunkSize) + 10;
        auto it = std::find(chunked.begin(), chunked.end(), target);
        REQUIRE(it != chunked.end());
        REQUIRE(*it == target);
        REQUIRE((it - chunked.begin()) == static_cast<std::ptrdiff_t>(target));
    }

    SECTION("std::find returns end() for missing element")
    {
        REQUIRE(std::find(chunked.begin(), chunked.end(), -1) == chunked.end());
    }

    SECTION("std::reverse mirrors elements")
    {
        std::vector<int> reference(chunked.begin(), chunked.end());
        std::reverse(chunked.begin(), chunked.end());
        std::reverse(reference.begin(), reference.end());
        for (int i = 0; i < N; ++i) REQUIRE(chunked[i] == reference[i]);
    }
}

// =============================================================================
// pop_back write-pointer invariants (covers the specific bug fix)
// =============================================================================

TEST_CASE("ChunkedArray<int>: pop_back then push_back write-pointer invariants", "[ChunkedArray][non-empty][pop_back][write_ptr]")
{
    ent::ChunkedArray<int, kChunkSize> chunked;

    SECTION("pop_back all elements then push_back rebuilds correctly")
    {
        for (int i = 0; i < 100; ++i) chunked.push_back(i);
        while (!chunked.empty()) chunked.pop_back();
        REQUIRE(chunked.empty());

        for (int i = 0; i < 100; ++i) chunked.push_back(i * 2);
        REQUIRE(chunked.size() == 100);
        for (int i = 0; i < 100; ++i) REQUIRE(chunked[i] == i * 2);
    }

    SECTION("pop_back to exact chunk boundary: next push_back writes to slot 0 of next chunk")
    {
        // Fill exactly 1.5 chunks, then pop down to exactly kChunkSize elements.
        for (size_t i = 0; i < kChunkSize + kChunkSize / 2; ++i)
            chunked.push_back(static_cast<int>(i));
        while (chunked.size() > kChunkSize) chunked.pop_back();
        REQUIRE(chunked.size() == kChunkSize);

        chunked.push_back(777);
        REQUIRE(chunked.size() == kChunkSize + 1);
        REQUIRE(chunked[kChunkSize] == 777);
        REQUIRE(chunked.chunk_count() == 2); // chunk 1 was already allocated, no new alloc
    }

    SECTION("pop_back one past chunk boundary: next push_back rewrites slot kChunkSize-1 of chunk 1")
    {
        for (size_t i = 0; i < kChunkSize * 2; ++i)
            chunked.push_back(static_cast<int>(i));
        REQUIRE(chunked.chunk_count() == 2);

        chunked.pop_back(); // removes element at index kChunkSize*2-1
        REQUIRE(chunked.size() == kChunkSize * 2 - 1);

        chunked.push_back(999);
        REQUIRE(chunked.size() == kChunkSize * 2);
        REQUIRE(chunked[kChunkSize * 2 - 1] == 999);
        REQUIRE(chunked.chunk_count() == 2); // no new chunk allocated
    }

    SECTION("Interleaved pop_back and push_back around chunk boundary keeps data intact")
    {
        for (int i = 0; i < static_cast<int>(kChunkSize) + 5; ++i) chunked.push_back(i);
        for (int rep = 0; rep < 10; ++rep)
        {
            chunked.pop_back();
            chunked.push_back(rep * 100);
        }
        REQUIRE(chunked.size() == static_cast<size_t>(kChunkSize) + 5);
        for (size_t i = 0; i < kChunkSize; ++i)
            REQUIRE(chunked[i] == static_cast<int>(i));
        REQUIRE(chunked.back() == 900); // last rep * 100
    }
}

// =============================================================================
// Non-default-constructible type: pop_back, resize-shrink, clear, move
// =============================================================================

// A type with no default constructor that tracks construction/destruction counts
// and holds a resource (non-trivial destructor).
struct NoDflt {
    explicit NoDflt(int v) : value(v) { ++s_constructed; }
    NoDflt(const NoDflt& o) : value(o.value) { ++s_constructed; }
    NoDflt(NoDflt&& o) noexcept : value(o.value) { ++s_constructed; }
    ~NoDflt() { ++s_destroyed; }
    NoDflt& operator=(const NoDflt&) = default;
    NoDflt& operator=(NoDflt&&) noexcept = default;

    int value;
    static int s_constructed;
    static int s_destroyed;
    static void reset_counts() { s_constructed = s_destroyed = 0; }
    static int live() { return s_constructed - s_destroyed; }
};
int NoDflt::s_constructed = 0;
int NoDflt::s_destroyed   = 0;

TEST_CASE("ChunkedArray<NoDflt>: pop_back does not require default constructor", "[ChunkedArray][non-default-constructible][pop_back]")
{
    NoDflt::reset_counts();
    {
        ent::ChunkedArray<NoDflt, kChunkSize> chunked;

        SECTION("push_back then pop_back destructs element")
        {
            chunked.push_back(NoDflt(42));
            REQUIRE(chunked.size() == 1);
            REQUIRE(chunked.back().value == 42);

            chunked.pop_back();
            REQUIRE(chunked.empty());
            // The element pushed must have been destroyed by pop_back
            REQUIRE(NoDflt::live() == 0);
        }

        SECTION("pop_back across chunk boundary destructs all elements exactly once")
        {
            const int N = static_cast<int>(kChunkSize) + 10;
            for (int i = 0; i < N; ++i) chunked.emplace_back(i);
            REQUIRE(static_cast<int>(chunked.size()) == N);

            while (!chunked.empty()) {
                int expected = static_cast<int>(chunked.size()) - 1;
                REQUIRE(chunked.back().value == expected);
                chunked.pop_back();
            }
            REQUIRE(chunked.empty());
            REQUIRE(NoDflt::live() == 0);
        }

        SECTION("pop_back then push_back reuses slot correctly")
        {
            chunked.emplace_back(10);
            chunked.emplace_back(20);
            chunked.pop_back();
            REQUIRE(chunked.size() == 1);
            REQUIRE(chunked.back().value == 10);

            chunked.emplace_back(99);
            REQUIRE(chunked.size() == 2);
            REQUIRE(chunked.back().value == 99);
            REQUIRE(NoDflt::live() == 2);
        }
    }
    // Destructor must destroy all remaining live elements
    REQUIRE(NoDflt::live() == 0);
}

TEST_CASE("ChunkedArray<NoDflt>: clear destructs all elements", "[ChunkedArray][non-default-constructible][clear]")
{
    NoDflt::reset_counts();
    {
        ent::ChunkedArray<NoDflt, kChunkSize> chunked;
        const int N = static_cast<int>(kChunkSize) * 3 + 7;
        for (int i = 0; i < N; ++i) chunked.emplace_back(i);
        REQUIRE(NoDflt::live() == N);

        chunked.clear();
        REQUIRE(chunked.empty());
        REQUIRE(NoDflt::live() == 0);

        // Re-use after clear
        chunked.emplace_back(42);
        REQUIRE(chunked.size() == 1);
        REQUIRE(chunked.back().value == 42);
        REQUIRE(NoDflt::live() == 1);
    }
    REQUIRE(NoDflt::live() == 0);
}

TEST_CASE("ChunkedArray<NoDflt>: move construction transfers ownership", "[ChunkedArray][non-default-constructible][move]")
{
    NoDflt::reset_counts();
    {
        ent::ChunkedArray<NoDflt, kChunkSize> src;
        for (int i = 0; i < 10; ++i) src.emplace_back(i * 2);
        REQUIRE(NoDflt::live() == 10);

        ent::ChunkedArray<NoDflt, kChunkSize> dst(std::move(src));
        REQUIRE(src.empty());
        REQUIRE(dst.size() == 10);
        REQUIRE(NoDflt::live() == 10); // no extra construction/destruction
        for (int i = 0; i < 10; ++i) REQUIRE(dst[i].value == i * 2);
    }
    REQUIRE(NoDflt::live() == 0);
}

TEST_CASE("ChunkedArray<NoDflt>: move assignment transfers ownership", "[ChunkedArray][non-default-constructible][move]")
{
    NoDflt::reset_counts();
    {
        ent::ChunkedArray<NoDflt, kChunkSize> src;
        for (int i = 0; i < 5; ++i) src.emplace_back(i);

        ent::ChunkedArray<NoDflt, kChunkSize> dst;
        for (int i = 10; i < 15; ++i) dst.emplace_back(i); // these must be destroyed
        REQUIRE(NoDflt::live() == 10);

        dst = std::move(src);
        REQUIRE(src.empty());
        REQUIRE(dst.size() == 5);
        // The 5 elements that were in dst before move-assign must be destroyed
        REQUIRE(NoDflt::live() == 5);
        for (int i = 0; i < 5; ++i) REQUIRE(dst[i].value == i);
    }
    REQUIRE(NoDflt::live() == 0);
}

TEST_CASE("ChunkedArray<NoDflt>: destructor releases all elements", "[ChunkedArray][non-default-constructible][destructor]")
{
    NoDflt::reset_counts();
    {
        ent::ChunkedArray<NoDflt, kChunkSize> chunked;
        const int N = static_cast<int>(kChunkSize) * 2 + 50;
        for (int i = 0; i < N; ++i) chunked.emplace_back(i);
        REQUIRE(NoDflt::live() == N);
    } // destructor runs here
    REQUIRE(NoDflt::live() == 0);
}

// =============================================================================
// Stress test: push/pop cycles across chunk boundaries (catches write pointer bugs)
// =============================================================================

TEST_CASE("ChunkedArray<NoDflt>: stress test push/pop cycles across chunk boundaries", "[ChunkedArray][non-default-constructible][pop_back][stress]")
{
    // This test would have caught the pop_back bug where m_writePtr was destroyed
    // instead of the actual last element. The bug caused element count corruption
    // after many push/pop cycles across chunk boundaries.
    NoDflt::reset_counts();
    {
        ent::ChunkedArray<NoDflt, kChunkSize> chunked;

        // Fill multiple chunks
        const int totalElements = static_cast<int>(kChunkSize) * 4;
        for (int i = 0; i < totalElements; ++i) {
            chunked.emplace_back(i);
        }
        REQUIRE(chunked.size() == totalElements);
        REQUIRE(NoDflt::live() == totalElements);

        // Do many push/pop cycles - this exposes write pointer bugs
        for (int cycle = 0; cycle < 100; ++cycle) {
            // Pop half the elements
            int popCount = totalElements / 2;
            for (int i = 0; i < popCount; ++i) {
                // Note: after first cycle, values are different due to re-push
                chunked.pop_back();
            }

            REQUIRE(chunked.size() == static_cast<size_t>(totalElements - popCount));

            // Push new elements back
            for (int i = 0; i < popCount; ++i) {
                chunked.emplace_back(1000 + cycle * 100 + i);
            }

            REQUIRE(chunked.size() == totalElements);
        }

        // Final verification: all elements should still be valid
        REQUIRE(chunked.size() == totalElements);

        // Clear and verify all destructors run
        chunked.clear();
        REQUIRE(chunked.empty());
        REQUIRE(NoDflt::live() == 0);
    }
    REQUIRE(NoDflt::live() == 0);
}

TEST_CASE("ChunkedArray<NoDflt>: resize(n, value) with non-default-constructible value type",
          "[ChunkedArray][non-default-constructible][resize]")
{
    NoDflt::reset_counts();
    {
        ent::ChunkedArray<NoDflt, kChunkSize> chunked;

        SECTION("grow fills with given value")
        {
            for (int i = 0; i < 50; ++i) chunked.emplace_back(i);
            chunked.resize(200, NoDflt(99));
            REQUIRE(chunked.size() == 200);
            for (int i = 0; i < 50; ++i)  REQUIRE(chunked[i].value == i);
            for (int i = 50; i < 200; ++i) REQUIRE(chunked[i].value == 99);
        }

        SECTION("shrink destroys excess and ignores value")
        {
            for (int i = 0; i < 200; ++i) chunked.emplace_back(i);
            const int liveBeforeShrink = NoDflt::live();
            chunked.resize(50, NoDflt(0));
            REQUIRE(chunked.size() == 50);
            REQUIRE(NoDflt::live() == liveBeforeShrink - 150);
            for (int i = 0; i < 50; ++i) REQUIRE(chunked[i].value == i);
        }
    }
    REQUIRE(NoDflt::live() == 0);
}