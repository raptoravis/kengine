#include "DirLight.hpp"

#include "EntityManager.hpp"
#include "helpers/LightHelper.hpp"

#include "components/LightComponent.hpp"
#include "components/AdjustableComponent.hpp"
#include "components/ShaderComponent.hpp"

#include "common/systems/opengl/ShaderHelper.hpp"
#include "shaders/QuadSrc.hpp"

namespace kengine {
	static bool DEBUG_CSM = false;
}

namespace kengine::Shaders {
	DirLight::DirLight(kengine::EntityManager & em)
		: Program(true, pmeta_nameof(DirLight)),
		_em(em)
	{
		_em += [](kengine::Entity & e) { e += kengine::AdjustableComponent("[Render/Lights] Debug CSM", &DEBUG_CSM); };
	}

	void DirLight::init(size_t firstTextureID, size_t screenWidth, size_t screenHeight, GLuint gBufferFBO) {
		initWithShaders<DirLight>(putils::make_vector(
			ShaderDescription{ src::Quad::Vert::glsl, GL_VERTEX_SHADER },
			ShaderDescription{ src::CSM::Frag::glsl, GL_FRAGMENT_SHADER },
			ShaderDescription{ src::DirLight::Frag::glsl, GL_FRAGMENT_SHADER }
		));

		_shadowMapTextureID = firstTextureID;
		for (size_t i = 0; i < lengthof(_shadowMap); ++i)
			_shadowMap[i] = _shadowMapTextureID + i;
	}

	void DirLight::run(const Parameters & params) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		ShaderHelper::Enable __b(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_ONE, GL_ONE);

		use();

		_proj = params.proj;
		_view = params.view;

		_debugCSM = DEBUG_CSM;
		_viewPos = params.camPos;
		_screenSize = putils::Point2f(params.viewPort.size);

		for (auto &[e, light] : _em.getEntities<DirLightComponent>()) {
			const putils::Point3f pos = { params.camPos.x, params.camPos.y, params.camPos.z };

			if (light.castShadows)
				for (const auto & [shadowMapEntity, shader, comp] : _em.getEntities<LightingShaderComponent, ShadowMapShaderComponent>()) {
					auto & shadowMap = static_cast<ShadowMapShader &>(*shader.shader);
					shadowMap.run(e, light, params);
				}

			use();
			setLight(light);

			const auto & depthMap = e.get<CSMComponent>();
			for (size_t i = 0; i < lengthof(_shadowMap); ++i) {
				glActiveTexture((GLenum)(GL_TEXTURE0 + _shadowMapTextureID + i));
				glBindTexture(GL_TEXTURE_2D, depthMap.textures[i]);
				_lightSpaceMatrix[i] = LightHelper::getCSMLightSpaceMatrix(light, params, i);
			}

			ShaderHelper::shapes::drawQuad();
		}
	}

	void DirLight::setLight(const DirLightComponent & light) {
		_color = light.color;
		_direction = light.direction;

		_ambientStrength = light.ambientStrength;
		_diffuseStrength = light.diffuseStrength;
		_specularStrength = light.specularStrength;
		_pcfSamples = light.shadowPCFSamples;
		_bias = light.shadowMapBias;

		for (size_t i = 0; i < KENGINE_CSM_COUNT; ++i)
			_cascadeEnd[i] = LightHelper::getCSMCascadeEnd(light, i);
	}
}
