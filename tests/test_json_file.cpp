#include <lunar/file/binary_file.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/file/text_file.hpp>
#include <gtest/gtest.h>

#include "temporary_directory.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace
{
	constexpr int         STORED_NUMBER     = 42;
	constexpr int         READABLE_INDENT   = 4;
	constexpr const char* INVALID_JSON      = "{ \"unterminated\": ";
	constexpr const char* TEXT_CONTENT      = "first line\nsecond line\n";
	constexpr const char* TEMPORARY_SUFFIX  = ".tmp";

	struct Sample
	{
		int number = 0;

		static nlohmann::json Serialize(const Sample& sample)
		{
			return { { "number", sample.number } };
		}

		static std::optional<Sample> Deserialize(const nlohmann::json& json)
		{
			const int number = json.at("number").get<int>();
			if (number < 0)
				return std::nullopt;

			return Sample { number };
		}
	};

	void WriteText(const Fs::Path& path, const std::string& text)
	{
		Fs::TextFile file;
		file.content = text;
		ASSERT_TRUE(file.toFile(path));
	}
}

TEST(JsonFile, MissingFileIsReportedAsFailure)
{
	const TemporaryDirectory directory;
	Fs::JsonFile             file;

	EXPECT_FALSE(file.fromFile(directory.getPath() / "missing.json"));
	EXPECT_TRUE(file.content.is_null());
}

TEST(JsonFile, InvalidJsonIsReportedAsFailureInsteadOfThrowing)
{
	const TemporaryDirectory directory;
	const Fs::Path           path = directory.getPath() / "invalid.json";
	WriteText(path, INVALID_JSON);

	Fs::JsonFile file;
	file.content = { { "stale", true } };

	EXPECT_FALSE(file.fromFile(path));
	EXPECT_TRUE(file.content.is_null());
	EXPECT_TRUE(Fs::JsonFile(path).content.is_null());
}

TEST(JsonFile, ContentRoundTripsThroughAtomicWrites)
{
	const TemporaryDirectory directory;
	const Fs::Path           path = directory.getPath() / "stored.json";

	Fs::JsonFile written;
	written.content     = { { "number", STORED_NUMBER }, { "list", { 1, 2, 3 } } };
	written.indentation = READABLE_INDENT;

	ASSERT_TRUE(written.toFile(path));
	EXPECT_FALSE(std::filesystem::exists(Fs::Path(path) += TEMPORARY_SUFFIX));
	EXPECT_EQ(Fs::JsonFile(path).content, written.content);

	written.content["number"] = STORED_NUMBER + 1;
	ASSERT_TRUE(written.toFile(path));
	EXPECT_EQ(Fs::JsonFile(path).content["number"], STORED_NUMBER + 1);
}

TEST(JsonFile, WritingIntoAMissingDirectoryFails)
{
	const TemporaryDirectory directory;
	Fs::JsonFile             file;
	file.content = { { "number", STORED_NUMBER } };

	EXPECT_FALSE(file.toFile(directory.getPath() / "missing" / "file.json"));
}

TEST(JsonDeserialization, MalformedDataBecomesEmptyInsteadOfThrowing)
{
	EXPECT_EQ(Fs::DeserializeJson<Sample>({ { "number", STORED_NUMBER } })->number, STORED_NUMBER);
	EXPECT_FALSE(Fs::DeserializeJson<Sample>({ { "number", "not a number" } }).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<Sample>({ { "other", STORED_NUMBER } }).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<Sample>({ { "number", -1 } }).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<Sample>(nlohmann::json::array()).has_value());
}

TEST(TextFile, ContentRoundTrips)
{
	const TemporaryDirectory directory;
	const Fs::Path           path = directory.getPath() / "text.txt";
	WriteText(path, TEXT_CONTENT);

	EXPECT_EQ(Fs::TextFile(path).content, TEXT_CONTENT);
}

TEST(BinaryFile, ContentRoundTrips)
{
	const TemporaryDirectory directory;
	const Fs::Path           path = directory.getPath() / "data.bin";

	Fs::BinaryFile written;
	written.content = { '\0', '\x7f', '\xff', 'a' };
	ASSERT_TRUE(written.toFile(path));

	EXPECT_EQ(Fs::BinaryFile(path).content, written.content);
}
