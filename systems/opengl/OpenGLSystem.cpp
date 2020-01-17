#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "opengl/Program.hpp"

#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl3.h"

#include "EntityManager.hpp"

#include "systems/InputSystem.hpp"

#include "data/ModelDataComponent.hpp"
#include "data/TextureDataComponent.hpp"
#include "data/ModelComponent.hpp"
#include "data/OpenGLModelComponent.hpp"
#include "data/ImGuiComponent.hpp"
#include "data/InputBufferComponent.hpp"
#include "data/AdjustableComponent.hpp"
#include "data/InputComponent.hpp"
#include "data/CameraComponent.hpp"
#include "data/ViewportComponent.hpp"
#include "data/WindowComponent.hpp"
#include "data/ShaderComponent.hpp"
#include "data/GBufferComponent.hpp"

#include "functions/Execute.hpp"
#include "functions/OnTerminate.hpp"
#include "functions/OnEntityCreated.hpp"
#include "functions/OnEntityRemoved.hpp"
#include "functions/OnMouseCaptured.hpp"
#include "functions/GetImGuiScale.hpp"
#include "functions/GetEntityInPixel.hpp"
#include "functions/InitGBuffer.hpp"

#include "systems/opengl/ShaderHelper.hpp"

#include "helpers/CameraHelper.hpp"
#include "rotate.hpp"

#include "OpenGLSystem.hpp"
#include "Controllers.hpp"

#include "ShadowMap.hpp"
#include "ShadowCube.hpp"
#include "SpotLight.hpp"
#include "DirLight.hpp"
#include "PointLight.hpp"
#include "LightSphere.hpp"
#include "GodRaysDirLight.hpp"
#include "GodRaysPointLight.hpp"
#include "GodRaysSpotLight.hpp"
#include "Highlight.hpp"
#include "Debug.hpp"
#include "SkyBox.hpp"
#include "Text.hpp"

#include "Timer.hpp"

#ifndef KENGINE_MAX_VIEWPORTS
# define KENGINE_MAX_VIEWPORTS 8
#endif

namespace kengine {
	namespace Input {
		static InputBufferComponent * g_buffer;
	}

	static EntityManager * g_em;
	static putils::gl::Program::Parameters g_params;
	static float g_dpiScale = 1.f;

	static size_t g_gBufferTextureCount = 0;
	static functions::GBufferAttributeIterator g_gBufferIterator = nullptr;

	// declarations
	static void execute(float deltaTime);
	static void onEntityCreated(Entity & e);
	static void onEntityRemoved(Entity & e);
	static void terminate();
	static void onMouseCaptured(Entity::ID window, bool captured);
	static Entity::ID getEntityInPixel(Entity::ID window, const putils::Point2ui & pixel);
	static void initGBuffer(size_t nbAttributes, const functions::GBufferAttributeIterator & iterator);
	//
	EntityCreator * OpenGLSystem(EntityManager & em) {
		g_em = &em;

		for (const auto & [e, buffer] : em.getEntities<InputBufferComponent>()) {
			Input::g_buffer = &buffer;
			break;
		}

		em += [](Entity & e) {
			e += AdjustableComponent{
				"ImGui", {
					{ "Scale", &g_dpiScale }
				}
			};
		};

#ifndef KENGINE_NDEBUG
		em += Controllers::ShaderController(em);
		em += Controllers::GBufferDebugger(em, g_gBufferIterator);
#endif

		g_params.nearPlane = 1.f;
		g_params.farPlane = 1000.f;

		return [](Entity & e) {
			e += functions::Execute{ execute };
			e += functions::OnEntityCreated{ onEntityCreated };
			e += functions::OnEntityRemoved{ onEntityRemoved };
			e += functions::OnTerminate{ terminate };
			e += functions::OnMouseCaptured{ onMouseCaptured };
			e += functions::GetImGuiScale{ [] { return g_dpiScale; } };
			e += functions::GetEntityInPixel{ getEntityInPixel };
			e += functions::InitGBuffer{ initGBuffer };

			e += AdjustableComponent{
				"Render/Planes", {
					{ "Near", &g_params.nearPlane },
					{ "Far", &g_params.farPlane }
				}
			};
		};
	}

