#pragma once

#include <bit>
#include <cstddef>
#include <iterator>
#include <cassert>
#include <memory>
#include <new>
#include <stdexcept>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef FORCE_INLINE
    #if defined(__clang__) || defined(__GNUC__)
        #define FORCE_INLINE [[gnu::always_inline]] inline
    #elif defined(_MSC_VER)
        #pragma warning(error: 4714)
        #define FORCE_INLINE __forceinline
    #else
        #warning FORCE_INLINE: Unsupported compiler
        #define FORCE_INLINE
    #endif
#endif


namespace entable {

    template <size_t CHUNK_SIZE>
    struct ChunkHelper
    {
        static_assert(std::has_single_bit(CHUNK_SIZE), "CHUNK_SIZE must be a power of two");
        static constexpr size_t LOG2 = std::countr_zero(CHUNK_SIZE);
        static constexpr size_t MASK = CHUNK_SIZE - 1;

        FORCE_INLINE static constexpr size_t ChunkIndex(size_t globalIndex) noexcept
        {
            return globalIndex >> LOG2;
        }

        FORCE_INLINE static constexpr size_t OffsetIndex(size_t globalIndex) noexcept
        {
            return globalIndex & MASK;
        }
    };


    // Chunked array with std::vector-like API.
    //
    // Memory is allocated in fixed-size chunks so that:
    //   - Existing element pointers are never invalidated by push_back/emplace_back.
    //   - Peak memory waste is bounded (at most CHUNK_SIZE - 1 uninitialised slots).
    //
    // Chunks hold raw aligned storage; element lifetimes are managed explicitly so
    // that T is NOT required to be default-constructible for pop_back or shrinking.
    //
    // CHUNK_SIZE must be a power of two (enforced by static_assert).
    template <class T, size_t CHUNK_SIZE = 128>
    class ChunkedArray
    {
    public:
        using value_type      = T;
        using size_type       = size_t;
        using difference_type = std::ptrdiff_t;
        using reference       = T&;
        using const_reference = const T&;
        using pointer         = T*;
        using const_pointer   = const T*;

    private:
        using Helper = ChunkHelper<CHUNK_SIZE>;

    public:

        // end() is represented as current == nullptr.
        template<bool IsConst>
        class Iterator {
        private:
            using ArrayType     = std::conditional_t<IsConst, const ChunkedArray, ChunkedArray>;
            using ReferenceType = std::conditional_t<IsConst, const T&, T&>;
            using PointerType   = std::conditional_t<IsConst, const T*, T*>;

            PointerType current{};      // Pointer into current chunk; nullptr == past-the-end
            PointerType chunk_end{};    // One past the last valid element in the current chunk
            ArrayType*  array{};        // For chunk access in reload and random-access ops
            size_t      chunk_index{};  // Current chunk index

            // Absolute index computed from chunk_index + intra-chunk offset of `current`.
            // Only called for random-access operations, not in the sequential hot path.
            size_t get_index() const noexcept {
                if (current == nullptr) {
                    return array ? array->size() : 0;
                }
                assert(array != nullptr);
                return chunk_index * CHUNK_SIZE +
                       static_cast<size_t>(current - array->get_chunk_ptr(chunk_index));
            }

            void reload_chunk() {
                const size_t chunk_base = chunk_index * CHUNK_SIZE;
                if (chunk_base < array->size()) [[likely]] {
                    PointerType chunkData = array->get_chunk_ptr(chunk_index);
                    current = chunkData;
                    const size_t elemsInChunk = (chunk_base + CHUNK_SIZE <= array->size())
                        ? CHUNK_SIZE
                        : array->size() - chunk_base;
                    chunk_end = chunkData + elemsInChunk;
                } else {
                    current   = nullptr;
                    chunk_end = nullptr;
                }
            }

            void reload_chunk_from_index(size_t idx) {
                if (idx < array->size()) [[likely]] {
                    chunk_index = Helper::ChunkIndex(idx);
                    const size_t o = Helper::OffsetIndex(idx);
                    PointerType chunkData = array->get_chunk_ptr(chunk_index);
                    current = chunkData + o;
                    const size_t chunk_base = chunk_index * CHUNK_SIZE;
                    const size_t elemsInChunk = (chunk_base + CHUNK_SIZE <= array->size())
                        ? CHUNK_SIZE
                        : array->size() - chunk_base;
                    chunk_end = chunkData + elemsInChunk;
                } else {
                    current     = nullptr;
                    chunk_end   = nullptr;
                    // Use size-based calculation so end() chunk_index is correct even
                    // when reserve() has pre-allocated chunks beyond the live elements.
                    chunk_index = array->size() > 0
                        ? Helper::ChunkIndex(array->size() - 1) + 1 : 0;
                }
            }

        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept  = std::random_access_iterator_tag;
            using value_type        = T;
            using difference_type   = std::ptrdiff_t;
            using pointer           = PointerType;
            using reference         = ReferenceType;

