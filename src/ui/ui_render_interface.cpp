#include "ui_render_interface.hpp"

#include <lunar/render/shader.hpp>
#include <lunar/debug.hpp>

#include "../../stb_image.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

namespace lunar::UI::imp
{
	namespace
	{
		constexpr float    ORTHO_NEAR        = -1.f;
		constexpr float    ORTHO_FAR         = 1.f;
		constexpr int      RGBA_CHANNELS     = 4;
		constexpr uint32_t HANDLE_INDEX_BITS = 32;
		constexpr uint64_t HANDLE_INDEX_MASK = (uint64_t(1) << HANDLE_INDEX_BITS) - 1;
		constexpr float    UNORM_MAX         = 255.f;

		static_assert(sizeof(Rml::Vertex) == 20, "The UI shader reads vertices with a 20 byte stride");
		static_assert(sizeof(Rml::TextureHandle) >= sizeof(uint64_t), "Handles are packed into 64 bits");

		struct GeometryTag;

		struct UiConstants
		{
			glm::mat4 transform     = glm::mat4(1.f);
			glm::vec2 translation   = {};
			uint64_t  vertexAddress = 0;
			uint32_t  texture       = Render::INVALID_TEXTURE_INDEX;
			uint32_t  linearOutput  = 0;
		};

		template<typename Tag>
		uintptr_t ToRmlHandle(Render::GpuHandle<Tag> handle)
		{
			return static_cast<uintptr_t>((uint64_t(handle.generation) << HANDLE_INDEX_BITS) | handle.index);
		}

		template<typename Tag>
		Render::GpuHandle<Tag> FromRmlHandle(uintptr_t handle)
		{
			return { static_cast<uint32_t>(handle & HANDLE_INDEX_MASK), static_cast<uint32_t>(uint64_t(handle) >> HANDLE_INDEX_BITS) };
		}

		bool IsSrgb(Render::Format format)
		{
			return format == Render::Format::eRGBA8Srgb || format == Render::Format::eBGRA8Srgb;
		}

		void Premultiply(std::span<Rml::byte> pixels)
		{
			for (size_t pixel = 0; pixel + RGBA_CHANNELS <= pixels.size(); pixel += RGBA_CHANNELS)
			{
				const float alpha = pixels[pixel + RGBA_CHANNELS - 1] / UNORM_MAX;
				for (int channel = 0; channel < RGBA_CHANNELS - 1; channel++)
					pixels[pixel + channel] = static_cast<Rml::byte>(pixels[pixel + channel] * alpha + 0.5f);
			}
		}
	}

	UiRenderInterface::UiRenderInterface(Render::RenderDevice& device, Render::Format color_format) noexcept
		: device(device),
		linearOutput(IsSrgb(color_format))
	{
		const std::vector<char> vertex_shader   = Render::LoadShader("ui.vert");
		const std::vector<char> fragment_shader = Render::LoadShader("ui.frag");

		pipeline = device.createGraphicsPipeline({
			.vertexShader   = std::as_bytes(std::span(vertex_shader)),
			.fragmentShader = std::as_bytes(std::span(fragment_shader)),
			.colorFormats   = std::span(&color_format, 1),
			.cullMode       = Render::CullMode::eNone,
			.blendMode      = Render::BlendMode::ePremultipliedAlpha
		});
	}

	UiRenderInterface::~UiRenderInterface() noexcept
	{
		geometries.forEach([&](PoolHandle<Geometry>, Geometry& geometry) {
			device.destroyBuffer(geometry.buffer);
		});

		device.destroyPipeline(pipeline);
	}

	void UiRenderInterface::beginFrame(Render::CommandList& frame_commands, Render::Extent2D frame_extent)
	{
		commands       = &frame_commands;
		extent         = frame_extent;
		projection     = glm::ortho(0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, ORTHO_NEAR, ORTHO_FAR);
		transform      = glm::mat4(1.f);
		scissorEnabled = false;

		commands->bindPipeline(pipeline);
		applyScissor();
	}

	void UiRenderInterface::endFrame()
	{
		commands = nullptr;
	}

