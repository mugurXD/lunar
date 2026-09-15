#include "vk_render_device.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	namespace
	{
		constexpr const char* SHADER_ENTRY_POINT = "main";
		constexpr float       DEFAULT_LINE_WIDTH = 1.f;

		constexpr VkColorComponentFlags ALL_COLOR_COMPONENTS = VK_COLOR_COMPONENT_R_BIT
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT;

		constexpr std::array<VkDynamicState, 2> DYNAMIC_STATES = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		constexpr std::pair<PrimitiveTopology, VkPrimitiveTopology> TOPOLOGY_TRANSLATIONS[] =
		{
			{ PrimitiveTopology::eTriangleList,  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST  },
			{ PrimitiveTopology::eTriangleStrip, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP },
			{ PrimitiveTopology::eLineList,      VK_PRIMITIVE_TOPOLOGY_LINE_LIST      },
			{ PrimitiveTopology::ePointList,     VK_PRIMITIVE_TOPOLOGY_POINT_LIST     }
		};

		constexpr std::pair<PolygonMode, VkPolygonMode> POLYGON_MODE_TRANSLATIONS[] =
		{
			{ PolygonMode::eFill, VK_POLYGON_MODE_FILL },
			{ PolygonMode::eLine, VK_POLYGON_MODE_LINE }
		};

		constexpr std::pair<CullMode, VkCullModeFlagBits> CULL_MODE_TRANSLATIONS[] =
		{
			{ CullMode::eNone,  VK_CULL_MODE_NONE      },
			{ CullMode::eFront, VK_CULL_MODE_FRONT_BIT },
			{ CullMode::eBack,  VK_CULL_MODE_BACK_BIT  }
		};

		constexpr std::pair<FrontFace, VkFrontFace> FRONT_FACE_TRANSLATIONS[] =
		{
			{ FrontFace::eCounterClockwise, VK_FRONT_FACE_COUNTER_CLOCKWISE },
			{ FrontFace::eClockwise,        VK_FRONT_FACE_CLOCKWISE         }
		};

		constexpr std::pair<CompareOp, VkCompareOp> COMPARE_OP_TRANSLATIONS[] =
		{
			{ CompareOp::eLess,           VK_COMPARE_OP_LESS             },
			{ CompareOp::eLessOrEqual,    VK_COMPARE_OP_LESS_OR_EQUAL    },
			{ CompareOp::eGreater,        VK_COMPARE_OP_GREATER          },
			{ CompareOp::eGreaterOrEqual, VK_COMPARE_OP_GREATER_OR_EQUAL },
			{ CompareOp::eEqual,          VK_COMPARE_OP_EQUAL            },
			{ CompareOp::eAlways,         VK_COMPARE_OP_ALWAYS           }
		};

		VkPipelineColorBlendAttachmentState ToVkBlendState(BlendMode mode)
		{
			switch (mode)
			{
			case BlendMode::eAlpha:
				return VkPipelineColorBlendAttachmentState
				{
					.blendEnable         = VK_TRUE,
					.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
					.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					.colorBlendOp        = VK_BLEND_OP_ADD,
					.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
					.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					.alphaBlendOp        = VK_BLEND_OP_ADD,
					.colorWriteMask      = ALL_COLOR_COMPONENTS
				};
			case BlendMode::eAdditive:
				return VkPipelineColorBlendAttachmentState
				{
					.blendEnable         = VK_TRUE,
					.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
					.dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
					.colorBlendOp        = VK_BLEND_OP_ADD,
					.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
					.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
					.alphaBlendOp        = VK_BLEND_OP_ADD,
					.colorWriteMask      = ALL_COLOR_COMPONENTS
				};
			default:
				return VkPipelineColorBlendAttachmentState { .colorWriteMask = ALL_COLOR_COMPONENTS };
			}
		}

		VkShaderModule CreateShaderModule(VkDevice device, std::span<const std::byte> bytecode)
		{
			if (bytecode.empty() || bytecode.size() % sizeof(uint32_t) != 0)
			{
				DEBUG_ERROR("Invalid shader bytecode of {} bytes", bytecode.size());
				return VK_NULL_HANDLE;
			}

			const VkShaderModuleCreateInfo module_info =
			{
				.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = bytecode.size(),
				.pCode    = reinterpret_cast<const uint32_t*>(bytecode.data())
			};

			VkShaderModule module = VK_NULL_HANDLE;

			const VkResult result = vkCreateShaderModule(device, &module_info, nullptr, &module);
			if (result != VK_SUCCESS)
				DEBUG_ERROR("Failed to create shader module: {}", string_VkResult(result));

			return module;
		}

		VkPipelineShaderStageCreateInfo ToStageInfo(VkShaderStageFlagBits stage, VkShaderModule module)
		{
			return VkPipelineShaderStageCreateInfo
			{
				.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage  = stage,
				.module = module,
				.pName  = SHADER_ENTRY_POINT
			};
		}
	}

	bool VkRenderDevice::createPipelineLayout()
	{
		const VkPushConstantRange push_constant_range =
		{
			.stageFlags = VK_SHADER_STAGE_ALL,
			.offset     = 0,
			.size       = MAX_PUSH_CONSTANTS_SIZE
		};

		const VkPipelineLayoutCreateInfo layout_info =
		{
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges    = &push_constant_range
		};

		const VkResult result = vkCreatePipelineLayout(device, &layout_info, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create pipeline layout: {}", string_VkResult(result));
			return false;
		}

		return true;
	}

	PipelineHandle VkRenderDevice::createGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		const VkShaderModule vertex_module   = CreateShaderModule(device, desc.vertexShader);
		const VkShaderModule fragment_module = CreateShaderModule(device, desc.fragmentShader);

		const std::array<VkPipelineShaderStageCreateInfo, 2> stages =
		{
			ToStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   vertex_module),
			ToStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_module)
		};

		const VkPipelineVertexInputStateCreateInfo vertex_input =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
		};

		const VkPipelineInputAssemblyStateCreateInfo input_assembly =
		{
			.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = Translate(TOPOLOGY_TRANSLATIONS, desc.topology)
		};

		const VkPipelineViewportStateCreateInfo viewport_state =
		{
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount  = 1
		};

		const VkPipelineRasterizationStateCreateInfo rasterization =
		{
			.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = Translate(POLYGON_MODE_TRANSLATIONS, desc.polygonMode),
			.cullMode    = static_cast<VkCullModeFlags>(Translate(CULL_MODE_TRANSLATIONS, desc.cullMode)),
			.frontFace   = Translate(FRONT_FACE_TRANSLATIONS, desc.frontFace),
			.lineWidth   = DEFAULT_LINE_WIDTH
		};

		const VkPipelineMultisampleStateCreateInfo multisample =
		{
			.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};

		const bool has_depth = desc.depthFormat != Format::eUndefined;

		const VkPipelineDepthStencilStateCreateInfo depth_stencil =
		{
			.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable  = has_depth && desc.depthTest,
			.depthWriteEnable = has_depth && desc.depthWrite,
			.depthCompareOp   = Translate(COMPARE_OP_TRANSLATIONS, desc.depthCompare)
		};

		std::vector<VkFormat> color_formats;
		std::ranges::transform(desc.colorFormats, std::back_inserter(color_formats), ToVkFormat);

		const std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(color_formats.size(), ToVkBlendState(desc.blendMode));

		const VkPipelineColorBlendStateCreateInfo color_blend =
		{
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = static_cast<uint32_t>(blend_attachments.size()),
			.pAttachments    = blend_attachments.data()
		};

		const VkPipelineDynamicStateCreateInfo dynamic_state =
		{
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(DYNAMIC_STATES.size()),
			.pDynamicStates    = DYNAMIC_STATES.data()
		};

		const VkPipelineRenderingCreateInfo rendering_info =
		{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount    = static_cast<uint32_t>(color_formats.size()),
			.pColorAttachmentFormats = color_formats.data(),
			.depthAttachmentFormat   = ToVkFormat(desc.depthFormat)
		};

		const VkGraphicsPipelineCreateInfo pipeline_info =
		{
			.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext               = &rendering_info,
			.stageCount          = static_cast<uint32_t>(stages.size()),
			.pStages             = stages.data(),
			.pVertexInputState   = &vertex_input,
			.pInputAssemblyState = &input_assembly,
			.pViewportState      = &viewport_state,
			.pRasterizationState = &rasterization,
			.pMultisampleState   = &multisample,
			.pDepthStencilState  = &depth_stencil,
			.pColorBlendState    = &color_blend,
			.pDynamicState       = &dynamic_state,
			.layout              = pipelineLayout
		};

		VkPipeline     pipeline = VK_NULL_HANDLE;
		const VkResult result   = vertex_module != VK_NULL_HANDLE && fragment_module != VK_NULL_HANDLE
			? vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline)
			: VK_ERROR_INITIALIZATION_FAILED;

		vkDestroyShaderModule(device, vertex_module, nullptr);
		vkDestroyShaderModule(device, fragment_module, nullptr);

		return registerPipeline(result, pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
	}

	PipelineHandle VkRenderDevice::createComputePipeline(const ComputePipelineDesc& desc)
	{
		const VkShaderModule compute_module = CreateShaderModule(device, desc.computeShader);

		const VkComputePipelineCreateInfo pipeline_info =
		{
			.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage  = ToStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, compute_module),
			.layout = pipelineLayout
		};

		VkPipeline     pipeline = VK_NULL_HANDLE;
		const VkResult result   = compute_module != VK_NULL_HANDLE
			? vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline)
			: VK_ERROR_INITIALIZATION_FAILED;

		vkDestroyShaderModule(device, compute_module, nullptr);

		return registerPipeline(result, pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
	}

	void VkRenderDevice::destroyPipeline(PipelineHandle pipeline)
	{
		const PoolHandle<VkPipelineRecord> stored = FromGpuHandle(pipelines, pipeline);
		const VkPipelineRecord*            record = pipelines.get(stored);
		if (record == nullptr)
			return;

		destroyLater(UploadTicket {}, [this, vk_pipeline = record->pipeline] {
			vkDestroyPipeline(device, vk_pipeline, nullptr);
		});

		pipelines.destroy(stored);
	}

	VkPipelineRecord* VkRenderDevice::resolve(PipelineHandle pipeline)
	{
		return pipelines.get(FromGpuHandle(pipelines, pipeline));
	}

	VkPipelineLayout VkRenderDevice::getPipelineLayout() const
	{
		return pipelineLayout;
	}

	PipelineHandle VkRenderDevice::registerPipeline(VkResult result, VkPipeline pipeline, VkPipelineBindPoint bind_point)
	{
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create pipeline: {}", string_VkResult(result));
			return {};
		}

		return ToGpuHandle<PipelineTag>(pipelines.create(VkPipelineRecord { pipeline, bind_point }));
	}
}
