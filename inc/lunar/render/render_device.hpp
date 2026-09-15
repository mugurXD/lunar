#pragma once
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

#include <lunar/api.hpp>
#include <lunar/render/common.hpp>
#include <lunar/render/gpu_types.hpp>
#include <lunar/render/buffer_types.hpp>

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

		virtual std::unique_ptr<Swapchain> createSwapchain(Window_T& window)                                                     = 0;
		virtual BufferHandle               createBuffer(const BufferDesc& desc, std::span<const std::byte> initial_data)         = 0;
		virtual void                       destroyBuffer(BufferHandle buffer)                                                    = 0;
		virtual UploadTicket               uploadBuffer(BufferHandle buffer, size_t offset, std::span<const std::byte> data)    = 0;
		virtual UploadTicket               flushUploads()                                                                        = 0;
		virtual bool                       isComplete(UploadTicket ticket)                                                 const = 0;
		virtual uint64_t                   getBufferAddress(BufferHandle buffer)                                                 = 0;

		const RenderDeviceCapabilities& getCapabilities() const { return capabilities; }

	protected:
		RenderDeviceCapabilities capabilities = {};
	};

	LUNAR_API std::unique_ptr<RenderDevice> CreateRenderDevice(const RenderDeviceSettings& settings);
}
