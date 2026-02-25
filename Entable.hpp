#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>

#include "ChunkedArray.hpp"

namespace entable {
	static constexpr size_t DEFAULT_DENSE_CHUNK_SIZE = 1024;
}

namespace utils {
	using namespace entable;

	template<typename... Ts>
	concept MoreThanOneType = (sizeof...(Ts) > 1);

	template <typename T>
	constexpr bool always_false_v = false;

	template<typename TupleType, typename Type>
	struct tuple_contains_type;

	template<typename Type, typename... TupleElementTypes>
	struct tuple_contains_type<std::tuple<TupleElementTypes...>, Type> : std::disjunction<std::is_same<Type, TupleElementTypes>...> {};

	template<typename TupleType, typename Type>
	constexpr inline bool tuple_contains_type_v = tuple_contains_type<TupleType, Type>::value;

	template<typename T, typename Tuple, std::size_t I = 0>
	static constexpr std::size_t tuple_type_index() {
		if constexpr (I == std::tuple_size_v<Tuple>) {
			static_assert(always_false_v<T>, "Type not found in Tuple");
			return I;
		}
		else if constexpr (std::is_same_v<T, std::tuple_element_t<I, Tuple>>) {
			return I;
		}
		else {
			return tuple_type_index<T, Tuple, I + 1>();
		}
	}

	template <typename T, typename Tuple>
	static constexpr size_t tuple_type_index_v = tuple_type_index<T, Tuple>();

	template <typename Fn, typename TupleT>
	void for_each_tuple(Fn&& fn, TupleT&& tp) {
		std::apply(
			[&fn]<typename ...T>(T&& ...args) {
			(fn(std::forward<T>(args)), ...);
		}, std::forward<TupleT>(tp));
	}

	template<typename... Type>
	struct type_list_t {
		using type = type_list_t;
		static constexpr auto size = sizeof...(Type);
	};

	template<typename... Type>
	static constexpr type_list_t<Type...> type_list{};

	template<typename T, typename TypeList>
	struct type_in_list;

	template<typename T, typename... Types>
	struct type_in_list<T, type_list_t<Types...>> {
		static constexpr bool value = (std::is_same_v<T, Types> || ...);
	};

	template<typename T, typename TypeList>
	constexpr bool type_in_list_v = type_in_list<T, TypeList>::value;

	// -------------------------------------------------------------------------
	// Get first type from a parameter pack
	// -------------------------------------------------------------------------
	template<typename... Ts>
	struct FirstComponentImpl {
		using type = typename std::tuple_element<0, std::tuple<Ts...>>::type;
	};

	template<typename... Ts>
	using FirstComponent = typename FirstComponentImpl<Ts...>::type;

	// -------------------------------------------------------------------------
	// Get first index from an index sequence
	// -------------------------------------------------------------------------
	template<size_t First, size_t... Rest>
	constexpr size_t FirstIndexImpl = First;

	template<size_t... Is>
	constexpr size_t FirstIndex = FirstIndexImpl<Is...>;

	// -------------------------------------------------------------------------
	// Check if all types in a type_list are unique
	// -------------------------------------------------------------------------
	template<typename TypeList>
	struct all_types_unique;

	template<>
	struct all_types_unique<type_list_t<>> : std::true_type {};

	template<typename T, typename... Ts>
	struct all_types_unique<type_list_t<T, Ts...>>
		: std::conjunction<std::negation<std::is_same<T, Ts>>..., all_types_unique<type_list_t<Ts...>>> {};

	template<typename TypeList>
	constexpr bool all_types_unique_v = all_types_unique<TypeList>::value;

	// Concept for unique types in a type_list
	template<typename TypeList>
	concept UniqueTypes = all_types_unique_v<TypeList>;
}

namespace entable {
	using namespace utils;

	struct Entity {
		using IdType = uint32_t;
		IdType id = std::numeric_limits<IdType>::max();

		constexpr Entity() = default;
		constexpr explicit Entity(IdType value) noexcept
			: id(value)
		{}

