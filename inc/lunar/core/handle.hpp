#pragma once
#include <lunar/api.hpp>
#include <lunar/utils/collections.hpp>
#include <lunar/utils/identifiable.hpp>
#include <lunar/debug/assert.hpp>
#include <concepts>
#include <cstdint>
#include <utility>

namespace lunar
{
	template<typename T>
	concept HasValidCheck = requires(T t) {
		{ t.valid() } -> std::same_as<bool>;
		{ t.setValid( bool() ) };
	};

	//template<typename T>
	//concept RefCounted = requires(T t) { 
	//	{ t.refCount } -> std::same_as<size_t>; 
	//};

	/*
		A Handle<T> object acts like a normal raw pointer, except it guarantees that it 
		will not be invalidated in the future due to something out of the API caller's 
		control (e.g.: vector resizing).
	*/
	template<typename T>
	class LUNAR_API Handle
	{
	public:
		Handle(std::nullptr_t)                      noexcept : ref(nullptr), idx(0) {}
		Handle(vector<T>& collection, size_t index) noexcept : ref(&collection), idx(index) {}
		Handle(vector<T>& collection, T* object)    noexcept : ref(&collection), idx((size_t)(object - collection.data())) {}
		Handle()                                    noexcept = default;
		~Handle()                                   noexcept = default;

		T* operator->()                           { return &(ref->operator[](idx)); }
		const T* operator->()               const { return &(ref->operator[](idx)); }
		T& get()                                  { return ref->operator[](idx); }
		const T& get()                      const { return ref->operator[](idx); }
		T*       pointer()                        { if (ref == nullptr) return nullptr; return &(get()); }
		bool     operator==(T* pointer)     const { return pointer == (ref->data() + idx); }
		bool     operator==(std::nullptr_t) const { return ref == nullptr; }

		template<typename = typename std::enable_if<HasValidCheck<T>>::type>
		bool     valid()     const { return get().valid(); }

	protected:
		vector<T>* ref = nullptr;
		size_t     idx = 0;
	};

	template<typename T>
	class Pool;

	template<typename T>
	class LUNAR_API PoolHandle
	{
	public:
		PoolHandle()               noexcept = default;
		PoolHandle(std::nullptr_t) noexcept {}
		~PoolHandle()              noexcept = default;

		T*       operator->()                           { return &get(); }
		const T* operator->()                     const { return &get(); }
		T*       pointer()                              { return pool == nullptr ? nullptr : pool->get(*this); }
		bool     valid()                          const { return pool != nullptr && pool->contains(*this); }
		bool     operator==(const PoolHandle&)    const = default;
		bool     operator==(std::nullptr_t)       const { return !valid(); }

		T& get()
		{
			DEBUG_ASSERT(valid(), "Accessed an object through a null or stale handle");
			return *pool->get(*this);
		}

		const T& get() const
		{
			DEBUG_ASSERT(valid(), "Accessed an object through a null or stale handle");
			return *pool->get(*this);
		}

	private:
		static constexpr uint32_t NULL_GENERATION = 0;

		PoolHandle(Pool<T>* pool, uint32_t index, uint32_t generation) noexcept
			: pool(pool),
			index(index),
			generation(generation)
		{
		}

		Pool<T>* pool       = nullptr;
		uint32_t index      = 0;
		uint32_t generation = NULL_GENERATION;

		friend class Pool<T>;
	};

	template<typename T>
	class LUNAR_API Pool
	{
	public:
		Pool()  noexcept = default;
		~Pool() noexcept = default;

		Pool(const Pool&)            = delete;
		Pool(Pool&&)                 = delete;
		Pool& operator=(const Pool&) = delete;
		Pool& operator=(Pool&&)      = delete;

		template<typename... Args>
		PoolHandle<T> create(Args&&... args)
		{
			const uint32_t index = acquireSlot();
			Slot&          slot  = slots[index];

			slot.value.emplace(std::forward<Args>(args)...);
			liveCount++;

			return PoolHandle<T>(this, index, slot.generation);
		}

		void destroy(const PoolHandle<T>& handle)
		{
			if (!contains(handle))
				return;

			Slot& slot = slots[handle.index];
			slot.value.reset();
			slot.generation++;
			liveCount--;

			if (slot.generation != PoolHandle<T>::NULL_GENERATION)
				freeSlots.push_back(handle.index);
		}

		bool contains(const PoolHandle<T>& handle) const
		{
			return handle.pool == this
				&& handle.index < slots.size()
				&& slots[handle.index].generation == handle.generation;
		}

		T* get(const PoolHandle<T>& handle)
		{
			return contains(handle) ? &*slots[handle.index].value : nullptr;
		}

		const T* get(const PoolHandle<T>& handle) const
		{
			return contains(handle) ? &*slots[handle.index].value : nullptr;
		}

		size_t size() const
		{
			return liveCount;
		}

