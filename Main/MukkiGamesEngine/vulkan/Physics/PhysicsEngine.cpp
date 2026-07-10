#include "PhysicsEngine.h"
#include <iostream>
#include <cstdarg>
#include <Jolt/Core/Core.h>

JPH_SUPPRESS_WARNINGS

static void TraceImpl(const char* inFMT, ...) {
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
	std::cout << "Jolt: " << buffer << std::endl;
}

#ifdef _DEBUG
static bool AssertFailedImpl(const char* expr, const char* msg, const char* file, uint32_t line) {
	std::cerr << "Jolt Assert: " << expr << " " << (msg ? msg : "") << " " << file << ":" << line << std::endl;
	return true;
}
#endif

// Layer filter: Non-moving only collides with moving, moving collides with everything
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
		switch (inObject1) {
		case PhysicsLayers::NON_MOVING:
			return inObject2 == PhysicsLayers::MOVING;
		case PhysicsLayers::MOVING:
			return true;
		default:
			return false;
		}
	}
};

// BroadPhase layers
class BroadPhaseLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
public:
	BroadPhaseLayerInterfaceImpl() {
		mObjectToBroadPhase[PhysicsLayers::NON_MOVING] = JPH::BroadPhaseLayer(0);
		mObjectToBroadPhase[PhysicsLayers::MOVING] = JPH::BroadPhaseLayer(1);
	}

	virtual JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
		return mObjectToBroadPhase[inLayer];
	}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
		return inLayer == JPH::BroadPhaseLayer(0) ? "NON_MOVING" : "MOVING";
	}
#endif

private:
	JPH::BroadPhaseLayer mObjectToBroadPhase[PhysicsLayers::NUM_LAYERS];
};

// BroadPhase filter
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
		return true;
	}
};

PhysicsEngine::PhysicsEngine() {}
PhysicsEngine::~PhysicsEngine() { shutdown(); }

void PhysicsEngine::init()
{
	JPH::RegisterDefaultAllocator();
	JPH::Trace = TraceImpl;
#ifdef _DEBUG
	JPH::AssertFailed = AssertFailedImpl;
#endif
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	tempAllocator = new JPH::TempAllocatorImpl(32 * 1024 * 1024);
	jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	broadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
	objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
	objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

	physicsSystem = std::make_unique<JPH::PhysicsSystem>();
	debugRenderer = std::make_unique<PhysicsDebugRenderer>();
	physicsSystem->Init(
		65536, 0, 65536, 1024,
		*broadPhaseLayerInterface,
		*objectVsBroadPhaseLayerFilter,
		*objectLayerPairFilter);

	initialized = true;
	std::cout << "PhysicsEngine initialized" << std::endl;
}

void PhysicsEngine::step(float deltaTime)
{
	if (!initialized) return;
	float clampedDt = std::min(deltaTime, 1.0f / 30.0f);
	const int cCollisionSteps = 1;
	physicsSystem->Update(clampedDt, cCollisionSteps, tempAllocator, jobSystem);
}

void PhysicsEngine::shutdown()
{
	if (!initialized) return;
	physicsSystem.reset();
	broadPhaseLayerInterface.reset();
	objectVsBroadPhaseLayerFilter.reset();
	objectLayerPairFilter.reset();
	delete jobSystem;
	jobSystem = nullptr;
	delete tempAllocator;
	tempAllocator = nullptr;
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
	initialized = false;
}

JPH::Body* PhysicsEngine::createRigidBody(
	const glm::vec3& position,
	const glm::vec3& rotation,
	JPH::ShapeRefC shape,
	float mass,
	bool isDynamic)
{
	if (!initialized || shape == nullptr) return nullptr;

	JPH::EMotionType motionType = isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
	JPH::ObjectLayer layer = isDynamic ? PhysicsLayers::MOVING : PhysicsLayers::NON_MOVING;

	glm::quat q = glm::quat(glm::radians(rotation));
	JPH::Quat joltRot(q.x, q.y, q.z, q.w);
	JPH::Vec3 joltPos(position.x, position.y, position.z);

	JPH::BodyCreationSettings settings(shape, joltPos, joltRot, motionType, layer);
	if (isDynamic) {
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
		settings.mMassPropertiesOverride.mMass = mass;
	}

	JPH::Body* body = physicsSystem->GetBodyInterface().CreateBody(settings);
	if (body) {
		physicsSystem->GetBodyInterface().AddBody(body->GetID(), JPH::EActivation::Activate);
	}
	return body;
}

