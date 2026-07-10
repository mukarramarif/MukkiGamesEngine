#include "PhysicsDebugRenderer.h"

JPH_SUPPRESS_WARNINGS

PhysicsDebugRenderer::PhysicsDebugRenderer()
{
	JPH::DebugRenderer::sInstance = this;
}

void PhysicsDebugRenderer::ClearLines()
{
	mLineVertices.clear();
}

void PhysicsDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
	DebugLineVertex v;
	v.color = glm::vec3(inColor.r / 255.0f, inColor.g / 255.0f, inColor.b / 255.0f);
	v.pos = glm::vec3((float)inFrom.GetX(), (float)inFrom.GetY(), (float)inFrom.GetZ());
	mLineVertices.push_back(v);
	v.pos = glm::vec3((float)inTo.GetX(), (float)inTo.GetY(), (float)inTo.GetZ());
	mLineVertices.push_back(v);
}

void PhysicsDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight)
{
}