            Iterator() = default;

            Iterator(ArrayType* arr, size_t idx)
                : array(arr)
            {
                if (array && idx < array->size()) [[likely]] {
                    chunk_index = Helper::ChunkIndex(idx);
                    const size_t o = Helper::OffsetIndex(idx);
                    PointerType chunkData = array->get_chunk_ptr(chunk_index);
                    current = chunkData + o;
                    const size_t chunk_base = chunk_index * CHUNK_SIZE;
                    const size_t elemsInChunk = (chunk_base + CHUNK_SIZE <= array->size())
                        ? CHUNK_SIZE
                        : array->size() - chunk_base;
                    chunk_end = chunkData + elemsInChunk;
                } else {
                    current     = nullptr;
                    chunk_end   = nullptr;
                    // Use size-based calculation so end() chunk_index is correct even
                    // when reserve() has pre-allocated chunks beyond the live elements.
                    chunk_index = arr && arr->size() > 0
                        ? Helper::ChunkIndex(arr->size() - 1) + 1 : 0;
                }
            }

            template<bool IsConst_ = IsConst, std::enable_if_t<IsConst_, int> = 0>
            Iterator(const Iterator<false>& other)
                : current(other.current)
                , chunk_end(other.chunk_end)
                , array(other.array)
                , chunk_index(other.chunk_index)
            {}

            ReferenceType operator*()  const { assert(current != nullptr && "dereference of end() iterator"); return *current; }
            PointerType   operator->() const { assert(current != nullptr && "dereference of end() iterator"); return current;  }

            // Hot path: single pointer increment + one branch on chunk boundary.
            Iterator& operator++() {
                assert(current != nullptr && "increment of end() iterator");
                ++current;
                if (current == chunk_end) [[unlikely]] {
                    ++chunk_index;
                    reload_chunk();
                }
                return *this;
            }

            Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

            Iterator& operator--() {
                if (current == nullptr) [[unlikely]] {
                    if (array && array->size() > 0) [[likely]] {
                        // chunk_index is one past the last live chunk; step back into it.
                        --chunk_index;
                        reload_chunk();
                        current = chunk_end - 1;
                    }
                    return *this;
                }
                assert(array != nullptr);
                PointerType chunk_start = array->get_chunk_ptr(chunk_index);
                if (current == chunk_start) [[unlikely]] {
                    assert(chunk_index > 0 && "decrement of begin() iterator is undefined");
                    if (chunk_index > 0) [[likely]] {
                        --chunk_index;
                        reload_chunk();
                        current = chunk_end - 1;
                    }
                } else [[likely]] {
                    --current;
                }
                return *this;
            }

            Iterator operator--(int) { Iterator tmp = *this; --(*this); return tmp; }

            Iterator& operator+=(difference_type offset) {
                if (offset == 0) return *this;
                const size_t newIdx = static_cast<size_t>(
                    static_cast<difference_type>(get_index()) + offset);
                reload_chunk_from_index(newIdx);
                return *this;
            }

            Iterator& operator-=(difference_type offset) {
                if (offset == 0) return *this;
                const size_t newIdx = static_cast<size_t>(
                    static_cast<difference_type>(get_index()) - offset);
                reload_chunk_from_index(newIdx);
                return *this;
            }

            Iterator operator+(difference_type offset) const { Iterator tmp = *this; return tmp += offset; }
            Iterator operator-(difference_type offset) const { Iterator tmp = *this; return tmp -= offset; }

            reference operator[](difference_type offset) const {
                return (*array)[static_cast<size_t>(
                    static_cast<difference_type>(get_index()) + offset)];
            }

            template<bool OtherIsConst>
            difference_type operator-(const Iterator<OtherIsConst>& other) const {
                return static_cast<difference_type>(get_index()) -
                       static_cast<difference_type>(other.get_index());
            }

