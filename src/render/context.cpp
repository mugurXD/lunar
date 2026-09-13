#include <lunar/render/context.hpp>
#include <lunar/render/program.hpp>
#include <lunar/debug/log.hpp>
#include <lunar/debug/assert.hpp>

namespace lunar::Render
{
	LUNAR_REF_HANDLE_IMPL(GpuTexture);
	LUNAR_REF_HANDLE_IMPL(GpuProgram);
	LUNAR_REF_HANDLE_IMPL(GpuBuffer);

	void RenderContext_T::useCamera(const Camera& camera)
	{
		this->renderCamera = &camera;
	}

	void RenderContext_T::useCamera(const Camera* camera)
	{
		this->renderCamera = camera;
	}

	Window_T RenderContext_T::createWindow
	(
		int                     width,
		int                     height,
		bool                    fullscreen,
		const std::string_view& name,
		int                     msaa,
		bool                    vsync
	)
	{
		return Window_T(this, width, height, fullscreen, name, msaa, vsync);
	}

	GpuTexture RenderContext_T::createTexture
	(
		int               width,
		int               height,
		void*             data,
		TextureFormat     srcFormat,
		TextureDataFormat dataFormat,
		TextureFormat     dstFormat,
		TextureType       type,
		TextureFiltering  minFiltering,
		TextureFiltering  magFiltering,
		TextureWrapping   wrapping,
		TextureFlags      flags
	)
	{
		GpuTexture_T* texture = new GpuTexture_T
		(
			this,
			width, height,
			data, srcFormat, dataFormat,
			dstFormat, type,
			minFiltering,
			magFiltering,
			wrapping, 
			flags
		);

		textures.push_back(texture);
		return RefHandle<GpuTexture_T>(textures, textures.size() - 1);
	}

	GpuProgram RenderContext_T::createProgram
	(
		GpuProgramType programType,
		const std::initializer_list<GpuProgramStageData>& stages
	)
	{
		GpuProgram_T* program = new GpuProgram_T(this, programType, stages);
		programs.push_back(program);
		return RefHandle<GpuProgram_T>(programs, programs.size() - 1);
	}

	GpuProgram RenderContext_T::createProgram
	(
		GpuProgramType programType,
		const std::span<GpuProgramStageData>& stages
	)
	{
		GpuProgram_T* program = new GpuProgram_T(this, programType, stages);
		programs.push_back(program);
		return RefHandle<GpuProgram_T>(programs, programs.size() - 1);
	}

	GpuBuffer RenderContext_T::createBuffer
	(
		GpuBufferType type,
		GpuBufferUsageFlags usageFlags,
		size_t size,
		void* data
	)
	{
		GpuBuffer_T* buffer = new GpuBuffer_T(this, type, usageFlags, size, data);
		buffers.push_back(buffer);
		return RefHandle<GpuBuffer_T>(buffers, buffers.size() - 1);
	}

	GpuMesh RenderContext_T::createMesh
	(
		GpuBuffer    vertexBuffer,
		GpuBuffer    indexBuffer,
		MeshTopology topology,
		GpuBuffer    materialsBuffer,
		GpuTexture   materialsAtlas
	)
	{
		return meshes.create(this, vertexBuffer, indexBuffer, topology, materialsBuffer, materialsAtlas);
	}

	GpuCubemap RenderContext_T::createCubemap
	(
		int   width,
		int   height,
		void* data,
		bool  isSourceHdr
	)
	{
		return cubemaps.create(this, width, height, data, isSourceHdr);
	}

	void RenderContext_T::loadDefaultMeshes()
	{
		DEBUG_ASSERT(meshes.size() == 0, "Default meshes must be built before any other meshes.");
		DEBUG_ASSERT(defaultMeshesBuilt == false, "Default meshes were already built.");

		const auto cube_vertices = std::vector<Vertex>
		{
			Vertex{ { -1.f,-1.f,-1.f}, 0, { 0, 0, 0 }, 0, { 1.f, 0.f, 0.f, 1.f } },
			Vertex{ {  1.f, 1.f,-1.f}, 1, { 0, 0, 0 }, 1, { 0.f, 0.f, 1.f, 1.f } },
			Vertex{ {  1.f,-1.f,-1.f}, 1, { 0, 0, 0 }, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ {  1.f, 1.f,-1.f}, 1, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f,-1.f,-1.f}, 0, { 0, 0, 0 }, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f, 1.f,-1.f}, 0, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },

