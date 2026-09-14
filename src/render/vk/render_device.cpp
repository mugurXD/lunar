#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

#include <lunar/render/context.hpp>
#include <lunar/render/imp/vk/render_device.hpp>
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