JPH::Body* PhysicsEngine::createStaticBody(
	const glm::vec3& position,
	const glm::vec3& rotation,
	JPH::ShapeRefC shape)
{
	return createRigidBody(position, rotation, shape, 0.0f, false);
}

JPH::Body* PhysicsEngine::createMeshShapeFromVertices(
	const glm::vec3& position,
	const glm::vec3& rotation,
	const glm::vec3& scale,
	const Vertex* vertices,
	int vertexCount,
	const uint32_t* indices,
	int indexCount,
	bool isDynamic)
{
	if (!initialized || vertexCount < 3 || indexCount < 3) return nullptr;

	JPH::VertexList vertexList;
	vertexList.reserve(vertexCount);
	for (int i = 0; i < vertexCount; i++) {
		vertexList.push_back(JPH::Float3(
			vertices[i].pos.x * scale.x,
			vertices[i].pos.y * scale.y,
			vertices[i].pos.z * scale.z));
	}

	JPH::IndexedTriangleList triangleList;
	triangleList.reserve(indexCount / 3);
	for (int i = 0; i < indexCount; i += 3) {
		triangleList.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2], 0));
	}

	JPH::ShapeRefC meshShape = JPH::MeshShapeSettings(vertexList, triangleList).Create().Get();
	if (meshShape == nullptr) return nullptr;

	return createRigidBody(position, rotation, meshShape, 0.0f, false);
}

void PhysicsEngine::removeBody(uint32_t bodyIndex)
{
	if (!initialized) return;
	if (bodyIndex == 0xFFFFFFFF) return;
	JPH::BodyID id(bodyIndex);
	physicsSystem->GetBodyInterface().RemoveBody(id);
	physicsSystem->GetBodyInterface().DestroyBody(id);
}

glm::vec3 PhysicsEngine::getBodyPosition(uint32_t bodyIndex) const
{
	if (!initialized || bodyIndex == 0xFFFFFFFF) return glm::vec3(0.0f);
	JPH::BodyID id(bodyIndex);
	JPH::Vec3 pos = physicsSystem->GetBodyInterface().GetPosition(id);
	return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

glm::quat PhysicsEngine::getBodyRotation(uint32_t bodyIndex) const
{
	if (!initialized || bodyIndex == 0xFFFFFFFF) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	JPH::BodyID id(bodyIndex);
	JPH::Quat rot = physicsSystem->GetBodyInterface().GetRotation(id);
	return glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
}

void PhysicsEngine::setBodyPosition(uint32_t bodyIndex, const glm::vec3& pos)
{
	if (!initialized || bodyIndex == 0xFFFFFFFF) return;
	JPH::BodyID id(bodyIndex);
	physicsSystem->GetBodyInterface().SetPosition(id, JPH::Vec3(pos.x, pos.y, pos.z), JPH::EActivation::Activate);
}

void PhysicsEngine::drawDebug()
{
	if (!initialized || !debugRenderer) return;
	debugRenderer->ClearLines();
	JPH::BodyManager::DrawSettings settings;
	settings.mDrawShape = true;
	settings.mDrawShapeWireframe = true;
	physicsSystem->DrawBodies(settings, debugRenderer.get());
}

const std::vector<DebugLineVertex>& PhysicsEngine::getDebugLines() const
{
	static std::vector<DebugLineVertex> empty;
	return debugRenderer ? debugRenderer->GetLines() : empty;
}

void PhysicsEngine::clearDebugLines()
{
	if (debugRenderer) debugRenderer->ClearLines();
}

JPH::BodyInterface& PhysicsEngine::getBodyInterface()
{
	return physicsSystem->GetBodyInterface();
}
