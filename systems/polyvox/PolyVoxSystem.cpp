#include "PolyVoxSystem.hpp"

#include <PolyVox/CubicSurfaceExtractor.h>

#include "EntityManager.hpp"
#include "PolyVoxShader.hpp"
#include "data/ShaderComponent.hpp"
#include "data/ModelDataComponent.hpp"
#include "data/PolyVoxComponent.hpp"
#include "data/ModelComponent.hpp"

#include "functions/Execute.hpp"

namespace kengine {
	static EntityManager * g_em;

	// declarations
	static void execute(float deltaTime);
	//
	EntityCreator * PolyVoxSystem(EntityManager & em) {
		g_em = &em;

		return [](Entity & e) {
			e += functions::Execute { execute };
			e += makeGBufferShaderComponent<PolyVoxShader>(*g_em);
		};
	}

	namespace detailPolyVox {
		static auto buildMesh(PolyVox::RawVolume<PolyVoxComponent::VertexData> & volume) {
			const auto encodedMesh = PolyVox::extractCubicMesh(&volume, volume.getEnclosingRegion());
			const auto mesh = PolyVox::decodeMesh(encodedMesh);
			return mesh;
		}
	}

	struct PolyVoxMeshContainerComponent {
		using MeshType = decltype(detailPolyVox::buildMesh(PolyVox::RawVolume<PolyVoxComponent::VertexData>({ {0, 0, 0 }, {0, 0, 0} })));
		MeshType mesh;
	};

	static auto FreePolyVoxMeshData(Entity::ID id, EntityManager & em) {
		return [id, &em] {
			auto & e = em.getEntity(id);
			auto & mesh = e.attach<PolyVoxMeshContainerComponent>().mesh; // previous `attach` hasn't been processed yet, so `get` would assert
			mesh.clear();
			e.detach<PolyVoxMeshContainerComponent>();
		};
	}

	static void execute(float deltaTime) {
		for (auto &[e, poly] : g_em->getEntities<PolyVoxComponent>()) {
			if (!poly.changed)
				continue;
			poly.changed = false;

			ModelDataComponent modelData;

			auto & mesh = e.attach<PolyVoxMeshContainerComponent>().mesh;
			mesh = detailPolyVox::buildMesh(poly.volume);

			const auto & centre = poly.volume.getEnclosingRegion().getCentre();
			auto & model = e.attach<ModelComponent>();
			model.boundingBox.position = { (float)centre.getX(), (float)centre.getY(), (float)centre.getZ() };

			ModelDataComponent::Mesh meshData;
			meshData.vertices = { mesh.getNoOfVertices(), sizeof(PolyVoxMeshContainerComponent::MeshType::VertexType), mesh.getRawVertexData() };
			meshData.indices = { mesh.getNoOfIndices(), sizeof(PolyVoxMeshContainerComponent::MeshType::IndexType), mesh.getRawIndexData() };
			meshData.indexType = sizeof(PolyVoxMeshContainerComponent::MeshType::IndexType) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
			modelData.meshes.push_back(meshData);

			modelData.free = FreePolyVoxMeshData(e.id, *g_em);
			modelData.vertexRegisterFunc =putils::gl::setPolyvoxVertexType<PolyVoxMeshContainerComponent::MeshType::VertexType>; 

			e += std::move(modelData);
		}
	}
}
