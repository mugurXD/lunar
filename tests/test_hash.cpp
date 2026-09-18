#include <lunar/api.hpp>
#include <gtest/gtest.h>

namespace
{
	constexpr uint64_t EMPTY_HASH     = 0xCBF29CE484222325ull;
	constexpr uint64_t SINGLE_HASH    = 0xAF63DC4C8601EC8Cull;
	constexpr uint64_t WORD_HASH      = 0x85944171F73967E8ull;
	constexpr uint64_t NON_ASCII_HASH = 0xA7A4348BB0C41E97ull;
}

TEST(Hash, Fnv1aMatchesTheReferenceValues)
{
	EXPECT_EQ(lunar::imp::fnv1a_hash(""),       EMPTY_HASH);
	EXPECT_EQ(lunar::imp::fnv1a_hash("a"),      SINGLE_HASH);
	EXPECT_EQ(lunar::imp::fnv1a_hash("foobar"), WORD_HASH);
}

TEST(Hash, Fnv1aHashesNonAsciiNamesAsUnsignedBytes)
{
	EXPECT_EQ(lunar::imp::fnv1a_hash("for\xC3\xAAt"), NON_ASCII_HASH);
}
