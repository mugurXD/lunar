#pragma once
#include <lunar/file/filesystem.hpp>
#include <lunar/api.hpp>
#include <string>
#include <vector>

namespace Fs
{
	class LUNAR_API BinaryFile : public Resource
	{
	public:
		BinaryFile(const Path& path) { fromFile(path); }
		BinaryFile() = default;

		bool toFile(const Path& path) override;
		bool fromFile(const Path& path) override;

		std::vector<char> content;
	};
}
