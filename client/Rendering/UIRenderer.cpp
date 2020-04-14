#include "UIRenderer.h"
#include "Camera.h"

#include "../Utils/ServiceLocator.h"
#include "../Scripting/Classes/UI/UIPanel.h"
#include "../Scripting/Classes/UI/UILabel.h"

#include <Renderer/Renderer.h>
#include <Renderer/Renderers/Vulkan/RendererVK.h>
#include <Renderer/Descriptors/FontDesc.h>
#include <Window/Window.h>
#include <InputManager.h>
#include <GLFW/glfw3.h>

const int WIDTH = 1920;
const int HEIGHT = 1080;

UIRenderer::UIRenderer(Renderer::Renderer* renderer)
{
    _renderer = renderer;
    CreatePermanentResources();

    InputManager* inputManager = ServiceLocator::GetInputManager();
    inputManager->RegisterBinding("UI Click Checker", GLFW_MOUSE_BUTTON_LEFT, BINDING_ACTION_CLICK, BINDING_MOD_ANY, std::bind(&UIRenderer::OnMouseClick, this, std::placeholders::_1, std::placeholders::_2));
    inputManager->RegisterMouseUpdateCallback("UI Mouse Position Checker", std::bind(&UIRenderer::OnMousePositionUpdate, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void UIRenderer::Update(f32 deltaTime)
{
    for (auto uiPanel : UIPanel::_panels) // TODO: Store panels in a better manner than this
    {
        UI::Panel* panel = uiPanel->GetInternal();

        if (panel->IsDirty())
        {
            // (Re)load texture
            Renderer::TextureID textureID = ReloadTexture(panel->GetTexture());
            panel->SetTextureID(textureID);

            // Update position depending on parents etc
            // TODO

            Renderer::PrimitiveModelDesc primitiveModelDesc;

            // Update vertex buffer
            const vec3& pos = panel->GetPosition();
            const vec2& size = panel->GetSize();

            CalculateVertices(pos, size, primitiveModelDesc.vertices);

            Renderer::ModelID modelID = panel->GetModelID();
            // If the primitive model hasn't been created yet, create it
            if (modelID == Renderer::ModelID::Invalid())
            {
                // Indices
                primitiveModelDesc.indices.push_back(0);
                primitiveModelDesc.indices.push_back(1);
                primitiveModelDesc.indices.push_back(2);
                primitiveModelDesc.indices.push_back(1);
                primitiveModelDesc.indices.push_back(3);
                primitiveModelDesc.indices.push_back(2);

                Renderer::ModelID modelID = _renderer->CreatePrimitiveModel(primitiveModelDesc);
                panel->SetModelID(modelID);
            }
            else // Otherwise we just update the already existing primitive model
            {
                _renderer->UpdatePrimitiveModel(modelID, primitiveModelDesc);
            }

            // Create constant buffer if necessary
            auto constantBuffer = panel->GetConstantBuffer();
            if (constantBuffer == nullptr)
            {
                constantBuffer = _renderer->CreateConstantBuffer<UI::Panel::PanelConstantBuffer>();
                panel->SetConstantBuffer(constantBuffer);
            }
            constantBuffer->resource.color = panel->GetColor();
            constantBuffer->Apply(0);
            constantBuffer->Apply(1);

            panel->ResetDirty();
        }
    }

    for (auto uiLabel : UILabel::_labels) // TODO: Store labels in a better manner than this
    {
        UI::Label* label = uiLabel->GetInternal();

        if (label->IsDirty())
        {
            // (Re)load font
            label->_font = Renderer::Font::GetFont(_renderer, label->GetFontPath(), label->GetFontSize());

            // Grow arrays if necessary
            std::string& text = label->GetText();

            size_t textLength = text.length();
            size_t glyphCount = label->_models.size();

            if (label->_models.size() < textLength)
            {
                label->_models.resize(textLength);
                for (size_t i = glyphCount; i < textLength; i++)
                {
                    label->_models[i] = Renderer::ModelID::Invalid();
                }
            }
            if (label->_textures.size() < textLength)
            {
                label->_textures.resize(textLength);
                for (size_t i = glyphCount; i < textLength; i++)
                {
                    label->_textures[i] = Renderer::TextureID::Invalid();
                }
            }

            vec3 currentPosition = label->GetPosition();
            for (size_t i = 0; i < textLength; i++)
            {
                char character = text[i];
                Renderer::FontChar& fontChar = label->_font->GetChar(character);

                const vec3& pos = currentPosition + vec3(fontChar.xOffset, fontChar.yOffset, 0);
                const vec2& size = vec2(fontChar.width, fontChar.height);

                Renderer::PrimitiveModelDesc primitiveModelDesc;
                primitiveModelDesc.debugName = "Label " + character;

                CalculateVertices(pos, size, primitiveModelDesc.vertices);

                Renderer::ModelID modelID = label->_models[i];

                // If the primitive model hasn't been created yet, create it
                if (modelID == Renderer::ModelID::Invalid())
                {
                    // Indices
                    primitiveModelDesc.indices.push_back(0);
                    primitiveModelDesc.indices.push_back(1);
                    primitiveModelDesc.indices.push_back(2);
                    primitiveModelDesc.indices.push_back(1);
                    primitiveModelDesc.indices.push_back(3);
                    primitiveModelDesc.indices.push_back(2);

                    label->_models[i] = _renderer->CreatePrimitiveModel(primitiveModelDesc);
                }
                else // Otherwise we just update the already existing primitive model
                {
                    _renderer->UpdatePrimitiveModel(modelID, primitiveModelDesc);
                }

                label->_textures[i] = fontChar.texture;

                currentPosition.x += fontChar.advance;
            }

            // Create constant buffer if necessary
            auto constantBuffer = label->GetConstantBuffer();
            if (constantBuffer == nullptr)
            {
                constantBuffer = _renderer->CreateConstantBuffer<UI::Label::LabelConstantBuffer>();
                label->SetConstantBuffer(constantBuffer);
            }
            constantBuffer->resource.textColor = label->GetColor();
            constantBuffer->resource.outlineColor = label->GetOutlineColor();
            constantBuffer->resource.outlineWidth = label->GetOutlineWidth();
            constantBuffer->Apply(0);
            constantBuffer->Apply(1);

            label->ResetDirty();
        }
    }
}

void UIRenderer::AddUIPass(Renderer::RenderGraph* renderGraph, Renderer::ImageID renderTarget, u8 frameIndex)
{
    // UI Pass
    {
        struct UIPassData
        {
            Renderer::RenderPassMutableResource renderTarget;
        };

        renderGraph->AddPass<UIPassData>("UI Pass",
            [&](UIPassData& data, Renderer::RenderGraphBuilder& builder) // Setup
        {
            data.renderTarget = builder.Write(renderTarget, Renderer::RenderGraphBuilder::WriteMode::WRITE_MODE_RENDERTARGET, Renderer::RenderGraphBuilder::LoadMode::LOAD_MODE_LOAD);

            return true; // Return true from setup to enable this pass, return false to disable it
        },
        [&](UIPassData& data, Renderer::CommandList& commandList) // Execute
        {
            Renderer::GraphicsPipelineDesc pipelineDesc;
            renderGraph->InitializePipelineDesc(pipelineDesc);

            // Shaders
            Renderer::VertexShaderDesc vertexShaderDesc;
            vertexShaderDesc.path = "Data/shaders/panel.vert.spv";
            pipelineDesc.states.vertexShader = _renderer->LoadShader(vertexShaderDesc);

            Renderer::PixelShaderDesc pixelShaderDesc;
            pixelShaderDesc.path = "Data/shaders/panel.frag.spv";
            pipelineDesc.states.pixelShader = _renderer->LoadShader(pixelShaderDesc);

            // Input layouts TODO: Improve on this, if I set state 0 and 3 it won't work etc... Maybe responsibility for this should be moved to ModelHandler and the cooker?
            pipelineDesc.states.inputLayouts[0].enabled = true;
            pipelineDesc.states.inputLayouts[0].SetName("POSITION");
            pipelineDesc.states.inputLayouts[0].format = Renderer::InputFormat::INPUT_FORMAT_R32G32B32_FLOAT;
            pipelineDesc.states.inputLayouts[0].inputClassification = Renderer::InputClassification::INPUT_CLASSIFICATION_PER_VERTEX;
            pipelineDesc.states.inputLayouts[1].enabled = true;
            pipelineDesc.states.inputLayouts[1].SetName("NORMAL");
            pipelineDesc.states.inputLayouts[1].format = Renderer::InputFormat::INPUT_FORMAT_R32G32B32_FLOAT;
            pipelineDesc.states.inputLayouts[1].inputClassification = Renderer::InputClassification::INPUT_CLASSIFICATION_PER_VERTEX;
            pipelineDesc.states.inputLayouts[2].enabled = true;
            pipelineDesc.states.inputLayouts[2].SetName("TEXCOORD");
            pipelineDesc.states.inputLayouts[2].format = Renderer::InputFormat::INPUT_FORMAT_R32G32_FLOAT;
            pipelineDesc.states.inputLayouts[2].inputClassification = Renderer::InputClassification::INPUT_CLASSIFICATION_PER_VERTEX;

            // Viewport
            pipelineDesc.states.viewport.topLeftX = 0;
            pipelineDesc.states.viewport.topLeftY = 0;
            pipelineDesc.states.viewport.width = static_cast<f32>(WIDTH);
            pipelineDesc.states.viewport.height = static_cast<f32>(HEIGHT);
            pipelineDesc.states.viewport.minDepth = 0.0f;
            pipelineDesc.states.viewport.maxDepth = 1.0f;

            // ScissorRect
            pipelineDesc.states.scissorRect.left = 0;
            pipelineDesc.states.scissorRect.right = WIDTH;
            pipelineDesc.states.scissorRect.top = 0;
            pipelineDesc.states.scissorRect.bottom = HEIGHT;

            // Rasterizer state
            pipelineDesc.states.rasterizerState.cullMode = Renderer::CullMode::CULL_MODE_BACK;

            // Samplers TODO: We don't care which samplers we have here, we just need the number of samplers
            pipelineDesc.states.samplers[0].enabled = true;

            // Textures TODO: We don't care which textures we have here, we just need the number of textures
            pipelineDesc.textures[0] = Renderer::RenderPassResource(1);

            // Render targets
            pipelineDesc.renderTargets[0] = data.renderTarget;

            // Blending
            pipelineDesc.states.blendState.renderTargets[0].blendEnable = true;
            pipelineDesc.states.blendState.renderTargets[0].srcBlend = Renderer::BlendMode::BLEND_MODE_SRC_ALPHA;
            pipelineDesc.states.blendState.renderTargets[0].destBlend = Renderer::BlendMode::BLEND_MODE_INV_SRC_ALPHA;
            pipelineDesc.states.blendState.renderTargets[0].srcBlendAlpha = Renderer::BlendMode::BLEND_MODE_ZERO;
            pipelineDesc.states.blendState.renderTargets[0].destBlendAlpha = Renderer::BlendMode::BLEND_MODE_ONE;

            // Set pipeline
            Renderer::GraphicsPipelineID pipeline = _renderer->CreatePipeline(pipelineDesc); // This will compile the pipeline and return the ID, or just return ID of cached pipeline
            commandList.BeginPipeline(pipeline);

            // Draw all the panels
            for (auto uiPanel : UIPanel::_panels)
            {
                UI::Panel* panel = uiPanel->GetInternal();

                commandList.PushMarker("Panel", Color(0.0f, 0.1f, 0.0f));

                // Set constant buffer
                commandList.SetConstantBuffer(0, panel->GetConstantBuffer()->GetGPUResource(frameIndex));

                // Set texture-sampler pair
                commandList.SetTextureSampler(1, panel->GetTextureID(), _linearSampler);

                // Draw
                commandList.Draw(panel->GetModelID());

                commandList.PopMarker();
            }
            commandList.EndPipeline(pipeline);


            // Draw text
            vertexShaderDesc.path = "Data/shaders/text.vert.spv";
            pipelineDesc.states.vertexShader = _renderer->LoadShader(vertexShaderDesc);

            pixelShaderDesc.path = "Data/shaders/text.frag.spv";
            pipelineDesc.states.pixelShader = _renderer->LoadShader(pixelShaderDesc);

            // Set pipeline
            pipeline = _renderer->CreatePipeline(pipelineDesc); // This will compile the pipeline and return the ID, or just return ID of cached pipeline
            commandList.BeginPipeline(pipeline);

            // Draw all the labels
            for (auto uiLabel : UILabel::_labels)
            {
                UI::Label* label = uiLabel->GetInternal();

                commandList.PushMarker("Label", Color(0.0f, 0.1f, 0.0f));

                // Set constant buffer
                commandList.SetConstantBuffer(0, label->GetConstantBuffer()->GetGPUResource(frameIndex));

                // Each letter in the label has it's own plane and texture, this could be optimized in the future.
                u32 numModels = label->GetTextLength();
                for (u32 i = 0; i < numModels; i++)
                {
                    // Set texture-sampler pair
                    commandList.SetTextureSampler(1, label->_textures[i], _linearSampler);

                    // Draw
                    commandList.Draw(label->_models[i]);
                }

                commandList.PopMarker();
            }
            commandList.EndPipeline(pipeline);
        });
    }
}

void UIRenderer::OnMouseClick(Window* window, std::shared_ptr<InputBinding> binding)
{
    InputManager* inputManager = ServiceLocator::GetInputManager();
    f32 mouseX = inputManager->GetMousePositionX();
    f32 mouseY = inputManager->GetMousePositionY();

    for (auto uiPanel : UIPanel::_panels) // TODO: Store panels in a better manner than this
    {
        UI::Panel* panel = uiPanel->GetInternal();

        if (!panel->IsClickable() && !panel->IsDragable())
            continue;

        const vec2& size = panel->GetSize();
        const vec2& pos = panel->GetPosition();

        if ((mouseX > pos.x && mouseX < pos.x + size.x) &&
            (mouseY > pos.y && mouseY < pos.y + size.y))
        {
            if (binding->state)
            {
                if (panel->IsDragable())
                    panel->BeingDrag(vec2(mouseX - pos.x, mouseY - pos.y));
            }
            else
            {
                if (panel->IsClickable())
                {
                    if (!panel->DidDrag())
                        uiPanel->OnClick();
                }
            }
        }

        if (!binding->state)
        {
            if (panel->IsDragable())
            {
                panel->EndDrag();
            }
        }
    }
}

void UIRenderer::OnMousePositionUpdate(Window* window, f32 x, f32 y)
{
    for (auto uiPanel : UIPanel::_panels) // TODO: Store panels in a better manner than this
    {
        UI::Panel* panel = uiPanel->GetInternal();

        if (!panel->IsDragable())
            continue;

        if (!panel->IsDragging())
            continue;

        if (!panel->DidDrag())
            panel->SetDidDrag();

        const vec2& deltaDragPosition = panel->GetDeltaDragPosition();
        const vec2& size = panel->GetSize();
        vec3 newPosition(x - deltaDragPosition.x, y - deltaDragPosition.y, 0);

        panel->SetPosition(newPosition);
        panel->SetDirty();
    }
}

void UIRenderer::CreatePermanentResources()
{
    // Sampler
    Renderer::SamplerDesc samplerDesc;
    samplerDesc.enabled = true;
    samplerDesc.filter = Renderer::SamplerFilter::SAMPLER_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.addressU = Renderer::TextureAddressMode::TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.addressV = Renderer::TextureAddressMode::TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.addressW = Renderer::TextureAddressMode::TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.shaderVisibility = Renderer::ShaderVisibility::SHADER_VISIBILITY_PIXEL;

    _linearSampler = _renderer->CreateSampler(samplerDesc);
}

Renderer::TextureID UIRenderer::ReloadTexture(std::string& texturePath)
{
    Renderer::TextureDesc textureDesc;
    textureDesc.path = texturePath;

    return _renderer->LoadTexture(textureDesc);
}

void UIRenderer::CalculateVertices(const vec3& pos, const vec2& size, std::vector<Renderer::Vertex>& vertices)
{
    vec3 upperLeftPos = vec3(pos.x, pos.y, 0.0f);
    vec3 upperRightPos = vec3(pos.x + size.x, pos.y, 0.0f);
    vec3 lowerLeftPos = vec3(pos.x, pos.y + size.y, 0.0f);
    vec3 lowerRightPos = vec3(pos.x + size.x, pos.y + size.y, 0.0f);

    // UV space
    // TODO: Do scaling depending on rendertargets actual size instead of assuming 1080p (which is our reference resolution)
    upperLeftPos /= vec3(1920, 1080, 1.0f);
    upperRightPos /= vec3(1920, 1080, 1.0f);
    lowerLeftPos /= vec3(1920, 1080, 1.0f);
    lowerRightPos /= vec3(1920, 1080, 1.0f);

    // Vertices
    Renderer::Vertex upperLeft;
    upperLeft.pos = upperLeftPos;
    upperLeft.normal = vec3(0, 1, 0);
    upperLeft.texCoord = vec2(0, 0);

    Renderer::Vertex upperRight;
    upperRight.pos = upperRightPos;
    upperRight.normal = vec3(0, 1, 0);
    upperRight.texCoord = vec2(1, 0);

    Renderer::Vertex lowerLeft;
    lowerLeft.pos = lowerLeftPos;
    lowerLeft.normal = vec3(0, 1, 0);
    lowerLeft.texCoord = vec2(0, 1);

    Renderer::Vertex lowerRight;
    lowerRight.pos = lowerRightPos;
    lowerRight.normal = vec3(0, 1, 0);
    lowerRight.texCoord = vec2(1, 1);

    vertices.push_back(upperLeft);
    vertices.push_back(upperRight);
    vertices.push_back(lowerLeft);
    vertices.push_back(lowerRight);
}