            // Equality uses the current pointer as a unique position identifier.
            template<bool OtherIsConst>
            bool operator==(const Iterator<OtherIsConst>& other) const noexcept { return current == other.current; }
            template<bool OtherIsConst>
            bool operator!=(const Iterator<OtherIsConst>& other) const noexcept { return current != other.current; }
            template<bool OtherIsConst>
            bool operator< (const Iterator<OtherIsConst>& other) const { return get_index() <  other.get_index(); }
            template<bool OtherIsConst>
            bool operator> (const Iterator<OtherIsConst>& other) const { return get_index() >  other.get_index(); }
            template<bool OtherIsConst>
            bool operator<=(const Iterator<OtherIsConst>& other) const { return get_index() <= other.get_index(); }
            template<bool OtherIsConst>
            bool operator>=(const Iterator<OtherIsConst>& other) const { return get_index() >= other.get_index(); }

            template<bool> friend class Iterator;
        };

        template<bool IsConst>
        friend Iterator<IsConst> operator+(
            typename Iterator<IsConst>::difference_type offset,
            const Iterator<IsConst>& it)
        {
            return it + offset;
        }

        using iterator       = Iterator<false>;
        using const_iterator = Iterator<true>;

        // -----------------------------------------------------------------------
        // Construction / destruction
        // -----------------------------------------------------------------------

        ChunkedArray() = default;

        ~ChunkedArray() {
            destroy_elements(0, elemCount);
        }

        ChunkedArray(const ChunkedArray&)            = delete;
        ChunkedArray& operator=(const ChunkedArray&) = delete;

        ChunkedArray(ChunkedArray&& other) noexcept
            : chunks(std::move(other.chunks))
            , elemCount(std::exchange(other.elemCount, 0))
            , m_writePtr(std::exchange(other.m_writePtr, nullptr))
            , m_chunkEnd(std::exchange(other.m_chunkEnd, nullptr))
        {}

        ChunkedArray& operator=(ChunkedArray&& other) noexcept {
            if (this != &other) {
                destroy_elements(0, elemCount);
                chunks     = std::move(other.chunks);
                elemCount  = std::exchange(other.elemCount, 0);
                m_writePtr = std::exchange(other.m_writePtr, nullptr);
                m_chunkEnd = std::exchange(other.m_chunkEnd, nullptr);
            }
            return *this;
        }

        // -----------------------------------------------------------------------
        // Capacity
        // -----------------------------------------------------------------------

        void clear() {
            destroy_elements(0, elemCount);
            elemCount  = 0;
            chunks.clear();
            m_writePtr = nullptr;
            m_chunkEnd = nullptr;
        }

        // Resets the logical size to zero without releasing chunk memory or
        // running destructors (which is correct for trivially destructible types).
        // For non-trivial types, destructors ARE called — memory is simply retained
        // for reuse, avoiding the cost of deallocation and re-allocation next time.
        //
        // Intended for hot-loop patterns where the same working set size recurs
        // repeatedly (e.g. pathfinding scratch buffers): allocate once, reset()
        // between searches, release with clear() or destructor when truly done.
        void reset() {
            destroy_elements(0, elemCount);  // no-op for trivially destructible T
            elemCount = 0;
            update_write_ptr();
        }

        // Pre-allocates chunk memory for at least count elements without constructing any.
        void reserve(size_t count) {
            if (count == 0) return;
            const size_t needed = Helper::ChunkIndex(count - 1) + 1;
            chunks.reserve(needed);
            while (chunks.size() < needed)
                chunks.push_back(make_chunk());
            update_write_ptr();
        }

        // Reserves only the outer pointer-vector capacity without allocating chunk memory.
        // Useful when the final element count is known but chunks should be lazy-allocated.
        void reserve_chunk_index_capacity(size_t count) {
            if (count == 0) return;
            chunks.reserve(Helper::ChunkIndex(count - 1) + 1);
        }

        // Number of elements that can be held in currently allocated chunk storage.
        [[nodiscard]] size_t capacity()    const noexcept { return chunks.size() * CHUNK_SIZE; }
        [[nodiscard]] bool   empty()       const noexcept { return elemCount == 0; }
        [[nodiscard]] size_t size()        const noexcept { return elemCount; }
        // Returns the number of allocated chunks, which may exceed the number of
        // chunks containing live elements after reserve() or reset(). Use
        // for_each_chunk() to iterate only over chunks with live data.
        [[nodiscard]] size_t chunk_count() const noexcept { return chunks.size(); }