			Vertex{ { -1.f,-1.f, 1.f}, 0, { 0, 0, 0 }, 0, { 1.f, 0.f, 0.f, 1.f } },
			Vertex{ {  1.f,-1.f, 1.f}, 1, { 0, 0, 0 }, 1, { 0.f, 0.f, 1.f, 1.f } },
			Vertex{ {  1.f, 1.f, 1.f}, 1, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ {  1.f, 1.f, 1.f}, 1, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f, 1.f, 1.f}, 0, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f,-1.f, 1.f}, 0, { 0, 0, 0 }, 0, { 0.f, 1.f, 0.f, 1.f } },

			Vertex{ { -1.f, 1.f, 1.f}, 1, { 0, 0, 0 }, 0, { 1.f, 0.f, 0.f, 1.f } },
			Vertex{ { -1.f, 1.f,-1.f}, 1, { 0, 0, 0 }, 1, { 0.f, 0.f, 1.f, 1.f } },
			Vertex{ { -1.f,-1.f,-1.f}, 0, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f,-1.f,-1.f}, 0, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f,-1.f, 1.f}, 0, { 0, 0, 0 }, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f, 1.f, 1.f}, 1, { 0, 0, 0 }, 0, { 0.f, 1.f, 0.f, 1.f } },

			Vertex{ {  1.f, 1.f, 1.f}, 1, { 0, 0, 0 }, 0, { 1.f, 0.f, 0.f, 1.f } },
			Vertex{ {  1.f,-1.f,-1.f}, 0, { 0, 0, 0 }, 1, { 0.f, 0.f, 1.f, 1.f } },
			Vertex{ {  1.f, 1.f,-1.f}, 1, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ {  1.f,-1.f,-1.f}, 0, { 0, 0, 0 }, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ {  1.f, 1.f, 1.f}, 1, { 0, 0, 0 }, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ {  1.f,-1.f, 1.f}, 0, { 0, 0, 0 }, 0, { 0.f, 1.f, 0.f, 1.f } },

			Vertex{ { -1.f,-1.f,-1.f}, 0, { 0, 0, 1.f}, 1, { 1.f, 0.f, 0.f, 1.f } },
			Vertex{ {  1.f,-1.f,-1.f}, 1, { 0, 0, 1.f}, 1, { 0.f, 0.f, 1.f, 1.f } },
			Vertex{ {  1.f,-1.f, 1.f}, 1, { 0, 0, 1.f}, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ {  1.f,-1.f, 1.f}, 1, { 0, 0, 1.f}, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f,-1.f, 1.f}, 0, { 0, 0, 1.f}, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f,-1.f,-1.f}, 0, { 0, 0, 1.f}, 1, { 0.f, 1.f, 0.f, 1.f } },

			Vertex{ { -1.f, 1.f,-1.f}, 0, { 0, 0, 1.f}, 1, { 1.f, 0.f, 0.f, 1.f } },
			Vertex{ {  1.f, 1.f, 1.f}, 1, { 0, 0, 1.f}, 0, { 0.f, 0.f, 1.f, 1.f } },
			Vertex{ {  1.f, 1.f,-1.f}, 1, { 0, 0, 1.f}, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ {  1.f, 1.f, 1.f}, 1, { 0, 0, 1.f}, 0, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f, 1.f,-1.f}, 0, { 0, 0, 1.f}, 1, { 0.f, 1.f, 0.f, 1.f } },
			Vertex{ { -1.f, 1.f, 1.f}, 0, { 0, 0, 1.f}, 0, { 0.f, 1.f, 0.f, 1.f } },
		};

		const auto cube_indices = std::vector<uint32_t>
		{
			0, 1, 2, 3, 4, 5,
			6, 7, 8, 9, 10, 11,
			12, 13, 14, 15, 16, 17,
			18, 19, 20, 21, 22, 23,
			24, 25, 26, 27, 28, 29,
			30, 31, 32, 33, 34, 35
		};

		const auto quad_vertices = std::vector<Vertex>
		{
			Vertex { { -1.f, 1.f, 0.f }, 0.f, { 0.f, 0.f, 0.f }, 1.f, { 0.f, 0.f, 0.f, 0.f } },
			Vertex { { -1.f,-1.f, 0.f }, 0.f, { 0.f, 0.f, 0.f }, 0.f, { 0.f, 0.f, 0.f, 0.f } },
			Vertex { {  1.f, 1.f, 0.f }, 1.f, { 0.f, 0.f, 0.f }, 1.f, { 0.f, 0.f, 0.f, 0.f } },
			Vertex { {  1.f,-1.f, 0.f }, 1.f, { 0.f, 0.f, 0.f }, 0.f, { 0.f, 0.f, 0.f, 0.f } },
		};

		const auto quad_indices = std::vector<uint32_t>
		{
			0, 1, 2,
			2, 1, 3
		};

		cubeMesh = GpuMeshBuilder()
			.useRenderContext(this)
			.fromVertexArray(cube_vertices)
			.fromIndexArray(cube_indices)
			.build();

		quadMesh = GpuMeshBuilder()
			.useRenderContext(this)
			.fromVertexArray(quad_vertices)
			.fromIndexArray(quad_indices)
			.build();

		defaultMeshesBuilt = true;
		DEBUG_LOG("Built primitive GPU meshes.");
	}

	void RenderContext_T::loadDefaultPrograms()
	{
		DEBUG_ASSERT(programs.size() == 0, "Default shaders must be built before any other shaders.");
		DEBUG_ASSERT(defaultProgramsBuilt == false, "Default shaders were already built.");

		/*
			Since we do not want the default assets to ever get cleaned up,
			the refCount is manually increased by 2 (which should, in theory,
			make sure that they never get cleaned up). 
			
			If you remove the addition, the program gets instantly destroyed
			because we are discarding their handles.
		
			This code is absolutely horrendous but I really feel there is no other
			better way of doing this.
		*/

		GpuProgramBuilder()
			.graphicsShader()
			.addVertexSource(Fs::fromData("shader-src/screen.vert"))
			.addFragmentSource(Fs::fromData("shader-src/unlit.frag"))
			.build(this)->refCount += 2;

		GpuProgramBuilder()
			.graphicsShader()
			.addVertexSource(Fs::fromData("shader-src/pbr/cubemap.vert"))
			.addFragmentSource(Fs::fromData("shader-src/pbr/equirect_to_cubemap.frag"))
			.build(this)->refCount += 2;

		GpuProgramBuilder()
			.graphicsShader()
			.addVertexSource(Fs::fromData("shader-src/pbr/cubemap.vert"))
			.addFragmentSource(Fs::fromData("shader-src/pbr/irradiance.frag"))
			.build(this)->refCount += 2;

		GpuProgramBuilder()
			.graphicsShader()
			.addVertexSource(Fs::fromData("shader-src/pbr/cubemap.vert"))
			.addFragmentSource(Fs::fromData("shader-src/pbr/prefilter.frag"))
			.build(this)->refCount += 2;

		GpuProgramBuilder()
			.graphicsShader()
			.addVertexSource(Fs::fromData("shader-src/pbr/brdf.vert"))
			.addFragmentSource(Fs::fromData("shader-src/pbr/brdf.frag"))
			.build(this)->refCount += 2;

		GpuProgramBuilder()
			.graphicsShader()
			.addVertexSource(Fs::fromData("shader-src/skybox.vert"))
			.addFragmentSource(Fs::fromData("shader-src/skybox.frag"))
			.build(this)->refCount += 2;

		GpuProgramBuilder()
			.graphicsShader()
			.addVertexSource(Fs::fromData("shader-src/pbr.vert"))
			.addFragmentSource(Fs::fromData("shader-src/pbr.frag"))
			.build(this)->refCount += 2;

		defaultProgramsBuilt = true;
		DEBUG_LOG("Built default GPU programs.");
	}

	/*
		Global render context
	*/

	namespace imp
	{
		GlobalRenderContext& GetGlobalRenderContext()
		{
			static GlobalRenderContext context = {};
			return context;
		}
	}
}
