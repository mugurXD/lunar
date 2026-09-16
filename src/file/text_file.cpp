#include <lunar/file/text_file.hpp>
#include <fstream>
#include <sstream>

namespace Fs
{
	bool TextFile::fromFile(const Path& path)
	{
		if (!fileExists(path))
			return false;

		auto res_file = std::ifstream(path);
		auto res_buf = std::stringstream();
		res_buf << res_file.rdbuf();
		content = res_buf.str();
		res_file.close();
		return true;
	}

	bool TextFile::toFile(const Path& path)
	{
		return WriteFileAtomically(path, content);
	}
}
