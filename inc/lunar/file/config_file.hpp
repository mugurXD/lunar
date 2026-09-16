#pragma once
#include <lunar/file/filesystem.hpp>
#include <lunar/api.hpp>
#include <concepts>
#include <string>
#include <map>

namespace Fs
{
	class LUNAR_API ConfigFile : public Resource
	{
	public:
		ConfigFile(const Path& path);
		ConfigFile() = default;

		bool fromFile(const Path& path) override;
		bool toFile(const Path& path) override;

		std::string& operator[](const std::string& key)
		{
			return content[key];
		}

		template<typename T>
		inline T get(const std::string& key) const
		{
			if constexpr (std::is_same<T, int>::value)
				return std::stoi(content.at(key));
			else if constexpr (std::is_same<T, float>::value)
				return std::stof(content.at(key));
			else if constexpr (std::is_same<T, double>::value)
				return std::stod(content.at(key));
			else if constexpr (std::is_same<T, std::string>::value)
				return content.at(key);
			else 
				static_assert(true); // not implemented
		}

		template<typename T>
		inline T get_or(const std::string& key, const T& default_value) const
		{
			if (content.contains(key))
				return get<T>(key);
			else
				return default_value;
		}

		std::map<std::string, std::string> content;
	};
}
