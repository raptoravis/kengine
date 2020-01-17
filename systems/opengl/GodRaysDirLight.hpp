#pragma once

#include "opengl/Program.hpp"
#include "opengl/Uniform.hpp"
#include "data/ShaderComponent.hpp"
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
		GodRaysDirLight(EntityManager & em);

		void init(size_t firstTextureID) override;
		void run(const Parameters & params) override;
		
	private:
		void drawLight(const DirLightComponent & light, const CSMComponent & depthMap, const Parameters & params);

	private:
		EntityManager & _em;
		GLuint _shadowMapTextureID;

	public:
		putils_reflection_parents(
			putils_reflection_parent(src::GodRays::Frag::Uniforms),
			putils_reflection_parent(src::CSM::Frag::Uniforms),
			putils_reflection_parent(src::DirLight::GetDirection::Uniforms)
		);
	};
}