		FORCE_INLINE constexpr bool operator==(const Entity& other) const noexcept { return id == other.id; }
		FORCE_INLINE constexpr auto operator<=>(const Entity& other) const noexcept { return id <=> other.id; }
		FORCE_INLINE explicit constexpr operator IdType() const noexcept { return id; }
	};

	namespace EntityTraits {
		static constexpr Entity::IdType INDEX_BITS = 20;
		static constexpr Entity::IdType VERSION_BITS = sizeof(Entity::IdType) * 8 - INDEX_BITS;
		static constexpr Entity::IdType INDEX_MASK = (1u << INDEX_BITS) - 1u;
		static constexpr Entity::IdType VERSION_MASK = (1u << VERSION_BITS) - 1u;
		static constexpr Entity::IdType INVALID_INDEX = INDEX_MASK;
	}

	static constexpr Entity NullEntity = Entity();

	FORCE_INLINE constexpr bool IsNullEntity(Entity entity) noexcept {
		return entity == NullEntity;
	}

	FORCE_INLINE constexpr auto EntityToIntegral(Entity entity) noexcept {
		return entity.id;
	}

	FORCE_INLINE constexpr auto IntegralToEntity(uint32_t u) noexcept {
		return Entity(u);
	}

	FORCE_INLINE constexpr uint32_t EntityToIndex(Entity entity) noexcept {
		return EntityToIntegral(entity) & EntityTraits::INDEX_MASK;
	}

	FORCE_INLINE constexpr uint32_t EntityToVersion(Entity entity) noexcept {
		return (EntityToIntegral(entity) >> EntityTraits::INDEX_BITS) & EntityTraits::VERSION_MASK;
	}

	FORCE_INLINE constexpr auto EntityToIndexAndVersion(Entity entity) noexcept {
		const auto u = EntityToIntegral(entity);
		return std::make_pair(
			(u & EntityTraits::INDEX_MASK),
			(u >> EntityTraits::INDEX_BITS) & EntityTraits::VERSION_MASK
		);
	}

	FORCE_INLINE constexpr auto ComposeEntity(uint32_t index, uint32_t version) noexcept {
		return Entity((version << EntityTraits::INDEX_BITS) | (index & EntityTraits::INDEX_MASK));
	}

	FORCE_INLINE constexpr uint32_t NextEntityVersion(Entity entity) noexcept {
		if (IsNullEntity(entity)) {
			return EntityToVersion(entity);
		}
		return (EntityToVersion(entity) + 1u) & EntityTraits::VERSION_MASK;
	}

	template <typename TypeList, size_t CHUNK_SIZE = DEFAULT_DENSE_CHUNK_SIZE>
	class Registry;

	template <typename TypedRegistry, typename T>
	class ComponentStorage {
	public:
		template<typename TypeList, size_t CHUNK_SIZE>
		friend class Registry;

		using MyStoredType = T;
		using IndexType = Entity::IdType;
		static constexpr size_t CHUNK_SIZE = TypedRegistry::ChunkSize;
		static constexpr bool IsContiguous = TypedRegistry::IsContiguous;
		using DataStorage = std::conditional_t<IsContiguous, std::vector<T>, ChunkedArray<T, CHUNK_SIZE>>;
		using EntityStorage = std::conditional_t<IsContiguous, std::vector<Entity>, ChunkedArray<Entity, CHUNK_SIZE>>;
		using SparseStorage = std::conditional_t<IsContiguous, std::vector<uint32_t>, ChunkedArray<uint32_t, CHUNK_SIZE>>;

		explicit ComponentStorage(TypedRegistry& r)
			: regPtr(&r)
		{}

	private:
		void Init(IndexType entityIndex, Entity entity) {
			const size_t slot = data.size();
			data.emplace_back();
			slotToEntity.emplace_back(entity);
			EnsureSparseSlot(entityIndex);
			indexToSlot[entityIndex] = static_cast<uint32_t>(slot);
		}