	static bool g_init = false;
	static struct {
		GLFWwindow * window = nullptr;

		Entity::ID id = Entity::INVALID_ID;
		WindowComponent::string name;
		WindowComponent * comp = nullptr;
		putils::Point2i size;
		bool fullScreen;
	} g_window;

	namespace Input {
		static void onKey(GLFWwindow *, int key, int scancode, int action, int mods) {
			if (GImGui != nullptr && ImGui::GetIO().WantCaptureKeyboard)
				return;

			if (action == GLFW_PRESS)
				g_buffer->keys.try_push_back(InputBufferComponent::KeyEvent{ g_window.id, key, true });
			else if (action == GLFW_RELEASE)
				g_buffer->keys.try_push_back(InputBufferComponent::KeyEvent{ g_window.id, key, false });
		}

		static putils::Point2f lastPos{ FLT_MAX, FLT_MAX };

		static void onClick(GLFWwindow *, int button, int action, int mods) {
			if (GImGui != nullptr && ImGui::GetIO().WantCaptureMouse)
				return;

			if (action == GLFW_PRESS)
				g_buffer->clicks.try_push_back(InputBufferComponent::ClickEvent{ g_window.id, lastPos, button, true });
			else if (action == GLFW_RELEASE)
				g_buffer->clicks.try_push_back(InputBufferComponent::ClickEvent{ g_window.id, lastPos, button, false });
		}

		static void onMouseMove(GLFWwindow *, double xpos, double ypos) {
			if (lastPos.x == FLT_MAX) {
				lastPos.x = (float)xpos;
				lastPos.y = (float)ypos;
			}

			InputBufferComponent::MouseMoveEvent info;
			info.window = g_window.id;
			info.pos = { (float)xpos, (float)ypos };
			info.rel = { (float)xpos - lastPos.x, (float)ypos - lastPos.y };
			lastPos = info.pos;

			if (GImGui != nullptr && ImGui::GetIO().WantCaptureMouse)
				return;
			g_buffer->moves.try_push_back(info);
		}

		static void onScroll(GLFWwindow *, double xoffset, double yoffset) {
			if (GImGui != nullptr && ImGui::GetIO().WantCaptureMouse)
				return;
			g_buffer->scrolls.try_push_back(InputBufferComponent::MouseScrollEvent{ g_window.id, (float)xoffset, (float)yoffset, lastPos });
		}
	}

