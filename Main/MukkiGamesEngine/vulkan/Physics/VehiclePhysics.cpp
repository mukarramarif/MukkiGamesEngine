#include "VehiclePhysics.h"
#include "PhysicsEngine.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <iostream>

JPH_SUPPRESS_WARNINGS

VehiclePhysics::VehiclePhysics() {}

VehiclePhysics::~VehiclePhysics()
{
	shutdown();
}

void VehiclePhysics::init(PhysicsEngine* engine, JPH::Body* chassisBody, const VehicleConfig& config)
{
	this->engine = engine;
	if (!engine || !chassisBody) return;

	JPH::VehicleConstraintSettings vehicleSettings;
	vehicleSettings.mUp = JPH::Vec3(0, 1, 0);
	vehicleSettings.mForward = JPH::Vec3(0, 0, 1);

	JPH::WheeledVehicleControllerSettings* controller = new JPH::WheeledVehicleControllerSettings;
	vehicleSettings.mController = controller;

	controller->mEngine.mMaxTorque = config.maxTorque;
	controller->mEngine.mMinRPM = 1000.0f;
	controller->mEngine.mMaxRPM = config.maxRPM;
	controller->mTransmission.mClutchStrength = config.clutchStrength;
	controller->mTransmission.mGearRatios.clear();
	for (auto r : config.gearRatios)
		controller->mTransmission.mGearRatios.push_back(r);
	controller->mTransmission.mReverseGearRatios.clear();
	controller->mTransmission.mReverseGearRatios.push_back(config.reverseGearRatio);
	controller->mTransmission.mSwitchTime = 0.1f;

	JPH::VehicleDifferentialSettings diffFront;
	diffFront.mLeftWheel = 0;
	diffFront.mRightWheel = 1;
	diffFront.mDifferentialRatio = config.differentialRatio;
	diffFront.mLeftRightSplit = 0.5f;
	diffFront.mEngineTorqueRatio = 0.5f;
	controller->mDifferentials.push_back(diffFront);

	JPH::VehicleDifferentialSettings diffRear;
	diffRear.mLeftWheel = 2;
	diffRear.mRightWheel = 3;
	diffRear.mDifferentialRatio = config.differentialRatio;
	diffRear.mLeftRightSplit = 0.5f;
	diffRear.mEngineTorqueRatio = 0.5f;
	controller->mDifferentials.push_back(diffRear);

	for (const auto& w : config.wheels) {
		JPH::WheelSettingsWV* wheelSettings = new JPH::WheelSettingsWV;
		wheelSettings->mPosition = JPH::Vec3(w.position.x, w.position.y, w.position.z);
		wheelSettings->mRadius = w.radius;
		wheelSettings->mWidth = w.width;
		wheelSettings->mSuspensionMinLength = 0.1f;
		wheelSettings->mSuspensionMaxLength = w.suspensionLength;
		wheelSettings->mSuspensionPreloadLength = 0.0f;
		wheelSettings->mSuspensionSpring = JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping, 1.5f, 0.5f);
		wheelSettings->mMaxSteerAngle = w.isFront ? JPH::DegreesToRadians(45.0f) : 0.0f;
		vehicleSettings.mWheels.push_back(wheelSettings);
	}

	constraint = new JPH::VehicleConstraint(*chassisBody, vehicleSettings);
	constraint->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterCastSphere(PhysicsLayers::NON_MOVING, 0.3f));
	engine->getSystem()->AddStepListener(constraint);
	engine->getSystem()->AddConstraint(constraint);

	initialized = true;
	std::cout << "Vehicle initialized with " << config.wheels.size() << " wheels" << std::endl;
}

void VehiclePhysics::shutdown()
{
	if (!initialized || !engine || !constraint) return;
	engine->getSystem()->RemoveConstraint(constraint);
	engine->getSystem()->RemoveStepListener(constraint);
	constraint = nullptr;
	initialized = false;
}

void VehiclePhysics::setInput(float throttle, float brake, float steering, bool handbrake)
{
	if (!initialized) return;

	auto* controller = static_cast<JPH::WheeledVehicleController*>(constraint->GetController());
	if (!controller) return;

	controller->SetDriverInput(throttle, steering, brake, handbrake ? 1.0f : 0.0f);
}

float VehiclePhysics::getSpeed() const
{
	if (!initialized || !constraint) return 0.0f;
	auto* body = constraint->GetVehicleBody();
	if (!body || !body->GetMotionProperties()) return 0.0f;
	return body->GetMotionProperties()->GetLinearVelocity().Length();
}

float VehiclePhysics::getRPM() const
{
	if (!initialized) return 0.0f;
	auto* controller = static_cast<JPH::WheeledVehicleController*>(constraint->GetController());
	return controller ? controller->GetEngine().GetCurrentRPM() : 0.0f;
}

int VehiclePhysics::getCurrentGear() const
{
	if (!initialized) return 0;
	auto* controller = static_cast<JPH::WheeledVehicleController*>(constraint->GetController());
	return controller ? controller->GetTransmission().GetCurrentGear() : 0;
}
