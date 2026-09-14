#pragma once
#include <lunar/api.hpp>
#include <lunar/render/common.hpp>
#include <memory>
#include <string_view>

namespace lunar::Render
{
	struct LUNAR_API Window_T;

	struct LUNAR_API RenderDeviceSettings
	{
		std::string_view appName = {};
		Window_T*        pWindow = nullptr;
		Backend          backend = Backend::eDefault;
	};

	struct LUNAR_API RenderDeviceCapabilities
	{
		bool rayQuery           = false;
		bool rayTracingPipeline = false;
		bool memoryBudget       = false;
	};

	class LUNAR_API Swapchain
	{
	public:
		Swapchain()          noexcept = default;
		virtual ~Swapchain() noexcept = default;

		Swapchain(const Swapchain&)            = delete;
		Swapchain& operator=(const Swapchain&) = delete;
	};

	class LUNAR_API RenderDevice
	{
	public:
		RenderDevice()          noexcept = default;
		virtual ~RenderDevice() noexcept = default;

		RenderDevice(const RenderDevice&)            = delete;
		RenderDevice& operator=(const RenderDevice&) = delete;

		virtual std::unique_ptr<Swapchain> createSwapchain(Window_T& window) = 0;

		const RenderDeviceCapabilities& getCapabilities() const { return capabilities; }

	protected:
		RenderDeviceCapabilities capabilities = {};
	};

	LUNAR_API std::unique_ptr<RenderDevice> CreateRenderDevice(const RenderDeviceSettings& settings);
}
