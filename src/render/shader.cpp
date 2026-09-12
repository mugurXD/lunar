#include <lunar/render/shader.hpp>
#include <lunar/file/text_file.hpp>
#include <lunar/exp/utils/lexer.hpp>
#include <lunar/exp/utils/token.hpp>
#include <variant>
#include <memory>
#include <format>

#ifdef LUNAR_VULKAN
#	include <lunar/render/internal/render_vk.hpp>
#endif

namespace Render
{
	GraphicsShaderBuilder& GraphicsShaderBuilder::useRenderContext(std::shared_ptr<RenderContext>& context)
	{
		this->context = context;
		return *this;
	}
	
	GraphicsShaderBuilder& GraphicsShaderBuilder::fromVertexSourceFile(const Fs::Path& path)
	{
		vertexPath = path;
		return *this;
	}

	GraphicsShaderBuilder& GraphicsShaderBuilder::fromFragmentSourceFile(const Fs::Path& path)
	{
		fragmentPath = path;
		return *this;
	}

	GraphicsShader GraphicsShaderBuilder::build()
	{
#		ifdef LUNAR_VULKAN
		auto& vk_ctx      = getVulkanContext(context);
		
		auto  shader      = GraphicsShader();

		auto push_ranges = vk::PushConstantRange
		{
			.stageFlags = vk::ShaderStageFlagBits::eVertex,
			.offset     = 0,
			.size       = sizeof(UniformBufferData),
		};

		auto  layout_info = vk::PipelineLayoutCreateInfo
		{
			.pushConstantRangeCount = 1,
			.pPushConstantRanges    = &push_ranges
		};

		shader._vkLayout   = vk_ctx
			.getDevice()
			.createPipelineLayout(layout_info);

		shader._vkPipeline = VulkanPipelineBuilder()
			.useVulkanContext(&vk_ctx)
			.useLayout(shader._vkLayout)
			.setVertexShader(vertexPath)
			.setFragmentShader(fragmentPath)
			.setColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat)
			.setPolygonMode(vk::PolygonMode::eFill)
			.setInputTopology(vk::PrimitiveTopology::eTriangleList)
			.setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise)
			.noMultisampling()
			.alphaBlending()
			.setDepthFormat(vk::Format::eD32Sfloat)
			.enableDepthTesting(true, vk::CompareOp::eGreaterOrEqual)
			.create();

		return shader;
#		endif

#ifdef LUNAR_OPENGL
		return _glBuild();
#endif
	}
}

