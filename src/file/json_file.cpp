#include <lunar/file/json_file.hpp>
#include <lunar/debug.hpp>

#include <fstream>

namespace Fs
{
	JsonFile::JsonFile(const Path& path)
	{
		fromFile(path);
	}

	bool JsonFile::fromFile(const Path& path)
	{
		content = nullptr;

		if (!fileExists(path))
			return false;

		std::ifstream  file(path);
		nlohmann::json parsed = nlohmann::json::parse(file, nullptr, false);
		if (parsed.is_discarded())
		{
			DEBUG_ERROR("'{}' does not contain valid JSON", path.string());
			return false;
		}

		content = std::move(parsed);
		return true;
	}

	bool JsonFile::toFile(const Path& path)
	{
		return WriteFileAtomically(path, content.dump(indentation));
	}

	bool JsonObject::fromFile(const Path& path)
	{
		const JsonFile file(path);
		if (file.content.is_null())
			return false;

		fromJson(file.content);
		return true;
	}

	bool JsonObject::toFile(const Path& path)
	{
		JsonFile file;
		file.content = toJson();
		return file.toFile(path);
	}

	void ReportMalformedJson(std::string_view reason)
	{
		DEBUG_ERROR("Malformed JSON data: {}", reason);
	}
}
