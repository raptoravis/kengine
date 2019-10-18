#include "CameraManager.hpp"
#include "EntityManager.hpp"

#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreSceneNode.h>
#include <OgreRenderWindow.h>

#include "components/CameraComponent.hpp"
#include "components/TransformComponent.hpp"

#include "Utils.hpp"

struct OgreCameraComponent {
	Ogre::SceneNode * node;
	Ogre::Camera * camera;
	Ogre::Viewport * viewPort;
};

static void setTransform(Ogre::SceneNode & node, const kengine::CameraComponent3f & transform) {
	node.setPosition(convert(transform.frustrum.position));
	node.yaw(Ogre::Radian(transform.yaw));
	node.pitch(Ogre::Radian(transform.pitch));
	node.roll(Ogre::Radian(transform.roll));
}

CameraManager::CameraManager(kengine::EntityManager & em, Ogre::SceneManager & sceneManager, Ogre::RenderWindow & window)
	: _em(em), _sceneManager(sceneManager), _window(window)
{
}

void CameraManager::registerEntity(kengine::Entity & e) noexcept {
	if (!e.has<kengine::CameraComponent3f>())
		return;

	const auto & transform = e.get<kengine::TransformComponent3f>();
	const auto & cam = e.get<kengine::CameraComponent3f>();

	OgreCameraComponent comp;
	comp.camera = _sceneManager.createCamera(Ogre::StringConverter::toString(e.id));

	comp.node = _sceneManager.getRootSceneNode()->createChildSceneNode();
	comp.node->attachObject(comp.camera);
	setTransform(*comp.node, cam);

	const auto & viewPortPos = transform.boundingBox.position;
	const auto & viewPortSize = transform.boundingBox.size;
	comp.viewPort = _window.addViewport(comp.camera, (int)viewPortPos.z, viewPortPos.x, viewPortPos.y, viewPortSize.x, viewPortSize.y);

	e += comp;
}

void CameraManager::removeEntity(kengine::Entity & e) noexcept {
	if (!e.has<OgreCameraComponent>())
		return;

	const auto & transform = e.get<kengine::TransformComponent3f>();
	_window.removeViewport((int)transform.boundingBox.position.z);

	const auto & comp = e.get<OgreCameraComponent>();
	_sceneManager.getRootSceneNode()->removeChild(comp.node);
	_sceneManager.destroyCamera(Ogre::StringConverter::toString(e.id));
}
