#pragma once
#include <lunar/file/filesystem.hpp>
#include <lunar/api.hpp>
#include <string>

namespace Fs
{
	class LUNAR_API ImageFile : public Resource
	{
	public:
		ImageFile(const Path& path) { fromFile(path); }
		ImageFile() = default;

		bool toFile(const Path& path)   override;
		bool fromFile(const Path& path) override;

		int   width;
		int   height;
		int   channels;
		void* bytes;
	};
}
