#include <lunar/file/filesystem.hpp>
#include <lunar/debug.hpp>

#include <fstream>
#include <system_error>

namespace Fs
{
	namespace
	{
		constexpr std::string_view TEMPORARY_EXTENSION = ".tmp";
	}

	Path fromData(const std::string_view& path)
	{
		return dataDirectory().append(path);
	}

	Path fromBase(const std::string_view& base)
	{
		return baseDirectory().append(base);
	}

	Path baseDirectory()
	{
		return std::filesystem::current_path();
	}

	Path dataDirectory()
	{
		return baseDirectory()
				.append("data");
	}

	bool fileExists(const Path& path)
	{
		return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
	}

	bool WriteFileAtomically(const Path& path, std::string_view contents)
	{
		Path temporary_path = path;
		temporary_path += TEMPORARY_EXTENSION;

		{
			std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
			file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
			if (!file.good())
			{
				DEBUG_ERROR("Failed to write '{}'", temporary_path.string());
				return false;
			}
		}

		std::error_code rename_status;
		std::filesystem::rename(temporary_path, path, rename_status);
		if (rename_status)
		{
			DEBUG_ERROR("Failed to replace '{}': {}", path.string(), rename_status.message());
			return false;
		}

		return true;
	}

	bool Resource::toFile(const Path&)
	{
		return false;
	}

	bool Resource::fromFile(const Path& path)
	{
		return false;
	}
}