//#include <lunar/render/shader.hpp>
//#include <lunar/render/render_context.hpp>
//#include <lunar/debug.hpp>
//
//#ifdef LUNAR_VULKAN
//#	include <lunar/render/internal/render_vk.hpp>
//#	include <vulkan/vulkan.hpp>
//
//#	if LUNAR_RUNTIME_SHADER_BUILD == 1
//#		include <glslang/Include/glslang_c_interface.h>
//#		include <glslang/Public/resource_limits_c.h>
//#	endif
//#endif
//
//#include <lunar/file/binary_file.hpp>
//
//namespace Render
//{
//	inline Fs::Path GetBinaryShaderPath(const std::string_view& name)
//	{
//#		ifdef LUNAR_VULKAN
//		return Fs::dataDirectory()
//			.append("shader-bin/")
//			.append(std::format("{}.spv", name));
//#		else
//		DEBUG_NOT_IMPLEMENTED();
//#		endif
//	}
//
//	ShaderBuilder& ShaderBuilder::fromVertexBinaryFile(const std::string_view& name)
//	{
//		auto bin_file = Fs::BinaryFile(GetBinaryShaderPath(name));
//		_vertCode = std::string(bin_file.content.data(), bin_file.content.size());
//		return *this;
//	}
//
//	ShaderBuilder& ShaderBuilder::fromFragmentBinaryFile(const std::string_view& name)
//	{
//		auto bin_file = Fs::BinaryFile(GetBinaryShaderPath(name));
//		_fragCode = std::string(bin_file.content.data(), bin_file.content.size());
//		return *this;
//	}
//
//	ShaderBuilder& ShaderBuilder::renderContext(std::shared_ptr<RenderContext>& context)
//	{
//		_renderCtx = context;
//		return *this;
//	}
//
//	ShaderBuilder& ShaderBuilder::uniformVariable(const std::string_view& name, ShaderVariableValueT type)
//	{
//		_variables[_variableCount] = {
//			.type      = ShaderVariableType::eUniform,
//			.valueType = type,
//			.name      = name
//		};
//
//		_variableCount++;
//
//		// TODO: bounds check
//		return *this;
//	}
//
//	GraphicsShader::GraphicsShader(const ShaderBuilder& builder) 
//		: initialized(false),
//		Identifiable()
//	{
//		init(builder);
//	}
//
//	GraphicsShader::GraphicsShader()
//		: initialized(false),
//		Identifiable()
//	{
//	}
//
//	GraphicsShader::~GraphicsShader()
//	{
//		destroy();
//	}
//
//	void GraphicsShader::init(const ShaderBuilder& builder)
//	{
//		if (initialized)
//			return;
//		
//		// TOOD: check if builder variables were initialized
//
//		renderCtx = builder._renderCtx;
//
//#		ifdef LUNAR_VULKAN
//		_vkInit(builder);
//#		endif
//
//		initialized = true;
//	}
//
//	void GraphicsShader::destroy()
//	{
//		if (!initialized)
//			return;
//		
//#		ifdef LUNAR_VULKAN
//		_vkDestroy();
//#		endif
//		initialized = false;
//	}
//
//#ifdef LUNAR_VULKAN
//	inline vk::ShaderModule CreateShaderModule
//	(
//		VulkanContext& context,
//		const std::string_view& code
//	)
//	{
//		vk::ShaderModuleCreateInfo module_info = {
//			.codeSize = code.size(),
//			.pCode    = reinterpret_cast<const uint32_t*>(code.data())
//		};
//
//		return context.getDevice()
//			.createShaderModule(module_info);
//	}
//
//#	if LUNAR_RUNTIME_SHADER_BUILD == 1
//	inline vk::ShaderModule CompileAndCreateShaderModule
//	(
//		VulkanContext& context,
//		const std::string_view& code,
//		glslang_stage_t stage
//	)
//	{
//		const glslang_input_t input =
//		{
//			.language                          = GLSLANG_SOURCE_GLSL,
//			.stage                             = stage,
//			.client                            = GLSLANG_CLIENT_VULKAN,
//			.client_version                    = GLSLANG_TARGET_VULKAN_1_3,
//			.target_language                   = GLSLANG_TARGET_SPV,
//			.target_language_version           = GLSLANG_TARGET_SPV_1_6,
//			.code                              = code.data(),
//			.default_version                   = 100,
//			.default_profile                   = GLSLANG_NO_PROFILE,
//			.force_default_version_and_profile = false,
//			.forward_compatible                = true,
//			.messages                          = GLSLANG_MSG_DEFAULT_BIT,
//			.resource                          = glslang_default_resource()
//		};
//
//		glslang_initialize_process();
//		glslang_shader_t* shader = glslang_shader_create(&input);
//		if (!glslang_shader_preprocess(shader, &input))
//		{
//			// TODO: error
//			// use glslang_shader_get_info_log() and glslang_shader_get_info_debug_log()
//		}
//		if (!glslang_shader_parse(shader, &input))
//		{
//			// TODO: error
//			// use glslang_shader_get_info_log() and glslang_shader_get_info_debug_log()
//		}
//
//		glslang_program_t* program = glslang_program_create();
//		glslang_program_add_shader(program, shader);
//		if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
//		{
//			// TODO: error
//			// use glslang_program_get_info_log() and glslang_program_get_info_debug_log();
//		}
//
//		glslang_program_SPIRV_generate(program, input.stage);
//		if (glslang_program_SPIRV_get_messages(program))
//		{
//			DEBUG_LOG("glslang: {}", glslang_program_SPIRV_get_messages(program));
//		}
//
//		vk::ShaderModuleCreateInfo module_info = {
//			.codeSize = glslang_program_SPIRV_get_size(program) * sizeof(unsigned int),
//			.pCode    = glslang_program_SPIRV_get_ptr(program)
//		};
//
//		auto shader_mod = context.getDevice()
//							.createShaderModule(module_info);
//
//		glslang_program_delete(program);
//		glslang_shader_delete(shader);
//
//		return shader_mod;
//	}
//#	endif
//
//	void GraphicsShader::_vkInit(const ShaderBuilder& builder)
//	{
//		auto& vulkan_ctx = getVulkanContext(renderCtx);
//		auto& device = vulkan_ctx.getDevice();
//
//		auto vertex_mod = CreateShaderModule(vulkan_ctx, builder._vertCode);
//		auto fragment_mod = CreateShaderModule(vulkan_ctx, builder._fragCode);
//
//		// TODO: figure out runtime shader build?
//		//auto vertex_mod = CreateShaderModule(vulkan_ctx, vertexCode, GLSLANG_STAGE_VERTEX);
//		//auto fragment_mod = CreateShaderModule(vulkan_ctx, fragmentCode, GLSLANG_STAGE_FRAGMENT);
//
//		vk::PipelineShaderStageCreateInfo stage_infos[2] = 
//		{
//			{
//				.stage  = vk::ShaderStageFlagBits::eVertex,
//				.module = vertex_mod,
//				.pName  = "main"
//			},
//			{
//				.stage  = vk::ShaderStageFlagBits::eFragment,
//				.module = fragment_mod,
//				.pName  = "main"
//			}
//		};
//
//		std::array<vk::DynamicState, 2> dynamic_states =
//		{
//			vk::DynamicState::eViewport,
//			vk::DynamicState::eScissor
//		};
//
//		vk::PipelineDynamicStateCreateInfo dyn_state_info = {
//			.dynamicStateCount = dynamic_states.size(),
//			.pDynamicStates    = dynamic_states.data()
//		};
//
//		vk::PipelineViewportStateCreateInfo viewport_info = {
//			.viewportCount = 1,
//			.scissorCount  = 1
//		};
//
//		vk::PipelineVertexInputStateCreateInfo vert_input_info = {
//			.vertexBindingDescriptionCount  = 0,
//			.vertexAttributeDescriptionCount = 0
//		};
//
//		vk::PipelineInputAssemblyStateCreateInfo input_assembly_info = {
//			.topology               = vk::PrimitiveTopology::eTriangleList,
//			.primitiveRestartEnable = VK_FALSE
//		};
//
//		vk::PipelineRasterizationStateCreateInfo rasterizer_info = {
//			.depthClampEnable        = VK_FALSE,
//			.rasterizerDiscardEnable = VK_FALSE,
//			.polygonMode             = vk::PolygonMode::eFill,
//			.cullMode                = vk::CullModeFlagBits::eBack,
//			.frontFace               = vk::FrontFace::eClockwise,
//			.depthBiasEnable         = VK_FALSE,
//			.lineWidth               = 1.f,
//		};
//
//		vk::PipelineMultisampleStateCreateInfo multisample_info = {
//			.rasterizationSamples = vk::SampleCountFlagBits::e1,
//			.sampleShadingEnable  = VK_FALSE
//		};
//
//		vk::PipelineColorBlendAttachmentState col_blend_attachm = {
//			.blendEnable    = VK_TRUE,
//			.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
//			.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
//			.srcAlphaBlendFactor = vk::BlendFactor::eOne,
//			.dstAlphaBlendFactor = vk::BlendFactor::eZero,
//			.alphaBlendOp        = vk::BlendOp::eAdd,
//			.colorWriteMask      = vk::ColorComponentFlagBits::eR |
//									vk::ColorComponentFlagBits::eG |
//									vk::ColorComponentFlagBits::eB |
//									vk::ColorComponentFlagBits::eA,
//		};
//
//		vk::PipelineColorBlendStateCreateInfo col_blend_info = {
//			.logicOpEnable   = VK_FALSE,
//			.attachmentCount = 1,
//			.pAttachments    = &col_blend_attachm
//		};
//
//		vk::GraphicsPipelineCreateInfo pipeline_info = {
//			.stageCount          = 2,
//			.pStages             = stage_infos,
//			.pVertexInputState   = &vert_input_info,
//			.pInputAssemblyState = &input_assembly_info,
//			.pViewportState      = &viewport_info,
//			.pRasterizationState = &rasterizer_info,
//			.pMultisampleState   = &multisample_info,
//			.pColorBlendState    = &col_blend_info,
//			.pDynamicState       = &dyn_state_info,
//			.layout              = vulkan_ctx.getDefaultGraphicsLayout(),
//			.renderPass          = vulkan_ctx.getDefaultRenderPass(),
//			.subpass             = 0,
//		};
//
//		_vkPipelineLayout = nullptr; // TODO: add custom layout support
//		_vkPipeline = device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info).value; // TODO: error checking
//
//		device.destroyShaderModule(vertex_mod);
//		device.destroyShaderModule(fragment_mod);
//	}
//
//	void GraphicsShader::_vkDestroy()
//	{
//		auto& vulkan_ctx = getVulkanContext(renderCtx);
//		auto& device = vulkan_ctx.getDevice();
//		device.waitIdle();
//		device.destroyPipeline(_vkPipeline);
//		// TODO: destroy custom layout
//	}
//
//	vk::Pipeline& GraphicsShader::getVkPipeline()
//	{
//		return _vkPipeline;
//	}
//
//	vk::PipelineLayout& GraphicsShader::getVkLayout()
//	{
//		return _vkPipelineLayout;
//	}
//#endif
//}
