#include <lunar/core/component_storage.hpp>
#include <gtest/gtest.h>
#include <string>

namespace
{
	struct Position
	{
		float x = 0.f;
		float y = 0.f;
	};

	struct Label
	{
		std::string text = {};
	};
}

TEST(ComponentTypeId, IsStablePerType)
{
	EXPECT_EQ(lunar::GetComponentTypeId<Position>(), lunar::GetComponentTypeId<Position>());
}

TEST(ComponentTypeId, IsDistinctBetweenTypes)
{
	EXPECT_NE(lunar::GetComponentTypeId<Position>(), lunar::GetComponentTypeId<Label>());
}

TEST(ComponentStorage, AddThenGetReturnsComponent)
{
	lunar::Pool<lunar::EntityRecord>  entities;
	lunar::ComponentStorage<Position> storage;

	const lunar::Entity entity = entities.create();
	storage.add(entity, 1.f, 2.f);

	ASSERT_NE(storage.get(entity), nullptr);
	EXPECT_FLOAT_EQ(storage.get(entity)->x, 1.f);
	EXPECT_FLOAT_EQ(storage.get(entity)->y, 2.f);
	EXPECT_TRUE(storage.has(entity));
	EXPECT_EQ(storage.size(), 1u);
}

TEST(ComponentStorage, MissingComponentIsNotFound)
{
	lunar::Pool<lunar::EntityRecord>  entities;
	lunar::ComponentStorage<Position> storage;

	const lunar::Entity with_component    = entities.create();
	const lunar::Entity without_component = entities.create();
	storage.add(with_component);

	EXPECT_FALSE(storage.has(without_component));
	EXPECT_EQ(storage.get(without_component), nullptr);
	EXPECT_FALSE(storage.has(lunar::Entity()));
}

TEST(ComponentStorage, RemovingFromMiddleKeepsOtherComponents)
{
	lunar::Pool<lunar::EntityRecord> entities;
	lunar::ComponentStorage<Label>   storage;

	const lunar::Entity first  = entities.create();
	const lunar::Entity middle = entities.create();
	const lunar::Entity last   = entities.create();
	storage.add(first,  "first");
	storage.add(middle, "middle");
	storage.add(last,   "last");

	storage.remove(middle);

	EXPECT_FALSE(storage.has(middle));
	ASSERT_NE(storage.get(first), nullptr);
	ASSERT_NE(storage.get(last), nullptr);
	EXPECT_EQ(storage.get(first)->text, "first");
	EXPECT_EQ(storage.get(last)->text, "last");
	EXPECT_EQ(storage.size(), 2u);
}

TEST(ComponentStorage, RemovingLastComponentWorks)
{
	lunar::Pool<lunar::EntityRecord>  entities;
	lunar::ComponentStorage<Position> storage;

	const lunar::Entity first = entities.create();
	const lunar::Entity last  = entities.create();
	storage.add(first, 1.f, 1.f);
	storage.add(last,  2.f, 2.f);

	storage.remove(last);

	EXPECT_FALSE(storage.has(last));
	ASSERT_NE(storage.get(first), nullptr);
	EXPECT_FLOAT_EQ(storage.get(first)->x, 1.f);
}

TEST(ComponentStorage, RemovingMissingComponentIsHarmless)
{
	lunar::Pool<lunar::EntityRecord>  entities;
	lunar::ComponentStorage<Position> storage;

	const lunar::Entity with_component    = entities.create();
	const lunar::Entity without_component = entities.create();
	storage.add(with_component);

	storage.remove(without_component);
	storage.remove(without_component);

	EXPECT_EQ(storage.size(), 1u);
	EXPECT_TRUE(storage.has(with_component));
}

TEST(ComponentStorage, ComponentCanBeAddedAgainAfterRemoval)
{
	lunar::Pool<lunar::EntityRecord>  entities;
	lunar::ComponentStorage<Position> storage;

	const lunar::Entity entity = entities.create();
	storage.add(entity, 1.f, 1.f);
	storage.remove(entity);
	storage.add(entity, 5.f, 5.f);

	ASSERT_NE(storage.get(entity), nullptr);
	EXPECT_FLOAT_EQ(storage.get(entity)->x, 5.f);
}

TEST(ComponentStorage, EntityReusingSlotDoesNotSeeOldComponent)
{
	lunar::Pool<lunar::EntityRecord>  entities;
	lunar::ComponentStorage<Position> storage;

	const lunar::Entity old_entity = entities.create();
	storage.add(old_entity, 1.f, 1.f);
	storage.remove(old_entity);
	entities.destroy(old_entity);

	const lunar::Entity new_entity = entities.create();

	EXPECT_EQ(new_entity.getIndex(), old_entity.getIndex());
	EXPECT_FALSE(storage.has(new_entity));
	EXPECT_EQ(storage.get(new_entity), nullptr);
}

TEST(ComponentStorage, GetEntitiesListsOwners)
{
	lunar::Pool<lunar::EntityRecord>  entities;
	lunar::ComponentStorage<Position> storage;

	const lunar::Entity first  = entities.create();
	const lunar::Entity second = entities.create();
	storage.add(first);
	storage.add(second);

	const auto& owners = storage.getEntities();

	ASSERT_EQ(owners.size(), 2u);
	EXPECT_EQ(owners[0], first);
	EXPECT_EQ(owners[1], second);
}
