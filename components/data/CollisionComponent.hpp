#pragma once

#ifndef KENGINE_COLLISION_FUNCTION_SIZE
# define KENGINE_COLLISION_FUNCTION_SIZE 64
#endif

#include "function.hpp"
#include "reflection.hpp"

namespace kengine {
	class Entity;

	struct CollisionComponent {
		using function = putils::function<void(Entity &, Entity &), KENGINE_COLLISION_FUNCTION_SIZE>;
		function onCollide = nullptr;

		putils_reflection_class_name(CollisionComponent);
		putils_reflection_attributes(
			putils_reflection_attribute(&CollisionComponent::onCollide)
		);
	};
}
