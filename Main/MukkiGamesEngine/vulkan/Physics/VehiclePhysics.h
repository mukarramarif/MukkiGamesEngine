#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleController.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include <glm/glm.hpp>
#include <memory>

class PhysicsEngine;

struct VehicleConfig {
	float maxTorque = 500.0f;
	float maxRPM = 6000.0f;
	float clutchStrength = 10.0f;
	std::vector<float> gearRatios = { 2.66f, 1.78f, 1.30f, 1.0f, 0.74f, 0.50f };
	float reverseGearRatio = -2.90f;
	float differentialRatio = 3.42f;

	struct Wheel {
		glm::vec3 position;
		float radius = 0.3f;
		float width = 0.2f;
		float suspensionLength = 0.3f;
		float suspensionStiffness = 200.0f;
		float suspensionDamping = 20.0f;
		float suspensionMaxForce = 3000.0f;
		bool isFront = true;
	};
	std::vector<Wheel> wheels;
};

class VehiclePhysics {
public:
	VehiclePhysics();
	~VehiclePhysics();

	void init(PhysicsEngine* engine, JPH::Body* chassisBody, const VehicleConfig& config);
	void shutdown();

	void setInput(float throttle, float brake, float steering, bool handbrake = false);
	float getSpeed() const;
	float getRPM() const;
	int getCurrentGear() const;

	JPH::VehicleConstraint* getConstraint() { return constraint; }

private:
	PhysicsEngine* engine = nullptr;
	JPH::VehicleConstraint* constraint = nullptr;
	bool initialized = false;
};
