#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <concepts>
#include <memory>
#include <list>

#ifndef APP_NAME
#	define APP_NAME "UntitledLunarApp"
#endif

#ifndef APP_VER_MAJOR
#	define APP_VER_MAJOR 0
#endif

#ifndef APP_VER_MINOR
#	define APP_VER_MINOR 0
#endif

#ifndef APP_VER_PATCH
#	define APP_VER_PATCH 1
#endif

#if defined(_WIN32) && defined(LUNAR_DLL) && defined(LUNAR_LIBRARY_EXPORT)
#	define LUNAR_API __declspec(dllexport)
#elif defined(_WIN32) && defined(LUNAR_DLL)
#	define LUNAR_API __declspec(dllimport)
#else
#	define LUNAR_API
#endif

#ifndef NDEBUG
#	define LUNAR_DEBUG_BUILD 1
#else
#	define LUNAR_DEBUG_BUILD 0
#endif

#ifndef _MSC_VER
#   define LUNAR_FN_NAME __PRETTY_FUNCTION__
#else
#   define LUNAR_FN_NAME __FUNCTION__
#endif

namespace lunar
{
	template<typename BitType>
	class LUNAR_API Flags
	{
	public:
		using MaskType = std::underlying_type<BitType>::type;

		constexpr Flags() noexcept : mask(0) {}
		constexpr Flags(BitType bit) noexcept : mask(static_cast<MaskType>(bit)) {}
		constexpr Flags(const Flags<BitType>& other) noexcept = default;
		constexpr explicit Flags(MaskType flags) noexcept : mask(flags) {}

		constexpr bool           operator&(BitType bit) const { return mask & static_cast<MaskType>(bit); }
		constexpr Flags<BitType> operator|(BitType bit) const { return Flags<BitType>(mask | static_cast<MaskType>(bit)); }
		constexpr Flags<BitType> operator|(const Flags<BitType>& other) const { return Flags<BitType>(mask | other.mask); }
		constexpr operator MaskType() const { return mask; }
		
	private:
		MaskType mask;
	};
}
	
#define LUNAR_FLAGS(Name, Underlying) using Name = lunar::Flags<Underlying>

namespace lunar::imp
{
	constexpr LUNAR_API uint64_t fnv1a_hash(const std::string_view& str)
	{
		uint64_t hash = 0xcbf29ce484222325;
		for (char c : str)
		{
			hash ^= static_cast<uint8_t>(c);
			hash *= 0x100000001b3;
		}
		return hash;
	}
}

#ifdef _MSC_VER
#	define LUNAR_DEBUG_DISPLAY(expr) __declspec(property(get = expr))
#else
#	define LUNAR_DEBUG_DISPLAY(expr)
#endif
