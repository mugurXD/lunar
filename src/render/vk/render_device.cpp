#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

#include <lunar/render/context.hpp>
#include <lunar/render/imp/vk/render_device.hpp>
#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	VkRenderDevice::VkRenderDevice(const RenderDeviceSettings& settings) noexcept
	{
		vkb::InstanceBuilder vkb_instance_builder;
		auto instance_res = vkb_instance_builder
			.set_app_name(std::string(settings.appName))
			.set_debug_callback([](
				VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT       messageType,
				const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
				void*) -> VkBool32 
			{
				if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				{
					const char* severity = vkb::to_string_message_severity(messageSeverity);
						const char* type     = vkb::to_string_message_type(messageType);
					DEBUG_ERROR("[{}: {}] {}", severity, type, pCallbackData->pMessage);
				}
				return VK_FALSE;
			})
			.request_validation_layers(true)
			.require_api_version(1, 3)
			.build();

		if (!instance_res)
		{
			DEBUG_ERROR("Failed to create Vulkan instance: {}", instance_res.error().message());
			return;
		}

		vkb::Instance               vkb_instance = instance_res.value();

		VkSurfaceKHR surface     = VK_NULL_HANDLE;
		VkResult     surface_res = {};
		if (settings.pWindow != nullptr)
		{
			surface_res = glfwCreateWindowSurface(vkb_instance, settings.pWindow->glfwGetHandle(), nullptr, &surface);
			if (surface_res != VK_SUCCESS)
			{
				DEBUG_ERROR("Failed to create Vulkan surface: {}", string_VkResult(surface_res));
				return;
			}
		}


		vkb::PhysicalDeviceSelector vkb_selector { vkb_instance };
		auto                        phys_res = vkb_selector
			.set_minimum_version(1, 3)
			.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
			.require_present(settings.pWindow != nullptr)
			.set_surface(surface)
			.select();

		if (!phys_res)
		{
			DEBUG_ERROR("Failed to select Vulkan physical device: {}", phys_res.error().message());
			if (phys_res.error() == vkb::PhysicalDeviceError::no_suitable_device)
			{
				const auto& detailed_reasons = phys_res.detailed_failure_reasons();
				for(const auto& reason : detailed_reasons)
					DEBUG_ERROR("Selection failure reason: {}", reason);
			}
			return;
		}

		DEBUG_LOG("Using device \"{}\" for rendering.", phys_res.value().properties.deviceName);
		vkb::DeviceBuilder vkb_device_builder { phys_res.value() };
		auto               device_res         = vkb_device_builder.build();
		if (!device_res)
		{
			DEBUG_ERROR("Failed to create Vulkan device: {}", device_res.error().message());
			return;
		}

		vkb::Device           vkb_device        = device_res.value();
		
		if (settings.pWindow != nullptr)
		{
			vkb::SwapchainBuilder swapchain_builder { vkb_device };
			auto                  swapchain_res     = swapchain_builder.build();
			if (!swapchain_res)
			{
				DEBUG_ERROR("{}", swapchain_res.error().message());
				return;
			}
			vkb::Swapchain vkb_swapchain = swapchain_res.value();
			this->swapchain = std::make_optional<Swapchain>(surface, vkb_swapchain);
		}

		this->instance      = vkb_instance;
		this->device        = device_res.value();
		this->graphicsQueue = device.get_queue(vkb::QueueType::graphics).value();
		this->presentQueue  = device.get_queue(vkb::QueueType::present).value();


		DEBUG_LOG("Vulkan rendering interface initialized.");
	}

	VkRenderDevice::~VkRenderDevice()
	{
		vkb::destroy_device(device);
		vkb::destroy_instance(instance);
	}
}
