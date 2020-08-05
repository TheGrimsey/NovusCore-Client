#include "PipelineHandlerVK.h"
#include <Utils/DebugHandler.h>
#include <Utils/XXHash64.h>
#include "FormatConverterVK.h"
#include "RenderDeviceVK.h"
#include "ShaderHandlerVK.h"
#include "ImageHandlerVK.h"
#include "SpirvReflect.h"
#include "DescriptorSetBuilderVK.h"


namespace Renderer
{
    namespace Backend
    {
        void PipelineHandlerVK::Init(RenderDeviceVK* device, ShaderHandlerVK* shaderHandler, ImageHandlerVK* imageHandler)
        {
            _device = device;
            _shaderHandler = shaderHandler;
            _imageHandler = imageHandler;
        }

        void PipelineHandlerVK::OnWindowResize()
        {
            for (auto& pipeline : _graphicsPipelines)
            {
                vkDestroyFramebuffer(_device->_device, pipeline.framebuffer, nullptr);
                CreateFramebuffer(pipeline);
            }
        }

        GraphicsPipelineID PipelineHandlerVK::CreatePipeline(const GraphicsPipelineDesc& desc)
        {
            assert(desc.ResourceToImageID != nullptr); // You need to bind this function pointer before creating pipeline, maybe use RenderGraph::InitializePipelineDesc?
            assert(desc.ResourceToDepthImageID != nullptr); // You need to bind this function pointer before creating pipeline, maybe use RenderGraph::InitializePipelineDesc?
            assert(desc.MutableResourceToImageID != nullptr); // You need to bind this function pointer before creating pipeline, maybe use RenderGraph::InitializePipelineDesc?
            assert(desc.MutableResourceToDepthImageID != nullptr); // You need to bind this function pointer before creating pipeline, maybe use RenderGraph::InitializePipelineDesc?

            // Check the cache
            size_t nextID;
            u64 cacheDescHash = CalculateCacheDescHash(desc);
            if (TryFindExistingGPipeline(cacheDescHash, nextID))
            {
                return GraphicsPipelineID(static_cast<gIDType>(nextID));
            }
            nextID = _graphicsPipelines.size();

            // Make sure we haven't exceeded the limit of the GraphicsPipelineID type, if this hits you need to change type of GraphicsPipelineID to something bigger
            assert(nextID < GraphicsPipelineID::MaxValue());

            GraphicsPipeline pipeline;
            pipeline.desc = desc;
            pipeline.cacheDescHash = cacheDescHash;

            // -- Get number of render targets and attachments --
            u8 numAttachments = 0;
            for (int i = 0; i < MAX_RENDER_TARGETS; i++)
            {
                if (desc.renderTargets[i] == RenderPassMutableResource::Invalid())
                    break;

                pipeline.numRenderTargets++;
                numAttachments++;
            }

            // -- Create Render Pass --
            std::vector<VkAttachmentDescription> attachments(numAttachments);
            std::vector< VkAttachmentReference> colorAttachmentRefs(numAttachments);
            for (int i = 0; i < numAttachments; i++)
            {
                ImageID imageID = desc.MutableResourceToImageID(desc.renderTargets[i]);
                const ImageDesc& imageDesc = _imageHandler->GetImageDesc(imageID);
                attachments[i].format = FormatConverterVK::ToVkFormat(imageDesc.format);
                attachments[i].samples = FormatConverterVK::ToVkSampleCount(imageDesc.sampleCount);
                attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachments[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
                attachments[i].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

                colorAttachmentRefs[i].attachment = i;
                colorAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = numAttachments;
            subpass.pColorAttachments = colorAttachmentRefs.data();

            VkAttachmentReference depthDescriptionRef = {};

            // If we have a depthstencil, add an attachment for that
            if (desc.depthStencil != RenderPassMutableResource::Invalid())
            {
                DepthImageID depthImageID = desc.MutableResourceToDepthImageID(desc.depthStencil);
                const DepthImageDesc& imageDesc = _imageHandler->GetDepthImageDesc(depthImageID);

                u32 attachmentSlot = numAttachments++;
                
                VkAttachmentDescription& depthDescription = attachments.emplace_back();
                depthDescription = {};
                depthDescription.format = FormatConverterVK::ToVkFormat(imageDesc.format);
                depthDescription.samples = FormatConverterVK::ToVkSampleCount(imageDesc.sampleCount);
                depthDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                depthDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depthDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depthDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthDescription.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                depthDescriptionRef.attachment = attachmentSlot;
                depthDescriptionRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                subpass.pDepthStencilAttachment = &depthDescriptionRef;
            }

            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = numAttachments;
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(_device->_device, &renderPassInfo, nullptr, &pipeline.renderPass) != VK_SUCCESS)
            {
                NC_LOG_FATAL("Failed to create render pass!");
            }

            // -- Create Framebuffer --
            CreateFramebuffer(pipeline);

            // -- Create Descriptor Set Layout from reflected SPIR-V --
            std::vector<BindInfo> bindInfos;
            if (desc.states.vertexShader != VertexShaderID::Invalid())
            {
                const BindReflection& bindReflection = _shaderHandler->GetBindReflection(desc.states.vertexShader);
                bindInfos.insert(bindInfos.end(), bindReflection.dataBindings.begin(), bindReflection.dataBindings.end());
            }
            if (desc.states.pixelShader != PixelShaderID::Invalid())
            {
                const BindReflection& bindReflection = _shaderHandler->GetBindReflection(desc.states.pixelShader);
                bindInfos.insert(bindInfos.end(), bindReflection.dataBindings.begin(), bindReflection.dataBindings.end());
            }

            for (BindInfo& bindInfo : bindInfos)
            {
                DescriptorSetLayoutData& layout = GetDescriptorSet(bindInfo.set, pipeline.descriptorSetLayoutDatas);
                VkDescriptorSetLayoutBinding layoutBinding = {};

                layoutBinding.binding = bindInfo.binding;
                layoutBinding.descriptorType = bindInfo.descriptorType;
                layoutBinding.descriptorCount = bindInfo.count;
                layoutBinding.stageFlags = bindInfo.stageFlags;

                layout.bindings.push_back(layoutBinding);
            }

            size_t numDescriptorSets = pipeline.descriptorSetLayoutDatas.size();
            pipeline.descriptorSetLayouts.resize(numDescriptorSets);

            for (size_t i = 0; i < numDescriptorSets; i++)
            {
                pipeline.descriptorSetLayoutDatas[i].createInfo.bindingCount = static_cast<u32>(pipeline.descriptorSetLayoutDatas[i].bindings.size());
                pipeline.descriptorSetLayoutDatas[i].createInfo.pBindings = pipeline.descriptorSetLayoutDatas[i].bindings.data();

                if (vkCreateDescriptorSetLayout(_device->_device, &pipeline.descriptorSetLayoutDatas[i].createInfo, nullptr, &pipeline.descriptorSetLayouts[i]) != VK_SUCCESS)
                {
                    NC_LOG_FATAL("Failed to create descriptor set layout!");
                }
            }

            std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
            if (desc.states.vertexShader != VertexShaderID::Invalid())
            {
                VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
                vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

                vertShaderStageInfo.module = _shaderHandler->GetShaderModule(desc.states.vertexShader);
                vertShaderStageInfo.pName = "main";

                shaderStages.push_back(vertShaderStageInfo);
            }
            if (desc.states.pixelShader != PixelShaderID::Invalid())
            {
                VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
                fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

                fragShaderStageInfo.module = _shaderHandler->GetShaderModule(desc.states.pixelShader);
                fragShaderStageInfo.pName = "main";

                shaderStages.push_back(fragShaderStageInfo);
            }

            // Now we need to create vertex input bindings, one (if necessary) for per-vertex data, one (if necessary) for per-instance data
            u8 numVertexAttributes = 0;
            u32 vertexStride = 0;

            u8 numInstanceAttributes = 0;
            u32 instanceStride = 0;

            for (auto& inputLayout : desc.states.inputLayouts)
            {
                if (!inputLayout.enabled)
                    break;

                if (inputLayout.inputClassification == Renderer::InputClassification::INPUT_CLASSIFICATION_PER_VERTEX)
                {
                    numVertexAttributes++;
                    vertexStride += FormatConverterVK::ToByteSize(inputLayout.format);
                }
                else
                {
                    numInstanceAttributes++;
                    instanceStride += FormatConverterVK::ToByteSize(inputLayout.format);
                }
            }

            // -- Create binding description(s) --
            std::vector<VkVertexInputBindingDescription> inputBindingDescriptions;

            u8 vertexBinding = 0;
            if (numVertexAttributes > 0)
            {
                VkVertexInputBindingDescription bindingDescription = {};
                bindingDescription.binding = static_cast<u32>(inputBindingDescriptions.size());
                bindingDescription.stride = vertexStride;
                bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

                inputBindingDescriptions.push_back(bindingDescription);
            }

            u8 instanceBinding = 0;
            if (numInstanceAttributes > 0)
            {
                instanceBinding = static_cast<u8>(inputBindingDescriptions.size());

                VkVertexInputBindingDescription bindingDescription = {};
                bindingDescription.binding = instanceBinding;
                bindingDescription.stride = instanceStride;
                bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

                inputBindingDescriptions.push_back(bindingDescription);
            }

            std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
            attributeDescriptions.reserve(numVertexAttributes + numInstanceAttributes);

            u8 attributeCounts[2] = { 0 };
            u32 attributeOffsets[2] = { 0 };

            for (auto& inputLayout : desc.states.inputLayouts)
            {
                if (!inputLayout.enabled)
                    break;

                bool isPerVertex = inputLayout.inputClassification == Renderer::InputClassification::INPUT_CLASSIFICATION_PER_VERTEX;

                u8 binding = (isPerVertex) ? vertexBinding : instanceBinding;

                u8& attributeCount = attributeCounts[isPerVertex];
                u32& attributeOffset = attributeOffsets[isPerVertex];
                
                VkVertexInputAttributeDescription attributeDescription;
                attributeDescription.binding = binding;
                attributeDescription.location = attributeCount++;
                attributeDescription.format = FormatConverterVK::ToVkFormat(inputLayout.format);
                attributeDescription.offset = attributeOffset;

                attributeOffset += FormatConverterVK::ToByteSize(inputLayout.format);

                attributeDescriptions.push_back(attributeDescription);
            }

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = static_cast<u32>(inputBindingDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = inputBindingDescriptions.data();
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            // -- Set viewport and scissor rect --
            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = nullptr; // These are dynamic
            viewportState.scissorCount = 1;
            viewportState.pScissors = nullptr; // These are dynamic

            // -- Rasterizer --
            VkPipelineRasterizationStateCreateInfo rasterizer = {};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = FormatConverterVK::ToVkPolygonMode(desc.states.rasterizerState.fillMode);
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = FormatConverterVK::ToVkCullModeFlags(desc.states.rasterizerState.cullMode);
            rasterizer.frontFace = FormatConverterVK::ToVkFrontFace(desc.states.rasterizerState.frontFaceMode);
            rasterizer.depthBiasEnable = desc.states.rasterizerState.depthBiasEnabled;
            rasterizer.depthBiasConstantFactor = static_cast<f32>(desc.states.rasterizerState.depthBias);
            rasterizer.depthBiasClamp = desc.states.rasterizerState.depthBiasClamp;
            rasterizer.depthBiasSlopeFactor = desc.states.rasterizerState.depthBiasSlopeFactor;

            // -- Multisampling --
            VkPipelineMultisampleStateCreateInfo multisampling = {};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = FormatConverterVK::ToVkSampleCount(desc.states.rasterizerState.sampleCount);
            multisampling.minSampleShading = 1.0f; // Optional
            multisampling.pSampleMask = nullptr; // Optional
            multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
            multisampling.alphaToOneEnable = VK_FALSE; // Optional

            // -- DepthStencil --
            VkPipelineDepthStencilStateCreateInfo depthStencil = {};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = desc.states.depthStencilState.depthEnable;
            depthStencil.depthWriteEnable = desc.states.depthStencilState.depthWriteEnable;
            depthStencil.depthCompareOp = FormatConverterVK::ToVkCompareOp(desc.states.depthStencilState.depthFunc);
            //depthStencil.depthBoundsTestEnable = desc.states.depthStencilState;
            //depthStencil.minDepthBounds = 0.0f;
            //depthStencil.maxDepthBounds = 1.0f;
            depthStencil.stencilTestEnable = desc.states.depthStencilState.stencilEnable;

            depthStencil.front = {};
            depthStencil.front.failOp = FormatConverterVK::ToVkStencilOp(desc.states.depthStencilState.frontFace.stencilFailOp);
            depthStencil.front.passOp = FormatConverterVK::ToVkStencilOp(desc.states.depthStencilState.frontFace.stencilPassOp);
            depthStencil.front.depthFailOp = FormatConverterVK::ToVkStencilOp(desc.states.depthStencilState.frontFace.stencilDepthFailOp);
            depthStencil.front.compareOp = FormatConverterVK::ToVkCompareOp(desc.states.depthStencilState.frontFace.stencilFunc);
            //depthStencil.front.compareMask;
            //depthStencil.front.writeMask;
            //depthStencil.front.reference;

            depthStencil.back = {};
            depthStencil.back.failOp = FormatConverterVK::ToVkStencilOp(desc.states.depthStencilState.backFace.stencilFailOp);
            depthStencil.back.passOp = FormatConverterVK::ToVkStencilOp(desc.states.depthStencilState.backFace.stencilPassOp);
            depthStencil.back.depthFailOp = FormatConverterVK::ToVkStencilOp(desc.states.depthStencilState.backFace.stencilDepthFailOp);
            depthStencil.back.compareOp = FormatConverterVK::ToVkCompareOp(desc.states.depthStencilState.backFace.stencilFunc);
            //depthStencil.back.compareMask;
            //depthStencil.back.writeMask;
            //depthStencil.back.reference;

            // -- Blenders --
            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(pipeline.numRenderTargets);
            
            for (u32 i = 0; i < pipeline.numRenderTargets; i++)
            {
                colorBlendAttachments[i].blendEnable = desc.states.blendState.renderTargets[i].blendEnable;
                colorBlendAttachments[i].srcColorBlendFactor = FormatConverterVK::ToVkBlendFactor(desc.states.blendState.renderTargets[i].srcBlend);
                colorBlendAttachments[i].dstColorBlendFactor = FormatConverterVK::ToVkBlendFactor(desc.states.blendState.renderTargets[i].destBlend);
                colorBlendAttachments[i].colorBlendOp = FormatConverterVK::ToVkBlendOp(desc.states.blendState.renderTargets[i].blendOp);
                colorBlendAttachments[i].srcAlphaBlendFactor = FormatConverterVK::ToVkBlendFactor(desc.states.blendState.renderTargets[i].srcBlendAlpha);
                colorBlendAttachments[i].dstAlphaBlendFactor = FormatConverterVK::ToVkBlendFactor(desc.states.blendState.renderTargets[i].destBlendAlpha);
                colorBlendAttachments[i].alphaBlendOp = FormatConverterVK::ToVkBlendOp(desc.states.blendState.renderTargets[i].blendOpAlpha);
                colorBlendAttachments[i].colorWriteMask = FormatConverterVK::ToVkColorComponentFlags(desc.states.blendState.renderTargets[i].renderTargetWriteMask);
            }

            VkPipelineColorBlendStateCreateInfo colorBlending = {};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = desc.states.blendState.renderTargets[0].logicOpEnable;
            colorBlending.logicOp = FormatConverterVK::ToVkLogicOp(desc.states.blendState.renderTargets[0].logicOp);
            colorBlending.attachmentCount = pipeline.numRenderTargets;
            colorBlending.pAttachments = colorBlendAttachments.data();
            colorBlending.blendConstants[0] = 0.0f; // TODO: Blend constants
            colorBlending.blendConstants[1] = 0.0f; // TODO: Blend constants
            colorBlending.blendConstants[2] = 0.0f; // TODO: Blend constants
            colorBlending.blendConstants[3] = 0.0f; // TODO: Blend constants
            
            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<u32>(pipeline.descriptorSetLayouts.size());
            pipelineLayoutInfo.pSetLayouts = pipeline.descriptorSetLayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

            if (vkCreatePipelineLayout(_device->_device, &pipelineLayoutInfo, nullptr, &pipeline.pipelineLayout) != VK_SUCCESS)
            {
                NC_LOG_FATAL("Failed to create pipeline layout!");
            }

            // Set up dynamic viewport and scissor
            std::vector<VkDynamicState> dynamicStates;
            dynamicStates.reserve(2);

            dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
            dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

            VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
            dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCreateInfo.dynamicStateCount = static_cast<u32>(dynamicStates.size());
            dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = static_cast<u32>(shaderStages.size());
            pipelineInfo.pStages = shaderStages.data();
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = &depthStencil;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
            pipelineInfo.layout = pipeline.pipelineLayout;
            pipelineInfo.renderPass = pipeline.renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
            pipelineInfo.basePipelineIndex = -1; // Optional

            if (vkCreateGraphicsPipelines(_device->_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS)
            {
                NC_LOG_FATAL("Failed to create graphics pipeline!");
            }

            GraphicsPipelineID pipelineID = GraphicsPipelineID(static_cast<gIDType>(nextID));
            pipeline.descriptorSetBuilder = new DescriptorSetBuilderVK(pipelineID, this, _shaderHandler, _device->_descriptorMegaPool);

            _graphicsPipelines.push_back(pipeline);

            pipeline.descriptorSetBuilder->InitReflectData(); // Needs to happen after push_back


            return pipelineID;
        }

        ComputePipelineID PipelineHandlerVK::CreatePipeline(const ComputePipelineDesc& desc)
        {
            return ComputePipelineID();
        }

        u64 PipelineHandlerVK::CalculateCacheDescHash(const GraphicsPipelineDesc& desc)
        {
            GraphicsPipelineCacheDesc cacheDesc;
            cacheDesc.states = desc.states;

            for (int i = 0; i < MAX_RENDER_TARGETS; i++)
            {
                if (desc.renderTargets[i] == RenderPassMutableResource::Invalid())
                    break;

                cacheDesc.renderTargets[i] = desc.MutableResourceToImageID(desc.renderTargets[i]);
            }

            if (desc.depthStencil != RenderPassMutableResource::Invalid())
            {
                cacheDesc.depthStencil = desc.MutableResourceToDepthImageID(desc.depthStencil);
            }

            u64 hash = XXHash64::hash(&cacheDesc, sizeof(cacheDesc), 0);

            return hash;
        }

        bool PipelineHandlerVK::TryFindExistingGPipeline(u64 descHash, size_t& id)
        {
            id = 0;

            for (auto& pipeline : _graphicsPipelines)
            {
                if (descHash == pipeline.cacheDescHash)
                {
                    return true;
                }
                id++;
            }

            return false;
        }

        bool PipelineHandlerVK::TryFindExistingCPipeline(u64 descHash, size_t& id)
        {
            id = 0;

            for (auto& pipeline : _computePipelines)
            {
                if (descHash == pipeline.cacheDescHash)
                {
                    return true;
                }
                id++;
            }

            return false;
        }

        DescriptorSetLayoutData& PipelineHandlerVK::GetDescriptorSet(i32 setNumber, std::vector<DescriptorSetLayoutData>& sets)
        {
            while (static_cast<i32>(sets.size())-1 < setNumber)
            {
                DescriptorSetLayoutData setLayoutData = {};
                setLayoutData.createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                setLayoutData.createInfo.bindingCount = 0;
                setLayoutData.createInfo.pBindings = nullptr;

                sets.push_back(setLayoutData);
            }

            return sets[setNumber];
        }

        void PipelineHandlerVK::CreateFramebuffer(GraphicsPipeline& pipeline)
        {
            u32 numAttachments = pipeline.numRenderTargets;
            
            if (pipeline.desc.depthStencil != RenderPassMutableResource::Invalid())
            {
                numAttachments += 1;
            }

            std::vector<VkImageView> attachmentViews(numAttachments);

            // Add all color rendertargets as attachments
            for (u32 i = 0; i < pipeline.numRenderTargets; i++)
            {
                ImageID imageID = pipeline.desc.MutableResourceToImageID(pipeline.desc.renderTargets[i]);
                attachmentViews[i] = _imageHandler->GetColorView(imageID);
            }
            // Add depthstencil as attachment
            if (pipeline.desc.depthStencil != RenderPassMutableResource::Invalid())
            {
                DepthImageID depthImageID = pipeline.desc.MutableResourceToDepthImageID(pipeline.desc.depthStencil);
                attachmentViews[pipeline.numRenderTargets] = _imageHandler->GetDepthView(depthImageID);
            }

            uvec2 renderSize = _device->GetMainWindowSize();

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = pipeline.renderPass;
            framebufferInfo.attachmentCount = static_cast<u32>(attachmentViews.size());
            framebufferInfo.pAttachments = attachmentViews.data();
            framebufferInfo.width = renderSize.x;
            framebufferInfo.height = renderSize.y;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(_device->_device, &framebufferInfo, nullptr, &pipeline.framebuffer) != VK_SUCCESS)
            {
                NC_LOG_FATAL("Failed to create framebuffer!");
            }
        }
    }
}