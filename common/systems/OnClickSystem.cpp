#include "OnClickSystem.hpp"
#include "EntityManager.hpp"

#include "components/OnClickComponent.hpp"
#include "components/InputComponent.hpp"

#include "packets/EntityInPixel.hpp"

namespace kengine {
	OnClickSystem::OnClickSystem(EntityManager & em)
		: System(em), _em(em)
	{
		onLoad("");
	}

	void OnClickSystem::onLoad(const char *) noexcept {
		_em += [this](Entity & e) {
			InputComponent input;
			input.onMouseButton = [this](int button, const putils::Point2f & coords, bool pressed) {
				if (!pressed)
					return;

				Entity::ID id = Entity::INVALID_ID;
				send(packets::GetEntityInPixel{ coords, id });

				if (id == Entity::INVALID_ID)
					return;

				auto & e = _em.getEntity(id);
				if (e.has<OnClickComponent>())
					e.get<OnClickComponent>().onClick();
			};
			e += input;
		};
	}
}
