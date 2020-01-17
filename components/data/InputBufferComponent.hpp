#pragma once

#ifndef KENGINE_INPUT_MAX_BUFFERED_EVENTS
# define KENGINE_INPUT_MAX_BUFFERED_EVENTS 128
#endif

#include "vector.hpp"

namespace kengine {
	struct InputBufferComponent {
		template<typename T>
		using EventVector = putils::vector<T, KENGINE_INPUT_MAX_BUFFERED_EVENTS>;

		struct KeyEvent {
			Entity::ID window;
			int key;
			bool pressed;
		};
		EventVector<KeyEvent> keys;

		struct ClickEvent {
			Entity::ID window;
			putils::Point2f pos;
			int button;
			bool pressed;
		};
		EventVector<ClickEvent> clicks;

		struct MouseMoveEvent {
			Entity::ID window;
			putils::Point2f pos;
			putils::Point2f rel;
		};
		EventVector<MouseMoveEvent> moves;

		struct MouseScrollEvent {
			Entity::ID window;
			float xoffset;
			float yoffset;
			putils::Point2f pos;
		};
		EventVector<MouseScrollEvent> scrolls;

		putils_reflection_class_name(InputBufferComponent);
	};
}