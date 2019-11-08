#pragma once

#include "opengl/Program.hpp"
#include "opengl/Uniform.hpp"
#include "components/ShaderComponent.hpp"
#include "shaders/GodRaysSrc.hpp"
#include "shaders/ShadowMapSrc.hpp"
#include "shaders/DirLightSrc.hpp"

namespace kengine {
	class EntityManager;
	struct DirLightComponent;
}

namespace kengine::Shaders {
	class GodRaysDirLight : public putils::gl::Program,
		public src::GodRays::Frag::Uniforms,
		public src::CSM::Frag::Uniforms,
		public src::DirLight::GetDirection::Uniforms
	{
	public:
		GodRaysDirLight(kengine::EntityManager & em);

		void init(size_t firstTextureID, size_t screenWidth, size_t screenHeight, GLuint gBufferFBO) override;
		void run(const Parameters & params) override;
		
	private:
		void drawLight(const DirLightComponent & light, const CSMComponent & depthMap, const Parameters & params);

	private:
		kengine::EntityManager & _em;
		GLuint _shadowMapTextureID;

	public:
		pmeta_get_parents(
			pmeta_reflectible_parent(src::GodRays::Frag::Uniforms),
			pmeta_reflectible_parent(src::CSM::Frag::Uniforms),
			pmeta_reflectible_parent(src::DirLight::GetDirection::Uniforms)
		);
	};
}