		void Kill(IndexType entityIndex) {
			const size_t slot = indexToSlot[entityIndex];
			const size_t last = data.size() - 1;

			if (slot != last) {
				data[slot] = std::move(data[last]);
				const Entity movedEntity = slotToEntity[last];
				slotToEntity[slot] = movedEntity;
				indexToSlot[EntityToIndex(movedEntity)] = static_cast<uint32_t>(slot);
			}

			data.pop_back();
			slotToEntity.pop_back();
		}

		// Unchecked - caller guarantees entity is live.
		template<typename... Args>
		void Set(IndexType entityIndex, Args&&... args) {
			data[indexToSlot[entityIndex]] = MyStoredType{ std::forward<Args>(args)... };
		}

		// Unchecked - caller guarantees entity is live.
		[[nodiscard]] decltype(auto) Get(IndexType entityIndex) {
			return data[indexToSlot[entityIndex]];
		}

		[[nodiscard]] decltype(auto) Get(IndexType entityIndex) const {
			return data[indexToSlot[entityIndex]];
		}

		[[nodiscard]] decltype(auto) GetByDenseSlotUnchecked(size_t slot) {
			return data[slot];
		}

		[[nodiscard]] decltype(auto) GetByDenseSlotUnchecked(size_t slot) const {
			return data[slot];
		}

		[[nodiscard]] MyStoredType* TryGet(IndexType entityIndex) noexcept {
			return &data[indexToSlot[entityIndex]];
		}

		[[nodiscard]] const MyStoredType* TryGet(IndexType entityIndex) const noexcept {
			return &data[indexToSlot[entityIndex]];
		}

		// Returns the size of the dense data array (number of components stored)
		[[nodiscard]] size_t DenseSize() const noexcept {
			return data.size();
		}

		void Clear() noexcept {
			data.clear();
			slotToEntity.clear();
			indexToSlot.clear();
		}

		[[nodiscard]] const MyStoredType* GetPointerAt(size_t slot) const noexcept {
			return &data[slot];
		}

		[[nodiscard]] auto GetDataSpan() noexcept {
			std::vector<std::span<MyStoredType>> result;
			if constexpr (IsContiguous) {
				// For contiguous storage (std::vector), emulate chunks of DEFAULT_DENSE_CHUNK_SIZE
				// This ensures consistent chunk-based parallel iteration across all storage types
				const size_t totalSize = data.size();
				const size_t numChunks = (totalSize + DEFAULT_DENSE_CHUNK_SIZE - 1) / DEFAULT_DENSE_CHUNK_SIZE;
				result.reserve(numChunks);
				for (size_t i = 0; i < numChunks; ++i) {
					const size_t start = i * DEFAULT_DENSE_CHUNK_SIZE;
					const size_t end = std::min(start + DEFAULT_DENSE_CHUNK_SIZE, totalSize);
					result.emplace_back(data.data() + start, end - start);
				}
			} else {
				// For chunked storage, return span per chunk
				const size_t numChunks = data.chunk_count();
				const size_t totalSize = data.size();
				result.reserve(numChunks);
				for (size_t i = 0; i < numChunks; ++i) {
					MyStoredType* chunkPtr = data.get_chunk_ptr(i);
					const bool isLast = (i == numChunks - 1);
					const size_t elemsInChunk = isLast
						? (totalSize - i * CHUNK_SIZE)
						: CHUNK_SIZE;
					result.emplace_back(chunkPtr, elemsInChunk);
				}
			}
			return result;
		}

		[[nodiscard]] auto GetDataSpan() const noexcept {
			std::vector<std::span<const MyStoredType>> result;
			if constexpr (IsContiguous) {
				// For contiguous storage (std::vector), emulate chunks of DEFAULT_DENSE_CHUNK_SIZE
				// This ensures consistent chunk-based parallel iteration across all storage types
				const size_t totalSize = data.size();
				const size_t numChunks = (totalSize + DEFAULT_DENSE_CHUNK_SIZE - 1) / DEFAULT_DENSE_CHUNK_SIZE;
				result.reserve(numChunks);
				for (size_t i = 0; i < numChunks; ++i) {
					const size_t start = i * DEFAULT_DENSE_CHUNK_SIZE;
					const size_t end = std::min(start + DEFAULT_DENSE_CHUNK_SIZE, totalSize);
					result.emplace_back(data.data() + start, end - start);
				}
			} else {
				// For chunked storage, return span per chunk
				const size_t numChunks = data.chunk_count();
				const size_t totalSize = data.size();
				result.reserve(numChunks);
				for (size_t i = 0; i < numChunks; ++i) {
					const MyStoredType* chunkPtr = data.get_chunk_ptr(i);
					const bool isLast = (i == numChunks - 1);
					const size_t elemsInChunk = isLast
						? (totalSize - i * CHUNK_SIZE)
						: CHUNK_SIZE;
					result.emplace_back(chunkPtr, elemsInChunk);
				}
			}
			return result;
		}

