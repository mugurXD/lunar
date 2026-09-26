#include "vk_render_device.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	namespace
	{
		constexpr uint32_t TEXTURE_BINDING   = 0;
		constexpr uint32_t SAMPLER_BINDING   = 1;
		constexpr uint32_t TEXTURE_SET_COUNT = 1;
		constexpr float    MAX_ANISOTROPY    = 16.f;

		struct SamplerSetup
		{
			VkFilter             filter      = VK_FILTER_LINEAR;
			VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			bool                 anisotropic = false;
		};

		constexpr SamplerSetup SAMPLER_SETUPS[] =
		{
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false },
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,        false },
			{ VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false },
			{ VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT,        false },
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,        true  }
		};

		static_assert(std::size(SAMPLER_SETUPS) == SAMPLER_COUNT, "Every SamplerType needs a sampler setup");

		VkSamplerCreateInfo ToSamplerInfo(const SamplerSetup& setup, float max_anisotropy)
		{
			return VkSamplerCreateInfo
			{
				.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter        = setup.filter,
				.minFilter        = setup.filter,
				.mipmapMode       = setup.filter == VK_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU     = setup.addressMode,
				.addressModeV     = setup.addressMode,
				.addressModeW     = setup.addressMode,
				.anisotropyEnable = setup.anisotropic,
				.maxAnisotropy    = setup.anisotropic ? max_anisotropy : 1.f,
				.maxLod           = VK_LOD_CLAMP_NONE
			};
		}
	}

	bool VkRenderDevice::createTextureTable()
	{
		const float max_anisotropy = std::min(device.physical_device.properties.limits.maxSamplerAnisotropy, MAX_ANISOTROPY);

		for (size_t index = 0; index < SAMPLER_COUNT; index++)
		{
			const VkSamplerCreateInfo sampler_info = ToSamplerInfo(SAMPLER_SETUPS[index], max_anisotropy);
			const VkResult            result       = vkCreateSampler(device, &sampler_info, nullptr, &samplers[index]);
			if (result != VK_SUCCESS)
			{
				DEBUG_ERROR("Failed to create sampler {}: {}", index, string_VkResult(result));
				return false;
			}
		}

		const std::array<VkDescriptorSetLayoutBinding, 2> bindings =
		{
			VkDescriptorSetLayoutBinding
			{
				.binding         = TEXTURE_BINDING,
				.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = MAX_TEXTURES,
				.stageFlags      = VK_SHADER_STAGE_ALL
			},
			VkDescriptorSetLayoutBinding
			{
				.binding            = SAMPLER_BINDING,
				.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount    = SAMPLER_COUNT,
				.stageFlags         = VK_SHADER_STAGE_ALL,
				.pImmutableSamplers = samplers.data()
			}
		};

		const std::array<VkDescriptorBindingFlags, 2> binding_flags =
		{
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
			0
		};

		const VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info =
		{
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount  = static_cast<uint32_t>(binding_flags.size()),
			.pBindingFlags = binding_flags.data()
		};

		const VkDescriptorSetLayoutCreateInfo layout_info =
		{
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext        = &binding_flags_info,
			.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings    = bindings.data()
		};

		VkResult result = vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &textureSetLayout);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create texture set layout: {}", string_VkResult(result));
			return false;
		}

		const std::array<VkDescriptorPoolSize, 2> pool_sizes =
		{
			VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURES },
			VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER,       SAMPLER_COUNT }
		};

		const VkDescriptorPoolCreateInfo pool_info =
		{
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
			.maxSets       = TEXTURE_SET_COUNT,
			.poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
			.pPoolSizes    = pool_sizes.data()
		};

		result = vkCreateDescriptorPool(device, &pool_info, nullptr, &texturePool);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create texture descriptor pool: {}", string_VkResult(result));
			return false;
		}

		const VkDescriptorSetAllocateInfo set_info =
		{
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool     = texturePool,
			.descriptorSetCount = TEXTURE_SET_COUNT,
			.pSetLayouts        = &textureSetLayout
		};

		result = vkAllocateDescriptorSets(device, &set_info, &textureSet);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to allocate texture descriptor set: {}", string_VkResult(result));
			return false;
		}

		return true;
	}

	void VkRenderDevice::destroyTextureTable()
	{
		if (texturePool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(device, texturePool, nullptr);

		if (textureSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);

		for (const VkSampler sampler : samplers)
			if (sampler != VK_NULL_HANDLE)
				vkDestroySampler(device, sampler, nullptr);
	}

	uint32_t VkRenderDevice::registerTexture(VkImageView view)
	{
		uint32_t index = nextTextureIndex;
		if (!freeTextureIndices.empty())
		{
			index = freeTextureIndices.back();
			freeTextureIndices.pop_back();
		}
		else if (nextTextureIndex < MAX_TEXTURES)
		{
			nextTextureIndex++;
		}
		else
		{
			DEBUG_ERROR("The texture table is full ({} textures)", MAX_TEXTURES);
			return INVALID_TEXTURE_INDEX;
		}

		const VkDescriptorImageInfo image_info =
		{
			.imageView   = view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		const VkWriteDescriptorSet write =
		{
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = textureSet,
			.dstBinding      = TEXTURE_BINDING,
			.dstArrayElement = index,
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo      = &image_info
		};

		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		return index;
	}

	void VkRenderDevice::releaseTexture(uint32_t index)
	{
		if (index != INVALID_TEXTURE_INDEX)
			freeTextureIndices.push_back(index);
	}

	uint32_t VkRenderDevice::getTextureIndex(ImageHandle image)
	{
		const VkImageRecord* record = resolve(image);
		return record == nullptr ? INVALID_TEXTURE_INDEX : record->textureIndex;
	}

	VkDescriptorSet VkRenderDevice::getTextureSet() const
	{
		return textureSet;
	}
}