        // Grow to exactly count elements, default-constructing any new ones.
        void ensure_size(size_t count)
            requires std::is_default_constructible_v<T>
        {
            if (count <= elemCount) return;
            const size_t old_count = elemCount;
            allocate_chunks_for(count);
            construct_range(old_count, count, [](T* base, size_t lo, size_t hi) {
                std::uninitialized_default_construct(base + lo, base + hi);
            });
        }

        // Releases chunk memory allocated beyond what is needed to hold elemCount elements.
        // reserve() never constructs elements, so no destruction is needed before freeing
        // surplus chunks.
        void shrink_to_fit() {
            const size_t needed = elemCount > 0
                ? Helper::ChunkIndex(elemCount - 1) + 1 : 0;
            chunks.resize(needed);
            chunks.shrink_to_fit();
            update_write_ptr();
        }

        void resize(size_t count)
            requires std::is_default_constructible_v<T>
        {
            if (count > elemCount) {
                ensure_size(count);
            } else if (count < elemCount) {
                destroy_elements(count, elemCount);
                elemCount = count;
                update_write_ptr();
            }
        }

        void resize(size_t count, const T& value) {
            if (count > elemCount) {
                const size_t old_count = elemCount;
                allocate_chunks_for(count);
                construct_range(old_count, count, [&value](T* base, size_t lo, size_t hi) {
                    std::uninitialized_fill(base + lo, base + hi, value);
                });
            } else if (count < elemCount) {
                destroy_elements(count, elemCount);
                elemCount = count;
                update_write_ptr();
            }
        }

        // -----------------------------------------------------------------------
        // Element access
        // -----------------------------------------------------------------------

        reference       operator[](size_t idx)       { return chunks[Helper::ChunkIndex(idx)].get()[Helper::OffsetIndex(idx)]; }
        const_reference operator[](size_t idx) const { return chunks[Helper::ChunkIndex(idx)].get()[Helper::OffsetIndex(idx)]; }

        reference at(size_t idx) {
            if (idx >= elemCount) [[unlikely]] throw std::out_of_range("ChunkedArray::at index out of range");
            return (*this)[idx];
        }
        const_reference at(size_t idx) const {
            if (idx >= elemCount) [[unlikely]] throw std::out_of_range("ChunkedArray::at index out of range");
            return (*this)[idx];
        }

        reference       front()       { assert(elemCount > 0); return (*this)[0]; }
        const_reference front() const { assert(elemCount > 0); return (*this)[0]; }
        reference       back()        { assert(elemCount > 0); return (*this)[elemCount - 1]; }
        const_reference back()  const { assert(elemCount > 0); return (*this)[elemCount - 1]; }

        // -----------------------------------------------------------------------
        // Modifiers
        // -----------------------------------------------------------------------

        template<typename... Args>
        reference emplace_back(Args&&... args) {
            if (m_writePtr == m_chunkEnd) [[unlikely]]
                allocate_new_chunk();
            T* slot = m_writePtr;
            std::construct_at(slot, std::forward<Args>(args)...);
            ++elemCount;
            ++m_writePtr;
            return *slot;
        }

        void push_back(const T& value) { emplace_back(value);            }
        void push_back(T&&      value) { emplace_back(std::move(value)); }

        void pop_back() {
            assert(elemCount > 0);
            --elemCount;
            std::destroy_at(std::addressof((*this)[elemCount]));
            // Fast path: the freed slot and the new write position are in the same
            // chunk, so we can simply step m_writePtr back by one.
            // This holds when the new elemCount is NOT the last slot of its chunk —
            // equivalently when OffsetIndex(elemCount) != CHUNK_SIZE-1 — because if
            // it were, then m_writePtr (which points at slot elemCount+1) would be
            // sitting at the first slot of the *next* chunk (a separate allocation),
            // and --m_writePtr would step before that chunk into unrelated memory.
            if (Helper::OffsetIndex(elemCount) != Helper::MASK) [[likely]] {
                --m_writePtr;
            } else {
                update_write_ptr();
            }
        }