		void EnsureSparseSlot(IndexType entityIndex) {
			if constexpr (IsContiguous) {
				if (indexToSlot.size() <= static_cast<size_t>(entityIndex)) {
					indexToSlot.resize(static_cast<size_t>(entityIndex) + 1);
				}
			} else {
				indexToSlot.ensure_size(static_cast<size_t>(entityIndex) + 1);
			}
		}

	private:
		DataStorage   data;
		EntityStorage slotToEntity;
		SparseStorage indexToSlot;
		TypedRegistry* regPtr = nullptr;
	};

	// -------------------------------------------------------------------------
	// Registry
	// -------------------------------------------------------------------------
	template<typename TypeList, size_t CHUNK_SIZE>
	class Registry;

	template<typename... Cs, size_t CHUNK_SIZE>
	class Registry<type_list_t<Cs...>, CHUNK_SIZE> {
	public:
		using Self = Registry<type_list_t<Cs...>, CHUNK_SIZE>;
		using TypesList = type_list_t<Cs...>;
		using StoragesTuple = std::tuple<ComponentStorage<Self, Cs>...>;
		static constexpr size_t ChunkSize = CHUNK_SIZE;
		static constexpr bool IsContiguous = (CHUNK_SIZE == 0);

		template <typename, typename>
		friend class ComponentStorage;

	public:
		Registry()
			: storages(ComponentStorage<Self, Cs>(*this)...)
		{
			static_assert(NUM_COMPONENTS > 0, "Define at least one component at Registry type level");
			(ValidateChunk<Cs>(), ...);
			(ValidateDefaultInitializable<Cs>(), ...);
		}

		~Registry() { Clear(); }
		Registry(const Registry&) = delete;
		Registry(Registry&&) noexcept = delete;
		Registry& operator=(const Registry&) = delete;
		Registry& operator=(Registry&&) noexcept = delete;

	public:
		Entity CreateEntity() {
			Entity entity;

			if (fSize > 0) {
				// Reuse from free list
				const auto i = fNext;
				fNext = EntityToIndex(entities[i]);  // Get next from current head
				--fSize;

				const auto v = EntityToVersion(entities[i]);
				entities[i] = ComposeEntity(i, v);   // Mark as live
				entity = entities[i];

				for_each_tuple([&entity, i](auto& s) {
					s.Init(i, entity);
				}, storages);
			}
			else {
				// Allocate fresh slot
				if (entities.size() >= EntityTraits::INVALID_INDEX) {
					throw std::runtime_error("Can't create Entity (too many entities)");
				}
				entity = entities.emplace_back(ComposeEntity(
					static_cast<uint32_t>(entities.size()),
					0u
				));

				for_each_tuple([&entity](auto& s) {
					s.Init(EntityToIndex(entity), entity);
				}, storages);
			}

			return entity;
		}

		void DestroyEntity(Entity entity) {
			CheckEntity(entity);

			const uint32_t index = EntityToIndex(entity);

			for_each_tuple([&entity, index](auto& s) {
				s.Kill(index);
			}, storages);

			// Add to free list - store fNext in freed slot's index bits
			const uint32_t nextVer = NextEntityVersion(entities[index]);
			entities[index] = ComposeEntity(fNext, nextVer);
			fNext = index;
			++fSize;
		}

