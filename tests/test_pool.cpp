#include <lunar/core/handle.hpp>
#include <gtest/gtest.h>
#include <string>

namespace
{
	struct Node
	{
		Node(std::string name, lunar::PoolHandle<Node> parent) : name(std::move(name)), parent(parent) {}

		std::string             name;
		lunar::PoolHandle<Node> parent;
	};

	constexpr int RELOCATION_OBJECT_COUNT = 1000;
}

TEST(Pool, DefaultHandleIsNull)
{
	const lunar::PoolHandle<Node> handle;

	EXPECT_TRUE(handle == nullptr);
	EXPECT_FALSE(handle.valid());
}

TEST(Pool, CreateReturnsValidHandle)
{
	lunar::Pool<Node> pool;

	auto handle = pool.create("root", nullptr);

	EXPECT_TRUE(handle.valid());
	EXPECT_FALSE(handle == nullptr);
	EXPECT_EQ(handle->name, "root");
	EXPECT_EQ(pool.size(), 1u);
}

TEST(Pool, HandleCanReferenceObjectInSamePool)
{
	lunar::Pool<Node> pool;

	auto root  = pool.create("root", nullptr);
	auto child = pool.create("child", root);

	EXPECT_EQ(child->parent, root);
	EXPECT_EQ(child->parent->name, "root");
}

TEST(Pool, HandlesSurviveStorageRelocation)
{
	lunar::Pool<Node> pool;

	auto root  = pool.create("root", nullptr);
	auto child = pool.create("child", root);
	for (int i = 0; i < RELOCATION_OBJECT_COUNT; i++)
		pool.create("filler", nullptr);

	EXPECT_EQ(root->name, "root");
	EXPECT_EQ(child->parent->name, "root");
}

TEST(Pool, DestroyInvalidatesAllCopiesOfHandle)
{
	lunar::Pool<Node> pool;

	auto       handle = pool.create("object", nullptr);
	const auto copy   = handle;
	pool.destroy(handle);

	EXPECT_FALSE(handle.valid());
	EXPECT_FALSE(copy.valid());
	EXPECT_TRUE(copy == nullptr);
	EXPECT_EQ(handle.pointer(), nullptr);
	EXPECT_EQ(pool.get(handle), nullptr);
	EXPECT_EQ(pool.size(), 0u);
}

TEST(Pool, DestroyingTwiceIsHarmless)
{
	lunar::Pool<Node> pool;

	auto handle = pool.create("object", nullptr);
	pool.create("other", nullptr);
	pool.destroy(handle);
	pool.destroy(handle);

	EXPECT_EQ(pool.size(), 1u);
}

TEST(Pool, ReusedSlotDoesNotReviveOldHandle)
{
	lunar::Pool<Node> pool;

	auto old_handle = pool.create("old", nullptr);
	pool.destroy(old_handle);
	auto new_handle = pool.create("new", nullptr);

	EXPECT_EQ(new_handle.getIndex(), old_handle.getIndex());
	EXPECT_NE(new_handle, old_handle);
	EXPECT_FALSE(old_handle.valid());
	EXPECT_TRUE(new_handle.valid());
	EXPECT_EQ(new_handle->name, "new");
}

TEST(Pool, DestroyedParentComparesEqualToNull)
{
	lunar::Pool<Node> pool;

	auto parent = pool.create("parent", nullptr);
	auto child  = pool.create("child", parent);
	pool.destroy(parent);

	EXPECT_TRUE(child->parent == nullptr);
}

TEST(Pool, HandleFromAnotherPoolIsRejected)
{
	lunar::Pool<Node> pool;
	lunar::Pool<Node> other_pool;

	pool.create("local", nullptr);
	auto foreign = other_pool.create("foreign", nullptr);

	EXPECT_FALSE(pool.contains(foreign));
	EXPECT_EQ(pool.get(foreign), nullptr);
}

TEST(Pool, ForEachVisitsOnlyLiveObjects)
{
	lunar::Pool<Node> pool;

	pool.create("a", nullptr);
	auto destroyed = pool.create("b", nullptr);
	pool.create("c", nullptr);
	pool.destroy(destroyed);

	std::string visited_names;
	pool.forEach([&](lunar::PoolHandle<Node> handle, Node& node) {
		EXPECT_EQ(&handle.get(), &node);
		visited_names += node.name;
	});

	EXPECT_EQ(visited_names, "ac");
}

TEST(Pool, FindReturnsFirstMatch)
{
	lunar::Pool<Node> pool;

	auto first  = pool.create("same", nullptr);
	auto second = pool.create("same", nullptr);

	EXPECT_EQ(pool.find([](const Node& node) { return node.name == "same"; }), first);

	pool.destroy(first);

	EXPECT_EQ(pool.find([](const Node& node) { return node.name == "same"; }), second);
}

TEST(Pool, ClearDestroysEverythingAndInvalidatesHandles)
{
	lunar::Pool<Node> pool;

	auto first  = pool.create("first", nullptr);
	auto second = pool.create("second", nullptr);
	pool.clear();

	EXPECT_EQ(pool.size(), 0u);
	EXPECT_FALSE(first.valid());
	EXPECT_FALSE(second.valid());

	auto recreated = pool.create("recreated", nullptr);

	EXPECT_TRUE(recreated.valid());
	EXPECT_FALSE(first.valid());
	EXPECT_EQ(pool.size(), 1u);
}

TEST(Pool, FindReturnsNullWhenNothingMatches)
{
	lunar::Pool<Node> pool;

	pool.create("present", nullptr);

	EXPECT_TRUE(pool.find([](const Node& node) { return node.name == "missing"; }) == nullptr);
}
