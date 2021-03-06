#pragma once
#include <NovusTypes.h>

#include <Renderer/Descriptors/ImageDesc.h>
#include <Renderer/Descriptors/DepthImageDesc.h>
#include <Renderer/Descriptors/TextureDesc.h>
#include <Renderer/Descriptors/ModelDesc.h>
#include <Renderer/Descriptors/SamplerDesc.h>
#include <Renderer/ConstantBuffer.h>
#include <Renderer/InstanceData.h>

namespace Renderer
{
    class Renderer;
}

namespace Memory
{
    class StackAllocator;
}

struct ViewConstantBuffer
{
    mat4x4 viewMatrix; // 64 bytes
    mat4x4 projMatrix; // 64 bytes

    u8 padding[128] = {};
};

struct ModelConstantBuffer
{
    vec4 colorMultiplier; // 16 bytes
    mat4x4 modelMatrix; // 64 bytes

    u8 padding[176] = {};
};

class Window;
class Camera;
class UIRenderer;
class InputManager;

class ClientRenderer
{
public:
    ClientRenderer();

    bool UpdateWindow(f32 deltaTime);
    void Update(f32 deltaTime);
    void Render();

private:
    void CreatePermanentResources();

private:
    Window* _window;
    Camera* _camera;
    InputManager* _inputManager;
    Renderer::Renderer* _renderer;
    Memory::StackAllocator* _frameAllocator;

    u8 _frameIndex = 0;

    // Permanent resources
    Renderer::ImageID _mainColor;
    Renderer::DepthImageID _mainDepth;

    Renderer::ModelID _cubeModel;
    Renderer::TextureID _cubeTexture;
    Renderer::InstanceData _cubeModelInstance;
    Renderer::SamplerID _linearSampler;

    Renderer::ConstantBuffer<ViewConstantBuffer>* _viewConstantBuffer;
    Renderer::ConstantBuffer<ModelConstantBuffer>* _modelConstantBuffer;

    // Sub renderers
    UIRenderer* _uiRenderer;
};