	static void terminate() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(g_window.window);
		glfwTerminate();
	}

	static void initWindow() noexcept {
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifndef KENGINE_NDEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
		// TODO: depend on g_windowComponent->fullscreen
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		g_window.window = glfwCreateWindow((int)g_window.comp->size.x, (int)g_window.comp->size.y, g_window.comp->name, nullptr, nullptr);
		// Desired size may not have been available, update to actual size
		glfwGetWindowSize(g_window.window, &g_window.size.x, &g_window.size.y);
		g_window.comp->size = g_window.size;
		glfwSetWindowAspectRatio(g_window.window, g_window.size.x, g_window.size.y);

		glfwMakeContextCurrent(g_window.window);
		glfwSetWindowSizeCallback(g_window.window, [](auto window, int width, int height) {
			g_window.size = { width, height };
			g_window.comp->size = g_window.size;
		});

		glfwSetMouseButtonCallback(g_window.window, Input::onClick);
		glfwSetCursorPosCallback(g_window.window, Input::onMouseMove);
		glfwSetScrollCallback(g_window.window, Input::onScroll);
		glfwSetKeyCallback(g_window.window, Input::onKey);
	}

	static void initImGui() noexcept {
		ImGui::CreateContext();
		auto & io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigViewportsNoTaskBarIcon = true;

		{ // Stolen from ImGui_ImplOpenGL3_CreateFontsTexture
			ImFontConfig config;
			config.SizePixels = 13.f * g_dpiScale;
			io.Fonts->AddFontDefault(&config);
			unsigned char * pixels;
			int width, height;
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

			GLint last_texture;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			glBindTexture(GL_TEXTURE_2D, (GLuint)io.Fonts->TexID);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			ImGui::GetStyle().ScaleAllSizes(g_dpiScale);
			glBindTexture(GL_TEXTURE_2D, last_texture);
		}

		ImGui_ImplGlfw_InitForOpenGL(g_window.window, true);
		ImGui_ImplOpenGL3_Init();
	}

	// declarations
	static void addShaders();
	//
	static void init() noexcept {
		g_init = true;

		for (const auto & [e, window] : g_em->getEntities<WindowComponent>()) {
			if (!window.assignedSystem.empty())
				continue;
			g_window.id = e.id;
			break;
		}

		if (g_window.id == Entity::INVALID_ID) {
			*g_em += [](Entity & e) {
				g_window.comp = &e.attach<WindowComponent>();
				g_window.comp->name = "Kengine";
				g_window.comp->size = { 1280, 720 };
				g_window.id = e.id;
			};
		}
		else
			g_window.comp = &g_em->getEntity(g_window.id).get<WindowComponent>();

		g_window.comp->assignedSystem = "OpenGL";

		g_window.name = g_window.comp->name;

		initWindow();
		initImGui();

		glewExperimental = true;
		const bool ret = glewInit();
		assert(ret == GLEW_OK);

#ifndef KENGINE_NDEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, const void * userParam) {
			if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
				std::cerr <<
				putils::termcolor::red <<
				"G: severity = 0x" << std::ios::hex << severity << std::ios::dec <<
				", message: " << message << '\n' <<
				putils::termcolor::reset;
		}, nullptr);
#endif

#ifndef KENGINE_OPENGL_NO_DEFAULT_SHADERS
		addShaders();