		[[nodiscard]] bool IsValidEntity(Entity entity) const noexcept {
			if (IsNullEntity(entity)) return false;
			const auto i = EntityToIndex(entity);
			if (i >= entities.size()) return false;
			// Entity is valid if it's live (index matches position)
			return entities[i] == entity;
		}

		// -----------------------------------------------------------------
		// Type-based API (only available when all component types are unique)
		// -----------------------------------------------------------------

		template<typename C, typename... Args>
		void Set(Entity entity, Args&&... args)
			requires UniqueTypes<TypesList>
		{
			GetStorage<C>().Set(EntityToIndex(entity), std::forward<Args>(args)...);
		}

		template<typename C, typename... Args>
		void SetSafe(Entity entity, Args&&... args)
			requires UniqueTypes<TypesList>
		{
			CheckEntity(entity);
			GetStorage<C>().Set(EntityToIndex(entity), std::forward<Args>(args)...);
		}

		template<typename C>
		[[nodiscard]] decltype(auto) Get(Entity entity)
			requires UniqueTypes<TypesList>
		{
			return GetStorage<C>().Get(EntityToIndex(entity));
		}

		template<typename C>
		[[nodiscard]] decltype(auto) Get(Entity entity) const
			requires UniqueTypes<TypesList>
		{
			return GetStorage<C>().Get(EntityToIndex(entity));
		}

		template<typename... Cts>
		[[nodiscard]] decltype(auto) Get(Entity entity) requires MoreThanOneType<Cts...> && UniqueTypes<TypesList> {
			const auto idx = EntityToIndex(entity);
			return std::forward_as_tuple(GetStorage<Cts>().Get(idx)...);
		}

		template<typename... Cts>
		[[nodiscard]] decltype(auto) Get(Entity entity) const requires MoreThanOneType<Cts...> && UniqueTypes<TypesList> {
			const auto idx = EntityToIndex(entity);
			return std::forward_as_tuple(GetStorage<Cts>().Get(idx)...);
		}

		template<typename... Cts>
		[[nodiscard]] decltype(auto) GetNonEmpty(Entity entity)
			requires UniqueTypes<TypesList>
		{
			return std::tuple_cat(ForwardNonEmpty<Cts>(entity)...);
		}

		template<typename... Cts>
		[[nodiscard]] decltype(auto) GetNonEmpty(Entity entity) const
			requires UniqueTypes<TypesList>
		{
			return std::tuple_cat(ForwardNonEmpty<Cts>(entity)...);
		}

		template<typename C>
		[[nodiscard]] decltype(auto) TryGet(Entity entity)
			requires UniqueTypes<TypesList>
		{
			return GetStorage<C>().TryGet(EntityToIndex(entity));
		}

		template<typename C>
		[[nodiscard]] decltype(auto) TryGet(Entity entity) const
			requires UniqueTypes<TypesList>
		{
			return GetStorage<C>().TryGet(EntityToIndex(entity));
		}

		template<typename... Cts>
		[[nodiscard]] decltype(auto) TryGet(Entity entity) requires MoreThanOneType<Cts...> && UniqueTypes<TypesList> {
			const auto idx = EntityToIndex(entity);
			return std::forward_as_tuple(GetStorage<Cts>().TryGet(idx)...);
		}

		template<typename... Cts>
		[[nodiscard]] decltype(auto) TryGet(Entity entity) const requires MoreThanOneType<Cts...> && UniqueTypes<TypesList> {
			const auto idx = EntityToIndex(entity);
			return std::forward_as_tuple(GetStorage<Cts>().TryGet(idx)...);
		}

		// -----------------------------------------------------------------
		// Component access - index-based API (always available)
		// Use when component types may not be unique
		// -----------------------------------------------------------------
		template<size_t I, typename... Args>
		void SetByIndex(Entity entity, Args&&... args) {
			std::get<I>(storages).Set(EntityToIndex(entity), std::forward<Args>(args)...);
		}

		template<size_t I, typename... Args>
		void SetSafeByIndex(Entity entity, Args&&... args)
		{
			CheckEntity(entity);
			std::get<I>(storages).Set(EntityToIndex(entity), std::forward<Args>(args)...);
		}

