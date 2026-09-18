#pragma once
#include <lunar/file/filesystem.hpp>
#include <lunar/api.hpp>
#include <nlohmann/json.hpp>
#include <concepts>
#include <optional>
#include <string_view>

template<typename T>
concept IsJsonSerializable = requires (const T& memoryForm, const nlohmann::json& serializedForm) {
	{ T::Deserialize(serializedForm) } -> std::same_as<std::optional<T>>;
	{ T::Serialize(memoryForm) }       -> std::same_as<nlohmann::json>;
};

namespace Fs
{
	constexpr int COMPACT_JSON = -1;

	class LUNAR_API JsonFile : public Resource
	{
	public:
		JsonFile(const Path& path);
		JsonFile() = default;

		bool fromFile(const Path& path) override;
		bool toFile(const Path& path) override;

		nlohmann::json content     = nullptr;
		int            indentation = COMPACT_JSON;
	};

	class LUNAR_API JsonObject : public Resource
	{
	public:
		bool fromFile(const Path& path) override;
		bool toFile(const Path& path) override;

		virtual void           fromJson(const nlohmann::json& json) = 0;
		virtual nlohmann::json toJson()                             = 0;
	};

	LUNAR_API void ReportMalformedJson(std::string_view reason);

	template<IsJsonSerializable T>
	std::optional<T> DeserializeJson(const nlohmann::json& json)
	{
		try
		{
			return T::Deserialize(json);
		}
		catch (const nlohmann::json::exception& exception)
		{
			ReportMalformedJson(exception.what());
			return std::nullopt;
		}
	}

	template<IsJsonSerializable T>
	std::optional<T> LoadJson(const Path& path)
	{
		const JsonFile file(path);
		if (file.content.is_null())
			return std::nullopt;

		return DeserializeJson<T>(file.content);
	}
}