#endif
	}

	static void addShaders() {
		{ // GBuffer
			*g_em += [=](Entity & e) { e += makeGBufferShaderComponent<Shaders::Debug>(*g_em); };
			*g_em += [=](Entity & e) { e += makeGBufferShaderComponent<Shaders::Text>(*g_em); };
		}

		{ // Lighting
			*g_em += [&](Entity & e) {
				e += makeLightingShaderComponent<Shaders::ShadowMap>(*g_em, e);
				e += ShadowMapShaderComponent{};
			};

			*g_em += [&](Entity & e) {
				e += makeLightingShaderComponent<Shaders::ShadowCube>(*g_em);
				e += ShadowCubeShaderComponent{};
			};

			*g_em += [=](Entity & e) { e += makeLightingShaderComponent<Shaders::SpotLight>(*g_em); };
			*g_em += [=](Entity & e) { e += makeLightingShaderComponent<Shaders::DirLight>(*g_em, e); };
			*g_em += [=](Entity & e) { e += makeLightingShaderComponent<Shaders::PointLight>(*g_em); };
		}

		{ // Post lighting
			*g_em += [=](Entity & e) { e += makePostLightingShaderComponent<Shaders::GodRaysDirLight>(*g_em); };
			*g_em += [=](Entity & e) { e += makePostLightingShaderComponent<Shaders::GodRaysPointLight>(*g_em); };
			*g_em += [=](Entity & e) { e += makePostLightingShaderComponent<Shaders::GodRaysSpotLight>(*g_em); };
		}

		{ // Post process
			*g_em += [=](Entity & e) { e += makePostProcessShaderComponent<Shaders::LightSphere>(*g_em, e); };
			*g_em += [=](Entity & e) { e += makePostProcessShaderComponent<Shaders::Highlight>(*g_em); };
			*g_em += [=](Entity & e) { e += makePostProcessShaderComponent<Shaders::SkyBox>(*g_em); };
		}
	}

	// declarations
	static void initShader(putils::gl::Program & program);
	//
	static void onEntityCreated(Entity & e) {
		if (g_gBufferIterator == nullptr)
			return;

		if (e.has<GBufferShaderComponent>())
			initShader(*e.get<GBufferShaderComponent>().shader);

		if (e.has<LightingShaderComponent>())
			initShader(*e.get<LightingShaderComponent>().shader);

		if (e.has<PostLightingShaderComponent>())
			initShader(*e.get<PostLightingShaderComponent>().shader);

		if (e.has<PostProcessShaderComponent>())
			initShader(*e.get<PostProcessShaderComponent>().shader);
	}

	static void initGBuffer(size_t nbAttributes, const functions::GBufferAttributeIterator & iterator) {
		g_gBufferTextureCount = nbAttributes;
		g_gBufferIterator = iterator;

		for (const auto & [e, shader] : g_em->getEntities<GBufferShaderComponent>())
			initShader(*shader.shader);
		for (const auto & [e, shader] : g_em->getEntities<LightingShaderComponent>())
			initShader(*shader.shader);
		for (const auto & [e, shader] : g_em->getEntities<PostLightingShaderComponent>())
			initShader(*shader.shader);
		for (const auto & [e, shader] : g_em->getEntities<PostProcessShaderComponent>())
			initShader(*shader.shader);

		for (const auto & [e, modelInfo] : g_em->getEntities<OpenGLModelComponent>())
			for (const auto & meshInfo : modelInfo.meshes) {
				glBindBuffer(GL_ARRAY_BUFFER, meshInfo.vertexBuffer);
				modelInfo.vertexRegisterFunc();
			}
	}

	static void onEntityRemoved(Entity & e) {
		if (!e.has<WindowComponent>() || e.id != g_window.id)
			return;
		g_window.id = Entity::INVALID_ID;
		g_window.comp = nullptr;
		glfwDestroyWindow(g_window.window);
	}

	static void onMouseCaptured(Entity::ID window, bool captured) {
		if (window != Entity::INVALID_ID && window != g_window.id)
			return;

		if (captured) {
			glfwSetInputMode(g_window.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
		}
		else {
			glfwSetInputMode(g_window.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		}
	}

	static void initShader(putils::gl::Program & p) {
		p.init(g_gBufferTextureCount);

		assert(g_gBufferIterator != nullptr);
		int texture = 0;
		g_gBufferIterator([&](const char * name) {
			p.addGBufferTexture(name, texture++);
		});
	}

	static void createObject(Entity & e, const ModelDataComponent & modelData) {
		auto & modelInfo = e.attach<OpenGLModelComponent>();
		modelInfo.meshes.clear();
		modelInfo.vertexRegisterFunc = modelData.vertexRegisterFunc;

		for (const auto & meshData : modelData.meshes) {
			OpenGLModelComponent::Mesh meshInfo;
			glGenVertexArrays(1, &meshInfo.vertexArrayObject);
			glBindVertexArray(meshInfo.vertexArrayObject);

			glGenBuffers(1, &meshInfo.vertexBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, meshInfo.vertexBuffer);
			glBufferData(GL_ARRAY_BUFFER, meshData.vertices.nbElements * meshData.vertices.elementSize, meshData.vertices.data, GL_STATIC_DRAW);

			glGenBuffers(1, &meshInfo.indexBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshInfo.indexBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.nbElements * meshData.indices.elementSize, meshData.indices.data, GL_STATIC_DRAW);

			modelData.vertexRegisterFunc();

			meshInfo.nbIndices = meshData.indices.nbElements;
			meshInfo.indexType = meshData.indexType;

			modelInfo.meshes.push_back(meshInfo);
		}

		modelData.free();
		e.detach<ModelDataComponent>();
	}

	static void loadTexture(Entity & e, TextureDataComponent & textureData) {
		if (*textureData.textureID == -1)
			glGenTextures(1, textureData.textureID);

		if (textureData.data != nullptr) {
			GLenum format;

			switch (textureData.components) {
			case 1:
				format = GL_RED;
				break;
			case 3:
				format = GL_RGB;
				break;
			case 4:
				format = GL_RGBA;
				break;
			default:
				assert(false);
			}

			glBindTexture(GL_TEXTURE_2D, *textureData.textureID);
			glTexImage2D(GL_TEXTURE_2D, 0, format, textureData.width, textureData.height, 0, format, GL_UNSIGNED_BYTE, textureData.data);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (textureData.free != nullptr)
				textureData.free(textureData.data);
		}

		e.detach<TextureDataComponent>();
	}

	// declarations
	static void updateWindowProperties();
	static void doOpenGL();
	static void doImGui();
	//
	static void execute(float deltaTime) {
		static bool first = true;
		if (first) {
			init();

#ifndef KENGINE_NO_DEFAULT_GBUFFER
			initGBuffer<GBufferTextures>(*g_em);
#endif
			first = false;
		}

		if (g_window.id == Entity::INVALID_ID)
			return;

		glfwPollEvents();
		updateWindowProperties();

		for (auto &[e, modelData] : g_em->getEntities<ModelDataComponent>()) {
			createObject(e, modelData);
			if (e.componentMask == 0)
				g_em->removeEntity(e);
		}

		for (auto &[e, textureLoader] : g_em->getEntities<TextureDataComponent>()) {
			loadTexture(e, textureLoader);
			if (e.componentMask == 0)
				g_em->removeEntity(e);
		}

		doOpenGL();
		doImGui();
		glfwSwapBuffers(g_window.window);
	}

	static void updateWindowProperties() {
		if (glfwGetWindowAttrib(g_window.window, GLFW_ICONIFIED)) {
			glfwSwapBuffers(g_window.window);
			return;
		}

		if (glfwWindowShouldClose(g_window.window)) {
			if (g_window.comp->shutdownOnClose)
				g_em->running = false;
			g_em->getEntity(g_window.id).detach<WindowComponent>();
			g_window.id = Entity::INVALID_ID;
			g_window.comp = nullptr;
			return;
		}
		
		if (g_window.comp->name != g_window.name) {
			g_window.name = g_window.comp->name;
			glfwSetWindowTitle(g_window.window, g_window.comp->name);
		}

		if (g_window.comp->size != g_window.size) {
			g_window.size = g_window.comp->size;
			glfwSetWindowSize(g_window.window, g_window.size.x, g_window.size.y);
			glfwGetWindowSize(g_window.window, &g_window.size.x, &g_window.size.y);
			g_window.comp->size = g_window.size;
			glfwSetWindowAspectRatio(g_window.window, g_window.size.x, g_window.size.y);
		}

		if (g_window.comp->fullscreen != g_window.fullScreen) {
			g_window.fullScreen = g_window.comp->fullscreen;

			const auto monitor = glfwGetPrimaryMonitor();
			const auto mode = glfwGetVideoMode(monitor);

			if (g_window.comp->fullscreen)
				glfwSetWindowMonitor(g_window.window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			else
				glfwSetWindowMonitor(g_window.window, nullptr, 0, 0, g_window.size.x, g_window.size.y, mode->refreshRate);

			glfwGetWindowSize(g_window.window, &g_window.size.x, &g_window.size.y);
			g_window.comp->size = g_window.size;
			glfwSetWindowAspectRatio(g_window.window, g_window.size.x, g_window.size.y);
		}
	}

	struct CameraFramebufferComponent {
		GLuint fbo = -1;
		GLuint depthTexture = -1;
		putils::Point2i resolution;
	};

	// declarations
	static void setupParams(const CameraComponent & cam, const ViewportComponent & viewport);
	static void initFramebuffer(Entity & e);
	static void fillGBuffer(EntityManager & em, Entity & e, const ViewportComponent & viewport) noexcept;
	static void renderToTexture(EntityManager & em, const CameraFramebufferComponent & fb) noexcept;
	static void blitTextureToViewport(const CameraFramebufferComponent & fb, const ViewportComponent & viewport);
	//
	static void doOpenGL() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		struct ToBlit {
			const CameraFramebufferComponent * fb;
			const ViewportComponent * viewport;
		};
		putils::vector<ToBlit, KENGINE_MAX_VIEWPORTS> toBlit;

		for (auto &[e, cam, viewport] : g_em->getEntities<CameraComponent, ViewportComponent>()) {
			if (viewport.window == Entity::INVALID_ID)
				viewport.window = g_window.id;
			else if (viewport.window != g_window.id)
				return;

			setupParams(cam, viewport);
			fillGBuffer(*g_em, e, viewport);

			if (!e.has<CameraFramebufferComponent>() || e.get<CameraFramebufferComponent>().resolution != viewport.resolution)
				initFramebuffer(e);
			auto & fb = e.get<CameraFramebufferComponent>();

			renderToTexture(*g_em, fb);
			if (viewport.boundingBox.size.x > 0 && viewport.boundingBox.size.y > 0)
				toBlit.push_back(ToBlit{ &fb, &viewport });
		}

		std::sort(toBlit.begin(), toBlit.end(), [](const ToBlit & lhs, const ToBlit & rhs) {
			return lhs.viewport->zOrder < rhs.viewport->zOrder;
		});

		for (const auto & blit : toBlit)
			blitTextureToViewport(*blit.fb, *blit.viewport);
	}

	static void setupParams(const CameraComponent & cam, const ViewportComponent & viewport) {
		g_params.viewPort.size = viewport.resolution;
		putils::gl::setViewPort(g_params.viewPort);

		g_params.camPos = ShaderHelper::toVec(cam.frustum.position);
		g_params.camFOV = cam.frustum.size.y;

		g_params.view = [&] {
			const auto front = glm::normalize(glm::vec3{
				std::sin(cam.yaw) * std::cos(cam.pitch),
				std::sin(cam.pitch),
				std::cos(cam.yaw) * std::cos(cam.pitch)
				});
			const auto right = glm::normalize(glm::cross(front, { 0.f, 1.f, 0.f }));
			auto up = glm::normalize(glm::cross(right, front));
			detail::rotate(up, front, cam.roll);

			return glm::lookAt(g_params.camPos, g_params.camPos + front, up);
		}();

		g_params.proj = glm::perspective(
			g_params.camFOV,
			(float)g_params.viewPort.size.x / (float)g_params.viewPort.size.y,
			g_params.nearPlane, g_params.farPlane
		);
	}

	static void initFramebuffer(Entity & e) {
		auto & viewport = e.get<ViewportComponent>();
		if (viewport.resolution.x == 0 || viewport.resolution.y == 0)
			return;

		auto & fb = e.attach<CameraFramebufferComponent>();
		fb.resolution = viewport.resolution;

		if (fb.fbo == -1)
			glGenFramebuffers(1, &fb.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

		GLuint texture = (GLuint)viewport.renderTexture;
		if (viewport.renderTexture == (ViewportComponent::RenderTexture)-1)
			glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, viewport.resolution.x, viewport.resolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

		if (fb.depthTexture == -1)
			glGenTextures(1, &fb.depthTexture);
		glBindTexture(GL_TEXTURE_2D, fb.depthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, viewport.resolution.x, viewport.resolution.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb.depthTexture, 0);

		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		viewport.renderTexture = (ViewportComponent::RenderTexture)texture;
	}

	template<typename Shaders>
	static void runShaders(Shaders && shaders) {
		for (auto & [e, comp] : shaders)
			if (comp.enabled) {
#ifndef KENGINE_NDEBUG
				struct ShaderProfiler {
					ShaderProfiler(Entity & e) {
						_comp = &e.attach<Controllers::ShaderProfileComponent>();
						_timer.restart();
					}

					~ShaderProfiler() {
						_comp->executionTime = _timer.getTimeSinceStart().count();
					}

					Controllers::ShaderProfileComponent * _comp;
					putils::Timer _timer;
				};
				ShaderProfiler _(e);
#endif
				comp.shader->run(g_params);
			}
	}

	static void fillGBuffer(EntityManager & em, Entity & e, const ViewportComponent & viewport) noexcept {
		if (!e.has<GBufferComponent>()) {
			auto & gbuffer = e.attach<GBufferComponent>();
			gbuffer.init(viewport.resolution.x, viewport.resolution.y, g_gBufferTextureCount);
		}
		auto & gbuffer = e.get<GBufferComponent>();
		if (gbuffer.getSize() != viewport.resolution)
			gbuffer.resize(viewport.resolution.x, viewport.resolution.y);

		gbuffer.bindForWriting();
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			ShaderHelper::Enable depth(GL_DEPTH_TEST);
			runShaders(em.getEntities<GBufferShaderComponent>());
		}
		gbuffer.bindForReading();
	}

	static void renderToTexture(EntityManager & em, const CameraFramebufferComponent & fb) noexcept {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.fbo);
		glClear(GL_COLOR_BUFFER_BIT);

		glBlitFramebuffer(0, 0, (GLint)g_params.viewPort.size.x, (GLint)g_params.viewPort.size.y, 0, 0, (GLint)g_params.viewPort.size.x, (GLint)g_params.viewPort.size.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		runShaders(em.getEntities<LightingShaderComponent>());
		runShaders(em.getEntities<PostLightingShaderComponent>());
		runShaders(em.getEntities<PostProcessShaderComponent>());
	}

	static void blitTextureToViewport(const CameraFramebufferComponent & fb, const ViewportComponent & viewport) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		const auto destSizeX = (GLint)(viewport.boundingBox.size.x * g_window.size.x);
		const auto destSizeY = (GLint)(viewport.boundingBox.size.y * g_window.size.y);

		const auto destX = (GLint)(viewport.boundingBox.position.x * g_window.size.x);
		// OpenGL coords have Y=0 at the bottom, I want Y=0 at the top
		const auto destY = (GLint)(g_window.size.y - destSizeY - viewport.boundingBox.position.y * g_window.size.y);

		glBlitFramebuffer(
			// src
			0, 0, fb.resolution.x, fb.resolution.y,
			// dest
			destX, destY, destX + destSizeX, destY + destSizeY,
			GL_COLOR_BUFFER_BIT, GL_LINEAR
		);
	}

	static void doImGui() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		for (const auto &[e, comp] : g_em->getEntities<ImGuiComponent>())
			comp.display(GImGui);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	static Entity::ID getEntityInPixel(Entity::ID window, const putils::Point2ui & pixel) {
		static constexpr auto GBUFFER_TEXTURE_COMPONENTS = 4;
		static constexpr auto GBUFFER_ENTITY_LOCATION = 3;

		if (window != Entity::INVALID_ID && window != g_window.id)
			return Entity::INVALID_ID;

		const auto viewportInfo = CameraHelper::getViewportForPixel(*g_em, g_window.id, pixel);
		if (viewportInfo.camera == Entity::INVALID_ID)
			return Entity::INVALID_ID;

		auto & camera = g_em->getEntity(viewportInfo.camera);
		if (!camera.has<GBufferComponent>())
			return Entity::INVALID_ID;

		auto & gbuffer = camera.get<GBufferComponent>();

		const putils::Point2ui gBufferSize = gbuffer.getSize();
		const auto pixelInGBuffer = putils::Point2i(viewportInfo.pixel * gBufferSize);
		if (pixelInGBuffer.x >= gBufferSize.x || pixelInGBuffer.y > gBufferSize.y || pixelInGBuffer.y == 0)
			return Entity::INVALID_ID;

		const auto index = (pixelInGBuffer.x + (gBufferSize.y - pixelInGBuffer.y) * gBufferSize.x) * GBUFFER_TEXTURE_COMPONENTS;
		Entity::ID ret;
		{ // Release texture asap
			const auto texture = gbuffer.getTexture(GBUFFER_ENTITY_LOCATION);
			ret = (Entity::ID)texture.data[index];
		}
		if (ret == 0)
			ret = Entity::INVALID_ID;
		return ret;
	}
}