		template<typename Function>
		void forEach(Function&& function)
		{
			for (uint32_t index = 0; index < slots.size(); index++)
				if (slots[index].value.has_value())
					function(PoolHandle<T>(this, index, slots[index].generation), *slots[index].value);
		}

		template<typename Predicate>
		PoolHandle<T> find(Predicate&& predicate)
		{
			for (uint32_t index = 0; index < slots.size(); index++)
				if (slots[index].value.has_value() && predicate(std::as_const(*slots[index].value)))
					return PoolHandle<T>(this, index, slots[index].generation);

			return nullptr;
		}

	private:
		static constexpr uint32_t FIRST_GENERATION = PoolHandle<T>::NULL_GENERATION + 1;

		struct Slot
		{
			std::optional<T> value      = std::nullopt;
			uint32_t         generation = FIRST_GENERATION;
		};

		uint32_t acquireSlot()
		{
			if (freeSlots.empty())
			{
				slots.emplace_back();
				return static_cast<uint32_t>(slots.size() - 1);
			}

			const uint32_t index = freeSlots.back();
			freeSlots.pop_back();
			return index;
		}

		vector<Slot>     slots     = {};
		vector<uint32_t> freeSlots = {};
		size_t           liveCount = 0;
	};

	/*
		A RefHandle<T> acts pretty much like a smart pointer for an object created by the engine.
	*/
	template<typename T>
	class LUNAR_API RefHandle
	{
	public:
		using TyPtr = T*;
		using TyRef = T&;

		RefHandle(std::nullptr_t)                          noexcept : ref(nullptr), idx(0) {}
		RefHandle(vector<TyPtr>& collection, size_t index) noexcept : ref(&collection), idx(index)
		{
			T* element = (*ref)[idx];
			element->refCount++;
		}
		RefHandle(vector<TyPtr>& collection, TyPtr object) noexcept : ref(&collection), idx(0)
		{
			for (size_t i = 0; i < collection.size(); i++)
				if (collection[i] == object)
					idx = i;

			T* element = (*ref)[idx];
			element->refCount++;
		}
		RefHandle()                                    noexcept = default;
		~RefHandle()                                   noexcept
		{
			if (ref != nullptr)
			{
				T* element = (*ref)[idx];
				element->refCount--;

				if (element->refCount <= 0)
					delete element;
			}
		}

		T*       operator->()           { return ref->operator[](idx); }
		const T* operator->() const     { return ref->operator[](idx); }
		T&       get()                  { return *(ref->operator[](idx)); }
		const T& get()       const      { return *(ref->operator[](idx)); }
		bool     operator==(T* pointer) { return pointer == (*ref)[idx]; }
		
		bool     exists()     const     { return ref !=  nullptr;}

		/* Copy & move operators */

		RefHandle(const RefHandle& other) noexcept
			: ref(other.ref),
			idx(other.idx)
		{
			if (this->ref != nullptr)
			{
				T* element = (*ref)[idx];
				element->refCount++;
			}
		}

		RefHandle& operator=(const RefHandle& other) noexcept
		{
			ref = other.ref;
			idx = other.idx;
			
			if (this->ref != nullptr)
			{
				T* element = (*ref)[idx];
				element->refCount++;
			}

			return *this;
		}

		RefHandle(RefHandle&& other) noexcept
			: ref(other.ref),
			idx(other.idx)
		{
			other.ref = nullptr;
			other.idx = 0;
		}

		RefHandle& operator=(RefHandle&& other) noexcept
		{
			ref = other.ref;
			idx = other.idx;

			other.ref = nullptr;
			other.idx = 0;
			return *this;
		}

	protected:
		vector<TyPtr>* ref = nullptr;
		size_t         idx = 0;
	};

	template<typename T>
	inline Handle<T> make_handle(vector<T>& collection, size_t index)
	{
		return Handle<T>(collection, index);
	}

	template<typename T>
	inline Handle<T> make_handle(vector<T>& collection)
	{
		return Handle<T>(collection, collection.size() - 1);
	}

	template<typename T>
	inline Handle<T> make_handle(vector<T>& collection, T* object)
	{
		return Handle<T>(collection, object);
	}
}

#define LUNAR_HANDLE(Type)          using Type = lunar::Handle<Type##_T>
#define LUNAR_HANDLE_IMPL(Type)     template class LUNAR_API lunar::Handle<Type##_T>

#define LUNAR_POOL_HANDLE(Type)     using Type = lunar::PoolHandle<Type##_T>

#define LUNAR_REF_HANDLE(Type)      using Type = lunar::RefHandle<Type##_T>
#define LUNAR_REF_HANDLE_IMPL(Type) template class LUNAR_API lunar::RefHandle<Type##_T>

#define LUNAR_SHARED_HANDLE(Type)   using Type = std::shared_ptr<Type##_T>
