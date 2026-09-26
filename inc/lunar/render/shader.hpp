#pragma once
#include <lunar/api.hpp>

#include <string_view>
#include <vector>

namespace lunar::Render
{
	LUNAR_API std::vector<char> LoadShader(std::string_view name);
}
