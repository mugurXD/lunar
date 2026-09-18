#pragma once
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

class TemporaryDirectory
{
public:
	TemporaryDirectory()
		: path(std::filesystem::temp_directory_path() / ROOT_NAME / currentTestName())
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
		std::filesystem::create_directories(path, error);
	}

	~TemporaryDirectory()
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}

	TemporaryDirectory(const TemporaryDirectory&)            = delete;
	TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

	const std::filesystem::path& getPath() const
	{
		return path;
	}

private:
	static constexpr std::string_view ROOT_NAME = "lunar_tests";

	static std::string currentTestName()
	{
		const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
		return std::string(info->test_suite_name()) + "." + info->name();
	}

	std::filesystem::path path;
};
