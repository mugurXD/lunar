//#include <lunar/render/imp/gl/texture.hpp>
//#include <lunar/render/context.hpp>
//#include <lunar/debug/assert.hpp>
//
//#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//
//namespace lunar::Render
//{
//	static const glm::mat4 CAPTURE_PROJ    = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 10.f);
//	static const glm::mat4 CAPTURE_VIEWS[] =
//	{
//		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f),  glm::vec3(0.0f, -1.0f,  0.0f)),
//		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
//		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f),  glm::vec3(0.0f,  0.0f,  1.0f)),
//		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f),  glm::vec3(0.0f,  0.0f, -1.0f)),
//		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f),  glm::vec3(0.0f, -1.0f,  0.0f)),
//		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f),  glm::vec3(0.0f, -1.0f,  0.0f))
//	};
//
//	GpuTexture EquirectToCubemap(RenderContext_T* context, GpuTexture source)
//	{
//		GpuTexture environment_map = context->createTexture
//		(
//			512, 512, nullptr,
//			TextureFormat::eRGB,
//			TextureDataFormat::eFloat,
//			TextureFormat::eRGB16F,
//			TextureType::eCubemap,
//			TextureFiltering::eLinearMipmapLinear,
//			TextureFiltering::eLinear
//		);
//
//		GpuMesh    cube_mesh = context->getMesh(MeshPrimitive::eCube);
//		GpuProgram shader    = context->getProgram(GpuDefaultPrograms::eEquirectToCubemap);
//		GLuint     fbo       = context->glGetFramebuffer();
//		GLuint     rbo       = context->glGetRenderbuffer();
//
//		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
//		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
//		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
//
//		shader->use();
//		shader->uniform("projection", CAPTURE_PROJ);
//		shader->bind("equirectangularMap", 0, source);
//
//		glViewport(0, 0, 512, 512);
//		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//		for (size_t i = 0; i < 6; i++)
//		{
//			shader->uniform("view", CAPTURE_VIEWS[i]);
//
//			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environment_map->glGetHandle(), 0);
//			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//			context->draw(cube_mesh);
//		}
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//		environment_map->generateMipmaps();
//
//		return environment_map;
//	}
//
//	GpuTexture BuildIrradianceMap(RenderContext_T* context, GpuTexture environmentMap)
//	{
//		GpuTexture irradiance_map = context->createTexture
//		(
//			32, 32, nullptr,
//			TextureFormat::eRGB,
//			TextureDataFormat::eFloat,
//			TextureFormat::eRGB16F,
//			TextureType::eCubemap
//		);
//
//		GpuMesh    cube_mesh = context->getMesh(MeshPrimitive::eCube);
//		GpuProgram shader    = context->getProgram(GpuDefaultPrograms::eIrradianceMapBuilder);
//		GLuint     fbo       = context->glGetFramebuffer();
//		GLuint     rbo       = context->glGetRenderbuffer();
//
//		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
//		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
//
//		shader->use();
//		shader->uniform("projection", CAPTURE_PROJ);
//		shader->bind("environmentMap", 0, environmentMap);
//
//		glViewport(0, 0, 32, 32);
//		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//		for (size_t i = 0; i < 6; i++)
//		{
//			shader->uniform("view", CAPTURE_VIEWS[i]);
//			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradiance_map->glGetHandle(), 0);
//			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//			context->draw(cube_mesh);
//		}
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//		return irradiance_map;
//	}
//
//	GpuTexture BuildPrefilterMap(RenderContext_T* context, GpuTexture environmentMap)
//	{
//		GpuTexture prefilter_map = context->createTexture
//		(
//			128, 128, nullptr,
//			TextureFormat::eRGB,
//			TextureDataFormat::eFloat,
//			TextureFormat::eRGB16F,
//			TextureType::eCubemap,
//			TextureFiltering::eLinearMipmapLinear,
//			TextureFiltering::eLinear
//		);
//
//		prefilter_map->generateMipmaps();
//
//		GpuMesh    cube_mesh = context->getMesh(MeshPrimitive::eCube);
//		GpuProgram shader    = context->getProgram(GpuDefaultPrograms::ePrefilterMapBuilder);
//		GLuint     fbo       = context->glGetFramebuffer();
//		GLuint     rbo       = context->glGetRenderbuffer();
//
//		shader->use();
//		shader->uniform("projection", CAPTURE_PROJ);
//		shader->bind("environmentMap", 0, environmentMap);
//
//		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//		constexpr size_t MAX_MIP_LEVELS = 5;
//		for (size_t mip = 0; mip < MAX_MIP_LEVELS; mip++)
//		{
//			size_t mip_w = 128 * glm::pow(0.5, mip);
//			size_t mip_h = 128 * glm::pow(0.5, mip);
//			glBindRenderbuffer(GL_RENDERBUFFER, rbo);
//			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mip_w, mip_h);
//			glViewport(0, 0, mip_w, mip_h);
//
//			GLfloat roughness = (GLfloat)mip / (GLfloat)(MAX_MIP_LEVELS - 1);
//			shader->uniform("roughness", roughness);
//
//			for (size_t i = 0; i < 6; i++)
//			{
//				shader->uniform("view", CAPTURE_VIEWS[i]);
//
//				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilter_map->glGetHandle(), mip);
//				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//				
//				context->draw(cube_mesh);
//			}
//		}
//
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//		return prefilter_map;
//	}
//
//	GpuTexture BuildBRDFLutTexture(RenderContext_T* context)
//	{
//		GpuTexture brdf_texture = context->createTexture
//		(
//			512, 512, nullptr,
//			TextureFormat::eRG,
//			TextureDataFormat::eFloat,
//			TextureFormat::eRG16F,
//			TextureType::e2D
//		);
//
//		GpuMesh    quad_mesh = context->getMesh(MeshPrimitive::eQuad);
//		GpuProgram shader    = context->getProgram(GpuDefaultPrograms::eBrdfBuilder);
//		GLuint     fbo       = context->glGetFramebuffer();
//		GLuint     rbo       = context->glGetRenderbuffer();
//
//		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
//		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
//		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
//		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdf_texture->glGetHandle(), 0);
//
//		glViewport(0, 0, 512, 512);
//		shader->use();
//
//		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
//		context->draw(quad_mesh);
//
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//		return brdf_texture;
//	}
//
//	GpuCubemap_T::GpuCubemap_T
//	(
//		RenderContext_T* context,
//		int              width,
//		int              height,
//		void*            data,
//		bool             isSourceHdr
//	) noexcept
//	{
//		auto staging = context->createTexture
//		(
//			width, height, data,
//			TextureFormat::eRGB,
//			TextureDataFormat::eFloat,
//			TextureFormat::eRGB16F,
//			TextureType::e2D,
//			TextureFiltering::eLinear,
//			TextureFiltering::eLinear,
//			TextureWrapping::eClampToEdge
//		);
//
//		//this->environmentMap = EquirectToCubemap(context, staging);
//		//this->irradianceMap  = BuildIrradianceMap(context, environmentMap);
//		//this->prefilterMap   = BuildPrefilterMap(context, environmentMap);
//		//this->brdfLut        = BuildBRDFLutTexture(context);
//
//		// TODO: delete staging
//	}
//}
