#pragma once
#include <lunar/file/filesystem.hpp>
#include <lunar/api.hpp>
#include <string>

namespace Fs
{
	class LUNAR_API TextFile : public Resource
	{
	public:
		TextFile(const Path& path) { fromFile(path); }
		TextFile() = default;

		bool toFile(const Path& path) override;
		bool fromFile(const Path& path) override;

		std::string content;
	};
}
