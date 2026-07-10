#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#include <glm/glm.hpp>
#include <vector>

struct DebugLineVertex {
	glm::vec3 pos;
	glm::vec3 color;
};

class PhysicsDebugRenderer : public JPH::DebugRendererSimple {
public:
	PhysicsDebugRenderer();
	void ClearLines();
	const std::vector<DebugLineVertex>& GetLines() const { return mLineVertices; }

protected:
	virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
	virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override;

private:
	std::vector<DebugLineVertex> mLineVertices;
};
