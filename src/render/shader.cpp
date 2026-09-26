#include <lunar/render/shader.hpp>
#include <lunar/file/binary_file.hpp>
#include <lunar/debug.hpp>

#include <format>

namespace lunar::Render
{
	namespace
	{
		constexpr std::string_view SHADER_BINARY_PATH = "shader-bin/{}.spv";
	}

	std::vector<char> LoadShader(std::string_view name)
	{
		const Fs::Path path = Fs::fromData(std::format(SHADER_BINARY_PATH, name));

		Fs::BinaryFile file(path);
		if (file.content.empty())
			DEBUG_ERROR("Failed to load shader '{}'", path.string());

		return std::move(file.content);
	}
}