        // Removes the element at idx in O(1) by moving the last element into the
        // vacated slot and popping the (now-duplicate) tail.
        // Invalidates any iterator or index pointing to the last element.
        // Intended as the building block for stable-index or sparse-set layers above
        // this class, which can intercept the move to update their own bookkeeping.
        void swap_remove(size_t idx) {
            assert(idx < elemCount && "swap_remove index out of range");
            if (idx != elemCount - 1)
                (*this)[idx] = std::move((*this)[elemCount - 1]);
            pop_back();
        }

        // -----------------------------------------------------------------------
        // Chunk access
        // -----------------------------------------------------------------------

        T*       get_chunk_ptr(size_t chunk_idx)       noexcept { return chunks[chunk_idx].get(); }
        const T* get_chunk_ptr(size_t chunk_idx) const noexcept { return chunks[chunk_idx].get(); }

        std::span<T> get_chunk_span(size_t chunk_idx) noexcept {
            if (chunk_idx >= chunks.size()) [[unlikely]] return {};
            const size_t chunk_base = chunk_idx * CHUNK_SIZE;
            if (chunk_base >= elemCount) return {};
            return {chunks[chunk_idx].get(), std::min(CHUNK_SIZE, elemCount - chunk_base)};
        }

        std::span<const T> get_chunk_span(size_t chunk_idx) const noexcept {
            if (chunk_idx >= chunks.size()) [[unlikely]] return {};
            const size_t chunk_base = chunk_idx * CHUNK_SIZE;
            if (chunk_base >= elemCount) return {};
            return {chunks[chunk_idx].get(), std::min(CHUNK_SIZE, elemCount - chunk_base)};
        }

        // -----------------------------------------------------------------------
        // Chunk iteration
        //
        // Chunk-first iteration is the canonical fast path for large workloads.
        // The inner loop over each span is pure pointer iteration with no branch
        // overhead, and each span is contiguous — ideal for prefetching, SIMD,
        // and auto-vectorization.
        //
        // Job-system parallelism is trivially expressible:
        //
        //   arr.for_each_chunk_indexed([&](size_t c, std::span<T> chunk) {
        //       jobSystem.schedule([chunk] {
        //           for (T& t : chunk) update(t);
        //       });
        //   });
        //
        // Note: chunk spans must not outlive the ChunkedArray, and the array
        // must not be mutated while spans are in use.
        // -----------------------------------------------------------------------

        // Calls f(std::span<T>) for each live chunk in order.
        template<typename F>
        void for_each_chunk(F&& f) {
            const size_t live = elemCount > 0 ? Helper::ChunkIndex(elemCount - 1) + 1 : 0;
            for (size_t c = 0; c < live; ++c)
                f(get_chunk_span(c));
        }

        template<typename F>
        void for_each_chunk(F&& f) const {
            const size_t live = elemCount > 0 ? Helper::ChunkIndex(elemCount - 1) + 1 : 0;
            for (size_t c = 0; c < live; ++c)
                f(get_chunk_span(c));
        }

        // Calls f(size_t chunkIndex, std::span<T>) for each live chunk in order.
        // The chunk index enables job-system scheduling and per-chunk bookkeeping.
        template<typename F>
        void for_each_chunk_indexed(F&& f) {
            const size_t live = elemCount > 0 ? Helper::ChunkIndex(elemCount - 1) + 1 : 0;
            for (size_t c = 0; c < live; ++c)
                f(c, get_chunk_span(c));
        }

        template<typename F>
        void for_each_chunk_indexed(F&& f) const {
            const size_t live = elemCount > 0 ? Helper::ChunkIndex(elemCount - 1) + 1 : 0;
            for (size_t c = 0; c < live; ++c)
                f(c, get_chunk_span(c));
        }

        iterator       begin()        { return iterator(this, 0); }
        iterator       end()          { return iterator(this, elemCount); }
        const_iterator begin()  const { return const_iterator(this, 0); }
        const_iterator end()    const { return const_iterator(this, elemCount); }
        const_iterator cbegin() const { return const_iterator(this, 0); }
        const_iterator cend()   const { return const_iterator(this, elemCount); }

    private:
        // -----------------------------------------------------------------------
        // Internal storage
        // -----------------------------------------------------------------------

        struct ChunkDeleter {
            void operator()(T* p) const noexcept {
                ::operator delete(p, std::align_val_t{alignof(T)});
            }
        };

        using ChunkPtr = std::unique_ptr<T, ChunkDeleter>;

        std::vector<ChunkPtr> chunks{};
        size_t elemCount{};
        // Cached write position for the emplace_back hot path.
        // Always consistent with elemCount; updated by update_write_ptr().
        T* m_writePtr{};
        T* m_chunkEnd{};

