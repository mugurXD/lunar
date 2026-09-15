#include "vk_render_device.hpp"
#include "vk_swapchain.hpp"

#include <vulkan/vk_enum_string_helper.h>
#include <GLFW/glfw3.h>

#include <lunar/render/window.hpp>
#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	namespace
	{
		VkPhysicalDeviceFeatures RequiredCoreFeatures()
		{
			VkPhysicalDeviceFeatures features = {};
			features.samplerAnisotropy = VK_TRUE;
			features.multiDrawIndirect = VK_TRUE;
			features.shaderInt64       = VK_TRUE;
			features.fillModeNonSolid  = VK_TRUE;
			features.depthClamp        = VK_TRUE;
			return features;
		}

		VkPhysicalDeviceVulkan11Features RequiredFeatures11()
		{
			return VkPhysicalDeviceVulkan11Features
			{
				.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
				.shaderDrawParameters = VK_TRUE
			};
		}

		VkPhysicalDeviceVulkan12Features RequiredFeatures12()
		{
			return VkPhysicalDeviceVulkan12Features
			{
				.sType                                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
				.drawIndirectCount                             = VK_TRUE,
				.descriptorIndexing                            = VK_TRUE,
				.shaderSampledImageArrayNonUniformIndexing     = VK_TRUE,
				.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE,
				.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
				.descriptorBindingPartiallyBound               = VK_TRUE,
				.descriptorBindingVariableDescriptorCount      = VK_TRUE,
				.runtimeDescriptorArray                        = VK_TRUE,
				.scalarBlockLayout                             = VK_TRUE,
				.timelineSemaphore                             = VK_TRUE,
				.bufferDeviceAddress                           = VK_TRUE
			};
		}

		VkPhysicalDeviceVulkan13Features RequiredFeatures13()
		{
			return VkPhysicalDeviceVulkan13Features
			{
				.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.synchronization2 = VK_TRUE,
				.dynamicRendering = VK_TRUE
			};
		}

		bool EnableAccelerationStructures(vkb::PhysicalDevice& physical_device)
		{
			const auto extensions = std::vector<const char*>
			{
				VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
				VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
			};

			const auto features = VkPhysicalDeviceAccelerationStructureFeaturesKHR
			{
				.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
				.accelerationStructure = VK_TRUE
			};

			return physical_device.enable_extensions_if_present(extensions)
				&& physical_device.enable_extension_features_if_present(features);
		}

		bool EnableRayQuery(vkb::PhysicalDevice& physical_device)
		{
			const auto features = VkPhysicalDeviceRayQueryFeaturesKHR
			{
				.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
				.rayQuery = VK_TRUE
			};

			return physical_device.enable_extension_if_present(VK_KHR_RAY_QUERY_EXTENSION_NAME)
				&& physical_device.enable_extension_features_if_present(features);
		}

		bool EnableRayTracingPipeline(vkb::PhysicalDevice& physical_device)
		{
			const auto features = VkPhysicalDeviceRayTracingPipelineFeaturesKHR
			{
				.sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
				.rayTracingPipeline = VK_TRUE
			};

			return physical_device.enable_extension_if_present(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
				&& physical_device.enable_extension_features_if_present(features);
		}

		RenderDeviceCapabilities EnableOptionalFeatures(vkb::PhysicalDevice& physical_device)
		{
			const bool has_acceleration_structures = EnableAccelerationStructures(physical_device);

			return RenderDeviceCapabilities
			{
				.rayQuery           = has_acceleration_structures && EnableRayQuery(physical_device),
				.rayTracingPipeline = has_acceleration_structures && EnableRayTracingPipeline(physical_device),
				.memoryBudget       = physical_device.enable_extension_if_present(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)
			};
		}
	}

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

		this->instance = instance_res.value();

		if (settings.pWindow != nullptr)
		{
			const VkResult surface_res = glfwCreateWindowSurface(instance, settings.pWindow->glfwGetHandle(), nullptr, &surface);
			if (surface_res != VK_SUCCESS)
			{
				DEBUG_ERROR("Failed to create Vulkan surface: {}", string_VkResult(surface_res));
				return;
			}

			this->presentWindow = settings.pWindow;
		}

		vkb::PhysicalDeviceSelector vkb_selector { instance };
		auto                        phys_res = vkb_selector
			.set_minimum_version(1, 3)
			.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
			.require_present(surface != VK_NULL_HANDLE)
			.set_surface(surface)
			.set_required_features(RequiredCoreFeatures())
			.set_required_features_11(RequiredFeatures11())
			.set_required_features_12(RequiredFeatures12())
			.set_required_features_13(RequiredFeatures13())
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

		vkb::PhysicalDevice vkb_physical_device = phys_res.value();
		this->capabilities = EnableOptionalFeatures(vkb_physical_device);

		DEBUG_LOG("Using device \"{}\" for rendering.", vkb_physical_device.properties.deviceName);
		DEBUG_LOG(
			"Optional features | ray query: {}, ray tracing pipeline: {}, memory budget: {}",
			capabilities.rayQuery,
			capabilities.rayTracingPipeline,
			capabilities.memoryBudget
		);

		vkb::DeviceBuilder vkb_device_builder { vkb_physical_device };
		auto               device_res         = vkb_device_builder.build();
		if (!device_res)
		{
			DEBUG_ERROR("Failed to create Vulkan device: {}", device_res.error().message());
			return;
		}

		this->device = device_res.value();

		const auto graphics_queue_res = device.get_queue(vkb::QueueType::graphics);
		if (!graphics_queue_res)
		{
			DEBUG_ERROR("Failed to get graphics queue: {}", graphics_queue_res.error().message());
			return;
		}

		this->graphicsQueue            = graphics_queue_res.value();
		this->graphicsQueueFamilyIndex = device.get_queue_index(vkb::QueueType::graphics).value();

		if (surface != VK_NULL_HANDLE)
		{
			const auto present_queue_res = device.get_queue(vkb::QueueType::present);
			if (!present_queue_res)
			{
				DEBUG_ERROR("Failed to get present queue: {}", present_queue_res.error().message());
				return;
			}

			this->presentQueue            = present_queue_res.value();
			this->presentQueueFamilyIndex = device.get_queue_index(vkb::QueueType::present).value();
		}

		VkCommandPoolCreateInfo command_pool_info =
		{
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = graphicsQueueFamilyIndex,
		};

		VkResult result;
		result = vkCreateCommandPool(this->device, &command_pool_info, nullptr, &commandPool);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create command pool: {}", string_VkResult(result));
			return;
		}

		VkCommandBufferAllocateInfo command_buffer_info =
		{
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = commandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,	
			.commandBufferCount = 1,
		};
		result = vkAllocateCommandBuffers(this->device, &command_buffer_info, &mainCommandBuffer);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to allocate command buffer: {}", string_VkResult(result));
			return;
		}

		const VkFenceCreateInfo fence_info =
		{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
		};

		result = vkCreateFence(this->device, &fence_info, nullptr, &immediateFence);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create immediate submit fence: {}", string_VkResult(result));
			return;
		}

		const VmaAllocatorCreateFlags allocator_flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
			| (capabilities.memoryBudget ? VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT : 0);

		const VmaAllocatorCreateInfo allocator_info =
		{
			.flags            = allocator_flags,
			.physicalDevice   = device.physical_device,
			.device           = device,
			.instance         = instance,
			.vulkanApiVersion = VK_API_VERSION_1_3
		};

		result = vmaCreateAllocator(&allocator_info, &allocator);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create memory allocator: {}", string_VkResult(result));
			return;
		}

		DEBUG_LOG("Vulkan rendering interface initialized.");
	}

	VkRenderDevice::~VkRenderDevice() noexcept
	{
		if (device.device != VK_NULL_HANDLE)
		{
			vkDeviceWaitIdle(device);

			buffers.forEach([&](PoolHandle<VkBufferRecord>, VkBufferRecord& record) {
				vmaDestroyBuffer(allocator, record.buffer, record.allocation);
			});
			buffers.clear();

			if (allocator != VK_NULL_HANDLE)
				vmaDestroyAllocator(allocator);

			if (immediateFence != VK_NULL_HANDLE)
				vkDestroyFence(device, immediateFence, nullptr);

			if (commandPool != VK_NULL_HANDLE)
				vkDestroyCommandPool(device, commandPool, nullptr);

			vkb::destroy_device(device);
		}

		if (surface != VK_NULL_HANDLE)
			vkb::destroy_surface(instance, surface);

		if (instance.instance != VK_NULL_HANDLE)
			vkb::destroy_instance(instance);
	}

	std::unique_ptr<Swapchain> VkRenderDevice::createSwapchain(Window_T& window)
	{
		DEBUG_ASSERT(&window == presentWindow, "Swapchains can currently only be created for the window the device was created with");
		return std::make_unique<VkSwapchain>(device, window);
	}
}
