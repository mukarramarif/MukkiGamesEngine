#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
//Forward declared for rendering
struct SceneObject;
struct Light;
class Camera;
class TextureManager;
class BufferManager;
struct Model;

struct RenderConfig {
    int windowWidgth= 800;
    int windowHeight=600;
    std::string windowTitle="Mukki Games Engine";
    std::string scenePath;
};


namespace MUKKI {
    class Renderer {
        public:
            virtual ~Renderer() = default;

            virtual void init(const RenderConfig& config) =0;
            virtual void shutdown()=0;

            virtual void run()=0;

            //Per Frame Calls
            virtual void beginFrame()=0;
            virtual void endFrame()=0;

            //Scene Objects

            virtual void addObject(uint32_t id, const ::Model& model, const glm::mat4& transform) =0;
            virtual void removeObject(uint32_t  id)=0;

            virtual void setLights(const std::vector<Light>& lights, float ambientStrength) =0;

            virtual void setCamera(const Camera& camera)=0;

            virtual void setSkybox(const std::string& skyBoxPath)=0;
            //Modes
            enum class Mode { GRAPHICS, RAYTRACING};

            virtual void setRenderMode(Mode mode) =0;

            virtual void getRenderMode() const =0;

            //Window
            virtual void* getNativeWindow() const =0;
            virtual void onResize(int width, int height)=0;

            //Frame Timing
            virtual float getFrameTime() const = 0;
            virtual uint32_t getFrameCount() const = 0;

            // UI rendering injection (called between begin/end)
            virtual void* getNativeCommandBuffer() = 0;

            // Native device handles (needed for resource creation)
            virtual void* getNativeDevice() = 0;
            virtual TextureManager* getTextureManager() = 0;
            virtual BufferManager* getBufferManager() = 0;



    };
};
