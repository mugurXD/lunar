#pragma once
#include <lunar/api.hpp>
#include <lunar/core/handle.hpp>
#include <lunar/debug/assert.hpp>
#include <lunar/utils/collections.hpp>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <limits>
#include <utility>

namespace lunar
{
	constexpr size_t MAX_COMPONENT_TYPES = 64;

	using ComponentMask = std::bitset<MAX_COMPONENT_TYPES>;

	struct LUNAR_API EntityRecord
	{
		ComponentMask components = {};
	};

	using Entity = PoolHandle<EntityRecord>;

	namespace imp
	{
		inline size_t NextComponentTypeId()
		{
			static std::atomic<size_t> next_type_id = 0;

			const size_t type_id = next_type_id++;
			DEBUG_ASSERT(type_id < MAX_COMPONENT_TYPES, "Exceeded MAX_COMPONENT_TYPES");
			return type_id;
		}
	}

	template<typename T>
	size_t GetComponentTypeId()
	{
		static const size_t type_id = imp::NextComponentTypeId();
		return type_id;
	}

	class LUNAR_API ComponentStorageBase
	{
	public:
		ComponentStorageBase()          noexcept = default;
		virtual ~ComponentStorageBase() noexcept = default;

		virtual void remove(const Entity& entity) = 0;

		bool has(const Entity& entity) const
		{
			const uint32_t index = entity.getIndex();
			return index < sparse.size()
				&& sparse[index] != INVALID_POSITION
				&& entities[sparse[index]] == entity;
		}

		size_t size() const
		{
			return entities.size();
		}

		const vector<Entity>& getEntities() const
		{
			return entities;
		}

	protected:
		static constexpr uint32_t INVALID_POSITION = std::numeric_limits<uint32_t>::max();

		vector<uint32_t> sparse   = {};
		vector<Entity>   entities = {};
	};

	template<typename T>
	class LUNAR_API ComponentStorage final : public ComponentStorageBase
	{
	public:
		template<typename... Args>
		T& add(const Entity& entity, Args&&... args)
		{
			DEBUG_ASSERT(!has(entity), "Entity already has a component of this type");

			const uint32_t index = entity.getIndex();
			if (index >= sparse.size())
				sparse.resize(index + 1, INVALID_POSITION);

			T& component = components.emplace_back(std::forward<Args>(args)...);
			entities.push_back(entity);
			sparse[index] = static_cast<uint32_t>(components.size() - 1);

			return component;
		}

		T* get(const Entity& entity)
		{
			return has(entity) ? &components[sparse[entity.getIndex()]] : nullptr;
		}

		void remove(const Entity& entity) override
		{
			if (!has(entity))
				return;

			const uint32_t removed_index    = entity.getIndex();
			const uint32_t removed_position = sparse[removed_index];
			const uint32_t last_position    = static_cast<uint32_t>(components.size() - 1);

			if (removed_position != last_position)
			{
				components[removed_position] = std::move(components[last_position]);
				entities[removed_position]   = entities[last_position];
				sparse[entities[removed_position].getIndex()] = removed_position;
			}

			components.pop_back();
			entities.pop_back();
			sparse[removed_index] = INVALID_POSITION;
		}

	private:
		vector<T> components = {};
	};
}