        // -----------------------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------------------

        static ChunkPtr make_chunk() {
            T* raw = static_cast<T*>(
                ::operator new(sizeof(T) * CHUNK_SIZE, std::align_val_t{alignof(T)}));
            return ChunkPtr(raw);
        }

        void allocate_chunks_for(size_t count) {
            if (count == 0) return;
            const size_t needed = Helper::ChunkIndex(count - 1) + 1;
            while (chunks.size() < needed)
                chunks.push_back(make_chunk());
        }

        // Constructs elements in [old_count, new_count) via the provided callable
        // (signature: void(T* base, size_t lo, size_t hi)).
        //
        // elemCount is advanced per-chunk so that destroy_elements() always covers
        // exactly the live elements if the callable throws mid-loop.
        //
        // A scope guard ensures update_write_ptr() runs on exit whether the loop
        // completes normally or unwinds due to a throwing constructor.
        template<typename ConstructFn>
        void construct_range(size_t old_count, size_t new_count, ConstructFn&& fn) {
            // Ensure m_writePtr/m_chunkEnd are always consistent with elemCount on exit,
            // whether the loop completes normally or unwinds due to a throwing constructor.
            struct WriteGuard {
                ChunkedArray* self;
                ~WriteGuard() { self->update_write_ptr(); }
            } guard{this};

            const size_t first_chunk = Helper::ChunkIndex(old_count);
            const size_t last_chunk  = Helper::ChunkIndex(new_count - 1);
            for (size_t ci = first_chunk; ci <= last_chunk; ++ci) {
                T*           base = chunks[ci].get();
                const size_t lo   = (ci == first_chunk) ? Helper::OffsetIndex(old_count)     : 0;
                const size_t hi   = (ci == last_chunk)  ? Helper::OffsetIndex(new_count - 1) + 1
                                                        : CHUNK_SIZE;
                fn(base, lo, hi);
                elemCount = ci * CHUNK_SIZE + hi;
            }
        }

        void allocate_new_chunk() {
            const size_t next_chunk = Helper::ChunkIndex(elemCount);
            if (next_chunk >= chunks.size())
                chunks.push_back(make_chunk());
            T* base = chunks[next_chunk].get();
            m_writePtr = base;
            m_chunkEnd = base + CHUNK_SIZE;
        }

        void destroy_elements(size_t first, size_t last) noexcept {
            if constexpr (std::is_trivially_destructible_v<T>) {
                return;
            } else {
                if (first >= last) return;
                const size_t first_chunk = Helper::ChunkIndex(first);
                const size_t last_chunk  = Helper::ChunkIndex(last - 1);
                for (size_t ci = first_chunk; ci <= last_chunk; ++ci) {
                    T*           base = chunks[ci].get();
                    const size_t lo   = (ci == first_chunk) ? Helper::OffsetIndex(first)     : 0;
                    const size_t hi   = (ci == last_chunk)  ? Helper::OffsetIndex(last - 1) + 1
                                                            : CHUNK_SIZE;
                    std::destroy(base + lo, base + hi);
                }
            }
        }

        // Recomputes m_writePtr/m_chunkEnd from the current elemCount and chunk vector.
        //
        // The sentinel for "emplace_back must allocate" is m_writePtr == m_chunkEnd
        // (nullptr == nullptr when no chunk is available).
        //
        // write_chunk < chunks.size()  -> pre-allocated chunk available; point into it.
        //   Covers both the normal mid-chunk case and the case where reserve() has
        //   pre-allocated beyond the current last element.
        // write_chunk >= chunks.size() -> all chunks fully populated (or none allocated);
        //   signal emplace_back to call allocate_new_chunk().
        void update_write_ptr() noexcept {
            if (chunks.empty()) {
                m_writePtr = nullptr;
                m_chunkEnd = nullptr;
                return;
            }
            const size_t write_chunk = Helper::ChunkIndex(elemCount);
            const size_t write_off   = Helper::OffsetIndex(elemCount);
            if (write_chunk < chunks.size()) {
                T* base    = chunks[write_chunk].get();
                m_writePtr = base + write_off;
                m_chunkEnd = base + CHUNK_SIZE;
            } else {
                m_writePtr = nullptr;
                m_chunkEnd = nullptr;
            }
        }
    };

} // namespace entable