		template<size_t I>
		[[nodiscard]] decltype(auto) GetByIndex(Entity entity) {
			return std::get<I>(storages).Get(EntityToIndex(entity));
		}

		template<size_t I>
		[[nodiscard]] decltype(auto) GetByIndex(Entity entity) const {
			return std::get<I>(storages).Get(EntityToIndex(entity));
		}

		template<size_t I>
		[[nodiscard]] decltype(auto) TryGetByIndex(Entity entity) {
			return std::get<I>(storages).TryGet(EntityToIndex(entity));
		}

		template<size_t I>
		[[nodiscard]] decltype(auto) TryGetByIndex(Entity entity) const {
			return std::get<I>(storages).TryGet(EntityToIndex(entity));
		}

		template<size_t... Is>
		[[nodiscard]] decltype(auto) GetByIndices(Entity entity) {
			const auto idx = EntityToIndex(entity);
			return std::forward_as_tuple(std::get<Is>(storages).Get(idx)...);
		}

		template<size_t... Is>
		[[nodiscard]] decltype(auto) GetByIndices(Entity entity) const {
			const auto idx = EntityToIndex(entity);
			return std::forward_as_tuple(std::get<Is>(storages).Get(idx)...);
		}

		// -----------------------------------------------------------------
		// Component access - checked (safe path)
		// -----------------------------------------------------------------
		// Iteration
		// -----------------------------------------------------------------
		template<typename... Cts, typename Fn>
		void Each(Fn&& fn)
			requires UniqueTypes<TypesList>
		{
			static_assert(sizeof...(Cts) > 0, "Each<> requires at least one component type");
			const size_t count = GetStorage<FirstComponent<Cts...>>().DenseSize();
			for (size_t i = 0; i < count; ++i) {
				std::invoke(fn, GetStorage<Cts>().GetByDenseSlotUnchecked(i)...);
			}
		}

		template<typename... Cts, typename Fn>
		void Each(Fn&& fn) const
			requires UniqueTypes<TypesList>
		{
			static_assert(sizeof...(Cts) > 0, "Each<> requires at least one component type");
			const size_t count = GetStorage<FirstComponent<Cts...>>().DenseSize();
			for (size_t i = 0; i < count; ++i) {
				std::invoke(fn, GetStorage<Cts>().GetByDenseSlotUnchecked(i)...);
			}
		}

		// -----------------------------------------------------------------
		// Iteration - index-based API (always available)
		// -----------------------------------------------------------------
		template<size_t... Is, typename Fn>
		void EachByIndex(Fn&& fn) {
			static_assert(sizeof...(Is) > 0, "EachByIndex<> requires at least one index");
			// Use first storage's size (dense) instead of entities size (now sparse)
			const size_t count = std::get<FirstIndex<Is...>()>(storages).DenseSize();
			for (size_t i = 0; i < count; ++i) {
				fn(std::get<Is>(storages).GetByDenseSlotUnchecked(i)...);
			}
		}

		template<size_t... Is, typename Fn>
		void EachByIndex(Fn&& fn) const {
			static_assert(sizeof...(Is) > 0, "EachByIndex<> requires at least one index");
			// Use first storage's size (dense) instead of entities size (now sparse)
			const size_t count = std::get<FirstIndex<Is...>()>(storages).DenseSize();
			for (size_t i = 0; i < count; ++i) {
				fn(std::get<Is>(storages).GetByDenseSlotUnchecked(i)...);
			}
		}

		// -----------------------------------------------------------------
		// Component span access - type-based (requires unique types)
		// Returns vector of spans, one per chunk
		// -----------------------------------------------------------------
		template<typename C>
		[[nodiscard]] auto Components() noexcept
			requires UniqueTypes<TypesList>
		{
			return GetStorage<C>().GetDataSpan();
		}

		template<typename C>
		[[nodiscard]] auto Components() const noexcept
			requires UniqueTypes<TypesList>
		{
			return GetStorage<C>().GetDataSpan();
		}

