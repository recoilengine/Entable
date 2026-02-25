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


    // Match std::vector-like API where practical.
    // Chunks hold raw aligned memory; element lifetimes are managed explicitly so that T
    // is NOT required to be default-constructible for pop_back (or any shrinking operation).
    template <class T, size_t CHUNK_SIZE>
    class ChunkedArray
    {
    public:
        using MyStoredType  = T;
        using MyChunkHelper = ChunkHelper<CHUNK_SIZE>;

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
                if (chunk_index < array->chunk_count()) [[likely]] {
                    PointerType chunkData = array->get_chunk_ptr(chunk_index);
                    current = chunkData;
                    const bool isLast = (chunk_index == array->chunk_count() - 1);
                    const size_t elemsInChunk = isLast
                        ? (array->size() - chunk_index * CHUNK_SIZE)
                        : CHUNK_SIZE;
                    chunk_end = chunkData + elemsInChunk;
                } else {
                    current   = nullptr;
                    chunk_end = nullptr;
                }
            }

            void reload_chunk_from_index(size_t idx) {
                if (idx < array->size()) [[likely]] {
                    chunk_index = MyChunkHelper::ChunkIndex(idx);
                    const size_t o = MyChunkHelper::OffsetIndex(idx);
                    PointerType chunkData = array->get_chunk_ptr(chunk_index);
                    current = chunkData + o;
                    const bool isLast = (chunk_index == array->chunk_count() - 1);
                    const size_t elemsInChunk = isLast
                        ? (array->size() - chunk_index * CHUNK_SIZE)
                        : CHUNK_SIZE;
                    chunk_end = chunkData + elemsInChunk;
                } else {
                    current     = nullptr;
                    chunk_end   = nullptr;
                    chunk_index = array->chunk_count();
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
                    chunk_index = MyChunkHelper::ChunkIndex(idx);
                    const size_t o = MyChunkHelper::OffsetIndex(idx);
                    PointerType chunkData = array->get_chunk_ptr(chunk_index);
                    current = chunkData + o;
                    const bool isLast = (chunk_index == array->chunk_count() - 1);
                    const size_t elemsInChunk = isLast
                        ? (array->size() - chunk_index * CHUNK_SIZE)
                        : CHUNK_SIZE;
                    chunk_end = chunkData + elemsInChunk;
                } else {
                    current     = nullptr;
                    chunk_end   = nullptr;
                    chunk_index = arr ? arr->chunk_count() : 0;
                }
            }

            template<bool IsConst_ = IsConst, std::enable_if_t<IsConst_, int> = 0>
            Iterator(const Iterator<false>& other)
                : current(other.current)
                , chunk_end(other.chunk_end)
                , array(other.array)
                , chunk_index(other.chunk_index)
            {}

            ReferenceType operator*()  const { return *current; }
            PointerType   operator->() const { return current;  }

            // Hot path: single pointer increment + one branch on chunk boundary.
            Iterator& operator++() {
                ++current;
                if (current == chunk_end) [[unlikely]] {
                    ++chunk_index;
                    reload_chunk();
                }
                return *this;
            }

            Iterator operator++(int) {
                Iterator temp = *this;
                ++(*this);
                return temp;
            }

            Iterator& operator--() {
                if (current == nullptr) [[unlikely]] {
                    if (array && array->chunk_count() > 0) [[likely]] {
                        chunk_index = array->chunk_count() - 1;
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

            Iterator operator--(int) {
                Iterator temp = *this;
                --(*this);
                return temp;
            }

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

            Iterator operator+(difference_type offset) const {
                Iterator tmp = *this;
                return tmp += offset;
            }

            Iterator operator-(difference_type offset) const {
                Iterator tmp = *this;
                return tmp -= offset;
            }

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
            bool operator==(const Iterator<OtherIsConst>& other) const noexcept {
                return current == other.current;
            }

            template<bool OtherIsConst>
            bool operator!=(const Iterator<OtherIsConst>& other) const noexcept {
                return current != other.current;
            }

            template<bool OtherIsConst>
            bool operator<(const Iterator<OtherIsConst>& other) const {
                return get_index() < other.get_index();
            }

            template<bool OtherIsConst>
            bool operator>(const Iterator<OtherIsConst>& other) const {
                return get_index() > other.get_index();
            }

            template<bool OtherIsConst>
            bool operator<=(const Iterator<OtherIsConst>& other) const {
                return get_index() <= other.get_index();
            }

            template<bool OtherIsConst>
            bool operator>=(const Iterator<OtherIsConst>& other) const {
                return get_index() >= other.get_index();
            }

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

        void clear() {
            destroy_elements(0, elemCount);
            elemCount  = 0;
            chunks.clear();
            m_writePtr = nullptr;
            m_chunkEnd = nullptr;
        }

        void reserve(size_t count)
        {
            if (count == 0) return;
            const size_t numChunksNeeded = MyChunkHelper::ChunkIndex(count - 1) + 1;
            chunks.reserve(numChunksNeeded);            // reserve outer vector
            while (chunks.size() < numChunksNeeded) {  // allocate raw chunk memory
                chunks.push_back(make_chunk());
            }
            // Do NOT advance elemCount; elements are still uninitialised.
            // Update write pointer in case we are at a fresh boundary.
            update_write_ptr();
        }

        // Reserves only the outer pointer-vector capacity without allocating chunk memory.
        // Useful when the final element count is known but chunks should be lazy-allocated.
        void reserve_chunk_index_capacity(size_t count)
        {
            const size_t numChunksNeeded = (count + CHUNK_SIZE - 1) >> MyChunkHelper::LOG2;
            chunks.reserve(numChunksNeeded);
        }

        void ensure_size(size_t count)
            requires std::is_default_constructible_v<T>
        {
            if (count <= elemCount) [[unlikely]] return;

            const size_t old_count = elemCount;
            elemCount = count;
            allocate_chunks_for(count);
            for (size_t ci = MyChunkHelper::ChunkIndex(old_count);
                 ci < chunks.size(); ++ci)
            {
                T* base   = chunks[ci].get();
                size_t lo = (ci == MyChunkHelper::ChunkIndex(old_count))
                                ? MyChunkHelper::OffsetIndex(old_count) : 0;
                size_t hi = (ci == MyChunkHelper::ChunkIndex(count - 1))
                                ? MyChunkHelper::OffsetIndex(count - 1) + 1
                                : CHUNK_SIZE;
                std::uninitialized_default_construct(base + lo, base + hi);
            }
            update_write_ptr();
        }

        void shrink_to_fit() {
            if (elemCount == 0) [[unlikely]] {
                clear();
                return;
            }
            const size_t numChunksNeeded = MyChunkHelper::ChunkIndex(elemCount - 1) + 1;
            chunks.resize(numChunksNeeded);
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
                elemCount = count;
                allocate_chunks_for(count);
                for (size_t ci = MyChunkHelper::ChunkIndex(old_count);
                     ci < chunks.size(); ++ci)
                {
                    T* base   = chunks[ci].get();
                    size_t lo = (ci == MyChunkHelper::ChunkIndex(old_count))
                                    ? MyChunkHelper::OffsetIndex(old_count) : 0;
                    size_t hi = (ci == MyChunkHelper::ChunkIndex(count - 1))
                                    ? MyChunkHelper::OffsetIndex(count - 1) + 1
                                    : CHUNK_SIZE;
                    std::uninitialized_fill(base + lo, base + hi, value);
                }
                update_write_ptr();
            } else if (count < elemCount) {
                destroy_elements(count, elemCount);
                elemCount = count;
                update_write_ptr();
            }
        }

        decltype(auto) operator[](size_t idx) {
            return chunks[MyChunkHelper::ChunkIndex(idx)].get()
                         [MyChunkHelper::OffsetIndex(idx)];
        }

        decltype(auto) operator[](size_t idx) const {
            return chunks[MyChunkHelper::ChunkIndex(idx)].get()
                         [MyChunkHelper::OffsetIndex(idx)];
        }

        decltype(auto) at(size_t idx) {
            if (idx >= elemCount) [[unlikely]]
                throw std::out_of_range("ChunkedArray::at index out of range");
            return (*this)[idx];
        }

        decltype(auto) at(size_t idx) const {
            if (idx >= elemCount) [[unlikely]]
                throw std::out_of_range("ChunkedArray::at index out of range");
            return (*this)[idx];
        }

        template<typename... Args>
        decltype(auto) emplace_back(Args&&... args) {
            if (m_writePtr == m_chunkEnd) [[unlikely]] {
                allocate_new_chunk();
            }
            T* slot = m_writePtr;
            std::construct_at(slot, std::forward<Args>(args)...);
            ++elemCount;
            ++m_writePtr;
            return *slot;
        }

        void push_back(const T& value) { emplace_back(value);            }
        void push_back(T&& value)      { emplace_back(std::move(value)); }

        decltype(auto) back() const {
            assert(elemCount > 0);
            return (*this)[elemCount - 1];
        }

        decltype(auto) back() {
            assert(elemCount > 0);
            return (*this)[elemCount - 1];
        }

        void pop_back() {
            assert(elemCount > 0);
            --elemCount;
            // Always destroy the old last element (at index elemCount after decrement)
            std::destroy_at(std::addressof((*this)[elemCount]));
            // Update write pointer to point to the slot after the new last element
            update_write_ptr();
        }

        [[nodiscard]] bool   empty()       const noexcept { return elemCount == 0; }
        [[nodiscard]] size_t size()        const noexcept { return elemCount;       }
        [[nodiscard]] size_t chunk_count() const noexcept { return chunks.size();   }

        T* get_chunk_ptr(size_t chunkIndex) noexcept {
            return chunks[chunkIndex].get();
        }

        const T* get_chunk_ptr(size_t chunkIndex) const noexcept {
            return chunks[chunkIndex].get();
        }

        std::span<T> GetChunkSpan(size_t chunkIndex) noexcept {
            if (chunkIndex >= chunks.size()) [[unlikely]] return {};
            T* start = chunks[chunkIndex].get();
            if (chunkIndex == chunks.size() - 1) [[unlikely]] {
                const size_t elemsInLastChunk = elemCount - (chunkIndex * CHUNK_SIZE);
                return {start, elemsInLastChunk};
            }
            return {start, CHUNK_SIZE};
        }

        std::span<const T> GetChunkSpan(size_t chunkIndex) const noexcept {
            if (chunkIndex >= chunks.size()) [[unlikely]] return {};
            const T* start = chunks[chunkIndex].get();
            if (chunkIndex == chunks.size() - 1) [[unlikely]] {
                const size_t elemsInLastChunk = elemCount - (chunkIndex * CHUNK_SIZE);
                return {start, elemsInLastChunk};
            }
            return {start, CHUNK_SIZE};
        }

        iterator       begin()        { return iterator(this, 0); }
        iterator       end()          { return iterator(this, elemCount); }
        const_iterator begin()  const { return const_iterator(this, 0); }
        const_iterator end()    const { return const_iterator(this, elemCount); }
        const_iterator cbegin() const { return const_iterator(this, 0); }
        const_iterator cend()   const { return const_iterator(this, elemCount); }

    private:
        struct ChunkDeleter {
            void operator()(T* p) const noexcept {
                ::operator delete(p, std::align_val_t{alignof(T)});
            }
        };

        using ChunkPtr = std::unique_ptr<T, ChunkDeleter>;

        std::vector<ChunkPtr> chunks{};
        size_t elemCount{};
        T* m_writePtr{};
        T* m_chunkEnd{};

        static ChunkPtr make_chunk() {
            T* raw = static_cast<T*>(
                ::operator new(sizeof(T) * CHUNK_SIZE, std::align_val_t{alignof(T)}));
            return ChunkPtr(raw);
        }

        void allocate_chunks_for(size_t count) {
            if (count == 0) return;
            const size_t numChunksNeeded = MyChunkHelper::ChunkIndex(count - 1) + 1;
            while (chunks.size() < numChunksNeeded) {
                chunks.push_back(make_chunk());
            }
        }

        void allocate_new_chunk() {
            chunks.push_back(make_chunk());
            m_writePtr = chunks.back().get();
            m_chunkEnd = m_writePtr + CHUNK_SIZE;
        }

        void destroy_elements(size_t first, size_t last) noexcept {
            if constexpr (std::is_trivially_destructible_v<T>) {
                return; // no-op: trivial destructors need not be called
            } else {
                if (first >= last) return;
                for (size_t ci = MyChunkHelper::ChunkIndex(first);
                     ci <= MyChunkHelper::ChunkIndex(last - 1); ++ci)
                {
                    T* base   = chunks[ci].get();
                    size_t lo = (ci == MyChunkHelper::ChunkIndex(first))
                                    ? MyChunkHelper::OffsetIndex(first) : 0;
                    size_t hi = (ci == MyChunkHelper::ChunkIndex(last - 1))
                                    ? MyChunkHelper::OffsetIndex(last - 1) + 1
                                    : CHUNK_SIZE;
                    std::destroy(base + lo, base + hi);
                }
            }
        }

        void update_write_ptr() {
            if (chunks.empty()) {
                m_writePtr = nullptr;
                m_chunkEnd = nullptr;
                return;
            }
            const size_t writeChunkIdx = MyChunkHelper::ChunkIndex(elemCount);
            if (writeChunkIdx >= chunks.size()) {
                // elemCount sits on a chunk boundary beyond the last allocated chunk;
                // setting both equal signals emplace_back to allocate a new chunk.
                m_writePtr = nullptr;
                m_chunkEnd = nullptr;
                return;
            }
            T* base    = chunks[writeChunkIdx].get();
            m_writePtr = base + MyChunkHelper::OffsetIndex(elemCount);
            m_chunkEnd = base + CHUNK_SIZE;
        }
    };

} // namespace entable