	Rml::CompiledGeometryHandle UiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
	{
		const size_t           vertex_bytes = vertices.size() * sizeof(Rml::Vertex);
		const size_t           index_bytes  = indices.size() * sizeof(int);
		std::vector<std::byte> data(vertex_bytes + index_bytes);

		std::memcpy(data.data(), vertices.data(), vertex_bytes);
		std::memcpy(data.data() + vertex_bytes, indices.data(), index_bytes);

		const Render::BufferHandle buffer = device.createBuffer({
			data.size(),
			Render::BufferUsageFlags(Render::BufferUsageFlagBits::eStorage) | Render::BufferUsageFlagBits::eIndex,
			Render::MemoryLocation::eUpload
		}, data);

		if (buffer == Render::BufferHandle {})
			return 0;

		const Geometry geometry =
		{
			.buffer        = buffer,
			.vertexAddress = device.getBufferAddress(buffer),
			.indexOffset   = vertex_bytes,
			.indexCount    = static_cast<uint32_t>(indices.size())
		};

		return ToRmlHandle(Render::ToGpuHandle<GeometryTag>(geometries.create(geometry)));
	}

	void UiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture)
	{
		const Geometry* geometry = geometries.get(Render::FromGpuHandle(geometries, FromRmlHandle<GeometryTag>(handle)));
		if (geometry == nullptr || commands == nullptr)
			return;

		commands->pushConstants(UiConstants {
			.transform     = projection * transform,
			.translation   = { translation.x, translation.y },
			.vertexAddress = geometry->vertexAddress,
			.texture       = texture != 0 ? device.getTextureIndex(FromRmlHandle<Render::ImageTag>(texture)) : Render::INVALID_TEXTURE_INDEX,
			.linearOutput  = linearOutput
		});

		commands->bindIndexBuffer(geometry->buffer, geometry->indexOffset, Render::IndexType::eUint32);
		commands->drawIndexed(geometry->indexCount);
	}

	void UiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle handle)
	{
		const PoolHandle<Geometry> stored   = Render::FromGpuHandle(geometries, FromRmlHandle<GeometryTag>(handle));
		const Geometry*            geometry = geometries.get(stored);
		if (geometry == nullptr)
			return;

		device.destroyBuffer(geometry->buffer);
		geometries.destroy(stored);
	}

	Rml::TextureHandle UiRenderInterface::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source)
	{
		int      width    = 0;
		int      height   = 0;
		int      channels = 0;
		stbi_uc* loaded   = stbi_load(source.c_str(), &width, &height, &channels, RGBA_CHANNELS);
		if (loaded == nullptr)
		{
			DEBUG_ERROR("Failed to load UI image '{}': {}", source, stbi_failure_reason());
			return 0;
		}

		std::vector<Rml::byte> pixels(loaded, loaded + static_cast<size_t>(width) * height * RGBA_CHANNELS);
		stbi_image_free(loaded);

		Premultiply(pixels);
		dimensions = { width, height };
		return GenerateTexture(pixels, dimensions);
	}

	Rml::TextureHandle UiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions)
	{
		const Render::ImageHandle image = device.createImage({
			.extent = { static_cast<uint32_t>(dimensions.x), static_cast<uint32_t>(dimensions.y) },
			.format = Render::Format::eRGBA8Unorm,
			.usage  = Render::ImageUsageFlags(Render::ImageUsageFlagBits::eSampled)
		});

		if (image == Render::ImageHandle {})
			return 0;

		device.uploadImage(image, std::as_bytes(std::span(source.data(), source.size())));
		return ToRmlHandle(image);
	}

	void UiRenderInterface::ReleaseTexture(Rml::TextureHandle texture)
	{
		device.destroyImage(FromRmlHandle<Render::ImageTag>(texture));
	}

	void UiRenderInterface::EnableScissorRegion(bool enable)
	{
		scissorEnabled = enable;
		applyScissor();
	}

	void UiRenderInterface::SetScissorRegion(Rml::Rectanglei region)
	{
		const int32_t left   = std::clamp(region.Left(),   0, static_cast<int32_t>(extent.width));
		const int32_t top    = std::clamp(region.Top(),    0, static_cast<int32_t>(extent.height));
		const int32_t right  = std::clamp(region.Right(),  left, static_cast<int32_t>(extent.width));
		const int32_t bottom = std::clamp(region.Bottom(), top,  static_cast<int32_t>(extent.height));

		scissor = { .offset = { left, top }, .extent = { static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top) } };
		applyScissor();
	}

	void UiRenderInterface::SetTransform(const Rml::Matrix4f* matrix)
	{
		transform = matrix != nullptr ? glm::make_mat4(matrix->data()) : glm::mat4(1.f);
	}

	void UiRenderInterface::applyScissor()
	{
		if (commands != nullptr)
			commands->setScissor(scissorEnabled ? scissor : Render::Rect2D { .extent = extent });
	}
}