		// -----------------------------------------------------------------
		// Component span access - index-based (always available)
		// Returns vector of spans, one per chunk
		// -----------------------------------------------------------------
		template<size_t I>
		[[nodiscard]] auto ComponentsByIndex() noexcept {
			return std::get<I>(storages).GetDataSpan();
		}

		template<size_t I>
		[[nodiscard]] auto ComponentsByIndex() const noexcept {
			return std::get<I>(storages).GetDataSpan();
		}

		// -----------------------------------------------------------------
		// Housekeeping
		// -----------------------------------------------------------------
		void Clear() noexcept {
			for_each_tuple([](auto& s) { s.Clear(); }, storages);
			entities.clear();
			fNext = EntityTraits::INVALID_INDEX;
			fSize = 0;
		}

		// Returns the number of live entities (total slots minus free slots)
		auto RawSize() const noexcept { return entities.size() - fSize; }
		auto Size()    const noexcept { return entities.size() - fSize; }

		// Note: Iterator access to entities array is now sparse (contains free slots)
		// Use Each() methods for iterating over live entities via component storage
		auto begin()        noexcept { return entities.begin(); }
		auto end()          noexcept { return entities.end(); }
		auto begin()  const noexcept { return entities.begin(); }
		auto end()    const noexcept { return entities.end(); }
		auto cbegin() const noexcept { return entities.cbegin(); }
		auto cend()   const noexcept { return entities.cend(); }

	private:
		template<typename T>
		inline decltype(auto) ForwardNonEmpty(Entity e) {
			if constexpr (std::is_empty_v<T>) return std::tuple<>{};
			else return std::forward_as_tuple(GetStorage<T>().Get(EntityToIndex(e)));
		}

		template<typename T>
		inline decltype(auto) ForwardNonEmpty(Entity e) const {
			if constexpr (std::is_empty_v<T>) return std::tuple<>{};
			else return std::forward_as_tuple(GetStorage<T>().Get(EntityToIndex(e)));
		}

		void CheckEntity(Entity entity) const {
			if (IsNullEntity(entity))
				throw std::runtime_error("Invalid Entity (Null Entity)");
			const auto i = EntityToIndex(entity);
			if (i >= entities.size())
				throw std::runtime_error("Invalid Entity (out of bounds)");
			if (entities[i] != entity)
				throw std::runtime_error("Invalid Entity (not active or stale version)");
		}

		template <typename T>
		static void ValidateChunk() {
			static_assert(
				IsContiguous || std::has_single_bit(CHUNK_SIZE),
				"CHUNK_SIZE must be a power of two (or 0 for contiguous storage)");
		}

		template <typename T>
		static void ValidateDefaultInitializable() {
			if constexpr (!std::is_empty_v<T>) {
				static_assert(std::default_initializable<T>, "Component must be default initializable");
			}
		}

		template <typename T>
		static constexpr size_t GetStorageIdx() noexcept {
			using Td = std::decay_t<T>;
			using StorageT = ComponentStorage<Self, Td>;
			static_assert(tuple_contains_type_v<StoragesTuple, StorageT>, "Component storage not found");
			return tuple_type_index_v<StorageT, StoragesTuple>;
		}

		template <typename T>
		inline decltype(auto) GetStorage() noexcept {
			return std::get<GetStorageIdx<T>()>(storages);
		}

		template <typename T>
		inline decltype(auto) GetStorage() const noexcept {
			return std::get<GetStorageIdx<T>()>(storages);
		}

	private:
		static constexpr size_t NUM_COMPONENTS = sizeof...(Cs);

		// Entities storage type - vector if contiguous, ChunkedArray otherwise
		using EntitiesStorage = std::conditional_t<IsContiguous, std::vector<Entity>, ChunkedArray<Entity, CHUNK_SIZE>>;

	public:
		EntitiesStorage entities;
		uint32_t        fNext = EntityTraits::INVALID_INDEX;
		uint32_t        fSize = 0;
		StoragesTuple   storages;
	};

	template<typename... Cs>
	using RegistryFromTypeList = Registry<type_list_t<Cs...>>;
}
