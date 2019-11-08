#include "GodRaysDirLight.hpp"
#include "EntityManager.hpp"

#include "components/LightComponent.hpp"
#include "components/AdjustableComponent.hpp"
#include "components/ShaderComponent.hpp"
#include "components/GodRaysComponent.hpp"

#include "helpers/LightHelper.hpp"
#include "common/systems/opengl/ShaderHelper.hpp"

#include "shaders/QuadSrc.hpp"
#include "ShadowMap.hpp"

namespace kengine::Shaders {
	GodRaysDirLight::GodRaysDirLight(kengine::EntityManager & em)
		: Program(true, pmeta_nameof(GodRaysDirLight)),
		_em(em)
	{
	}

	void GodRaysDirLight::init(size_t firstTextureID, size_t screenWidth, size_t screenHeight, GLuint gBufferFBO) {
		initWithShaders<GodRaysDirLight>(putils::make_vector(
			ShaderDescription{ src::Quad::Vert::glsl, GL_VERTEX_SHADER },
			ShaderDescription{ src::GodRays::Frag::glsl, GL_FRAGMENT_SHADER },
			ShaderDescription{ src::DirLight::GetDirection::glsl, GL_FRAGMENT_SHADER },
			ShaderDescription{ src::CSM::Frag::glsl, GL_FRAGMENT_SHADER }
		));

		_shadowMapTextureID = (GLuint)firstTextureID;
		for (size_t i = 0; i < lengthof(_shadowMap); ++i)
			_shadowMap[i] = _shadowMapTextureID + i;
	}

	void GodRaysDirLight::run(const Parameters & params) {
		use();

		ShaderHelper::Enable _(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_ONE, GL_ONE);


		_inverseView = glm::inverse(params.view);
		_inverseProj = glm::inverse(params.proj);
		_viewPos = params.camPos;
		_screenSize = putils::Point2f(params.viewPort.size);

		for (const auto &[e, light, depthMap, comp] : _em.getEntities<DirLightComponent, CSMComponent, GodRaysComponent>()) {
			_scattering = comp.scattering;
			_nbSteps = comp.nbSteps;
			_defaultStepLength = comp.defaultStepLength;
			_intensity = comp.intensity;

			drawLight(light, depthMap, params);
		}
	}

	void GodRaysDirLight::drawLight(const DirLightComponent & light, const CSMComponent & depthMap, const Parameters & params) {
		_color = light.color;
		_direction = light.direction;

		for (int i = 0; i < lengthof(depthMap.textures); ++i) {
			glActiveTexture(GL_TEXTURE0 + _shadowMapTextureID + i);
			glBindTexture(GL_TEXTURE_2D, depthMap.textures[i]);
			_lightSpaceMatrix[i] = LightHelper::getCSMLightSpaceMatrix(light, params, i);
			_cascadeEnd[i] = LightHelper::getCSMCascadeEnd(light, i);
		}

		ShaderHelper::shapes::drawQuad();
	}
}
