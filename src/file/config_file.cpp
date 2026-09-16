#include <lunar/file/config_file.hpp>
#include <lunar/utils/lexer.hpp>
#include <lunar/debug/log.hpp>
#include <sstream>
#include <fstream>
#include <string>

#include <lunar/exp/utils/scanner.hpp>

namespace Fs
{
	ConfigFile::ConfigFile(const Path& path)
	{
		fromFile(path);
	}

	bool ConfigFile::fromFile(const Path& path)
	{
		if (!fileExists(path))
			return false;

		using namespace Utils::Exp;

		auto        file    = TextFile(path);
		auto        scanner = Scanner(file.content);
		const auto& tokens  = scanner
								.run()
								.getResult();

		auto get_at = [&](size_t idx) -> const Token&
		{
			if (idx >= tokens.size())
				return tokens[tokens.size() - 1];
			
			if (idx < 0)
				return tokens[0];

			return tokens[idx];
		};

		size_t i = 0;
		while (get_at(i).type != TokenType::eEof)
		{
			const auto& key        = get_at(i);
			const auto& equal_sign = get_at(i + 1);
			const auto& value      = get_at(i + 2);

			if (key.type != TokenType::eIdentifier)
				DEBUG_ERROR(
					"At line {}: Invalid token '{}' inside config file '{}'. Expected key name.",
					key.line, key.toStringView(), path.generic_string()
				);

			if (equal_sign.type != TokenType::eEqual)
				DEBUG_ERROR(
					"At line {}: Invalid token '{}' inside config file '{}'. Expected equal sign instead.",
					equal_sign.line, equal_sign.toStringView(), path.generic_string()
				);

			if (equal_sign.value == value.value)
				DEBUG_ERROR(
					"At line {}: Expected value of key '{}' inside config file '{}'.",
					value.line, key.toStringView(), path.generic_string()
				);

			content.insert(std::pair<std::string, std::string>(key.toString(), value.toString()));
			//DEBUG_LOG("Found key-value pair '{}'='{}'", key.toString(), value.toString());

			i += 3;
		}

		return true;
	}

	bool ConfigFile::toFile(const Path& path)
	{
		std::string serialized;
		for (const auto& [key, value] : content)
			serialized += std::format("{} = {}\n", key, value);

		return WriteFileAtomically(path, serialized);
	}
}
