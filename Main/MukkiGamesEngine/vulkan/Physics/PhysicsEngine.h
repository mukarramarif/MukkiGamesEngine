#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include "../../objects/vertex.h"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterMask.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include "PhysicsDebugRenderer.h"

namespace PhysicsLayers {
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

class PhysicsEngine {
public:
	PhysicsEngine();
	~PhysicsEngine();

	void init();
	void step(float deltaTime);
	void shutdown();

	JPH::Body* createRigidBody(
		const glm::vec3& position,
		const glm::vec3& rotation,
		JPH::ShapeRefC shape,
		float mass,
		bool isDynamic);

	JPH::Body* createStaticBody(
		const glm::vec3& position,
		const glm::vec3& rotation,
		JPH::ShapeRefC shape);

	JPH::Body* createMeshShapeFromVertices(
		const glm::vec3& position,
		const glm::vec3& rotation,
		const glm::vec3& scale,
		const Vertex* vertices,
		int vertexCount,
		const uint32_t* indices,
		int indexCount,
		bool isDynamic);

	void removeBody(uint32_t bodyIndex);

	glm::vec3 getBodyPosition(uint32_t bodyIndex) const;
	glm::quat getBodyRotation(uint32_t bodyIndex) const;
	void setBodyPosition(uint32_t bodyIndex, const glm::vec3& pos);

	JPH::BodyInterface& getBodyInterface();
	JPH::PhysicsSystem* getSystem() { return physicsSystem.get(); }

	void drawDebug();
	const std::vector<DebugLineVertex>& getDebugLines() const;
	void clearDebugLines();

private:
	JPH::TempAllocator* tempAllocator = nullptr;
	JPH::JobSystemThreadPool* jobSystem = nullptr;
	std::unique_ptr<JPH::PhysicsSystem> physicsSystem;
	std::unique_ptr<JPH::BroadPhaseLayerInterface> broadPhaseLayerInterface;
	std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> objectVsBroadPhaseLayerFilter;
	std::unique_ptr<JPH::ObjectLayerPairFilter> objectLayerPairFilter;
	std::unique_ptr<PhysicsDebugRenderer> debugRenderer;
	bool initialized = false;
};
