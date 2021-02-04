#include "CommandListHandlerVK.h"
#include "RenderDeviceVK.h"

#include <queue>
#include <vector>
#include <cassert>
#include <vulkan/vulkan.h>
#include <Utils/DebugHandler.h>
#include <tracy/TracyVulkan.hpp>
#include <tracy/Tracy.hpp>

namespace Renderer
{
    namespace Backend
    {
        struct CommandList
        {
            std::vector<VkSemaphore> waitSemaphores;
            std::vector<VkSemaphore> signalSemaphores;

            VkCommandBuffer commandBuffer;
            VkCommandPool commandPool;

            tracy::VkCtxManualScope* tracyScope = nullptr;

            GraphicsPipelineID boundGraphicsPipeline = GraphicsPipelineID::Invalid();
            ComputePipelineID boundComputePipeline = ComputePipelineID::Invalid();
        };

        struct CommandListHandlerVKData : ICommandListHandlerVKData
        {
            std::vector<CommandList> commandLists;
            std::queue<CommandListID> availableCommandLists;

            u8 frameIndex = 0;
            FrameResource<std::queue<CommandListID>, 2> closedCommandLists;

            FrameResource<VkFence, 2> frameFences;
        };

        void CommandListHandlerVK::Init(RenderDeviceVK* device)
        {
            _device = device;

            CommandListHandlerVKData* data = new CommandListHandlerVKData();
            _data = data;

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (u32 i = 0; i < data->frameFences.Num; i++)
            {
                vkCreateFence(_device->_device, &fenceInfo, nullptr, &data->frameFences.Get(i));
            }
        }

        void CommandListHandlerVK::FlipFrame()
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            data.frameIndex++;

            if (data.frameIndex >= data.closedCommandLists.Num)
            {
                data.frameIndex = 0;
            }
        }

        void CommandListHandlerVK::ResetCommandBuffers()
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            std::queue<CommandListID>& closedCommandLists = data.closedCommandLists.Get(data.frameIndex);

            while (!closedCommandLists.empty())
            {
                CommandListID commandListID = closedCommandLists.front();
                closedCommandLists.pop();

                CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(commandListID)];

                // Reset commandlist
                vkResetCommandPool(_device->_device, commandList.commandPool, 0);

                // Push the commandlist into availableCommandLists
                data.availableCommandLists.push(commandListID);
            }
        }

        CommandListID CommandListHandlerVK::BeginCommandList()
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            CommandListID id;
            if (!data.availableCommandLists.empty())
            {
                id = data.availableCommandLists.front();
                data.availableCommandLists.pop();

                CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(id)];

                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = 0; // Optional
                beginInfo.pInheritanceInfo = nullptr; // Optional

                if (vkBeginCommandBuffer(commandList.commandBuffer, &beginInfo) != VK_SUCCESS)
                {
                    DebugHandler::PrintFatal("Failed to begin recording command buffer!");
                }
            }
            else
            {
                return CreateCommandList();
            }

            return id;
        }

        void CommandListHandlerVK::EndCommandList(CommandListID id, VkFence fence)
        {
            ZoneScopedC(tracy::Color::Red3);

            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(id)];

            {
                ZoneScopedNC("Submit", tracy::Color::Red3)

                // Close command list
                if (vkEndCommandBuffer(commandList.commandBuffer) != VK_SUCCESS)
                {
                    DebugHandler::PrintFatal("Failed to record command buffer!");
                }

                // Execute command list
                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandList.commandBuffer;

                u32 numWaitSemaphores = static_cast<u32>(commandList.waitSemaphores.size());
                std::vector<VkPipelineStageFlags> dstStageMasks(numWaitSemaphores);

                for (VkPipelineStageFlags& dstStageMask : dstStageMasks)
                {
                    dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                }

                submitInfo.waitSemaphoreCount = numWaitSemaphores;
                submitInfo.pWaitSemaphores = commandList.waitSemaphores.data();
                submitInfo.pWaitDstStageMask = dstStageMasks.data();
                
                submitInfo.signalSemaphoreCount = static_cast<u32>(commandList.signalSemaphores.size());
                submitInfo.pSignalSemaphores = commandList.signalSemaphores.data();

                vkQueueSubmit(_device->_graphicsQueue, 1, &submitInfo, fence);
            }

            commandList.waitSemaphores.clear();
            commandList.signalSemaphores.clear();
            commandList.boundGraphicsPipeline = GraphicsPipelineID::Invalid();

            data.closedCommandLists.Get(data.frameIndex).push(id);
        }

        VkCommandBuffer CommandListHandlerVK::GetCommandBuffer(CommandListID id)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(id)];

            return commandList.commandBuffer;
        }

        void CommandListHandlerVK::AddWaitSemaphore(CommandListID id, VkSemaphore semaphore)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(id)];

            commandList.waitSemaphores.push_back(semaphore);
        }

        void CommandListHandlerVK::AddSignalSemaphore(CommandListID id, VkSemaphore semaphore)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(id)];

            commandList.signalSemaphores.push_back(semaphore);
        }

        void CommandListHandlerVK::SetBoundGraphicsPipeline(CommandListID id, GraphicsPipelineID pipelineID)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(id)];

            commandList.boundGraphicsPipeline = pipelineID;
        }

        void CommandListHandlerVK::SetBoundComputePipeline(CommandListID id, ComputePipelineID pipelineID)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            CommandList& commandList = data.commandLists[static_cast<CommandListID::type>(id)];

            commandList.boundComputePipeline = pipelineID;
        }

        GraphicsPipelineID CommandListHandlerVK::GetBoundGraphicsPipeline(CommandListID id)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            return data.commandLists[static_cast<CommandListID::type>(id)].boundGraphicsPipeline;
        }

        ComputePipelineID CommandListHandlerVK::GetBoundComputePipeline(CommandListID id)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            return data.commandLists[static_cast<CommandListID::type>(id)].boundComputePipeline;
        }

        tracy::VkCtxManualScope*& CommandListHandlerVK::GetTracyScope(CommandListID id)
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            // Lets make sure this id exists
            assert(data.commandLists.size() > static_cast<CommandListID::type>(id));

            return data.commandLists[static_cast<CommandListID::type>(id)].tracyScope;
        }

        VkFence CommandListHandlerVK::GetCurrentFence()
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);
            return data.frameFences.Get(data.frameIndex);
        }

        CommandListID CommandListHandlerVK::CreateCommandList()
        {
            CommandListHandlerVKData& data = static_cast<CommandListHandlerVKData&>(*_data);

            size_t id = data.commandLists.size();
            assert(id < CommandListID::MaxValue());

            CommandList commandList;

            // Create commandpool
            QueueFamilyIndices queueFamilyIndices = _device->FindQueueFamilies(_device->_physicalDevice);

            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            if (vkCreateCommandPool(_device->_device, &poolInfo, nullptr, &commandList.commandPool) != VK_SUCCESS)
            {
                DebugHandler::PrintFatal("Failed to create command pool!");
            }

            // Create commandlist
            VkCommandBufferAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandList.commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            if (vkAllocateCommandBuffers(_device->_device, &allocInfo, &commandList.commandBuffer) != VK_SUCCESS)
            {
                DebugHandler::PrintFatal("Failed to allocate command buffers!");
            }

            // Open commandlist
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0; // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional

            if (vkBeginCommandBuffer(commandList.commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                DebugHandler::PrintFatal("Failed to begin recording command buffer!");
            }

            data.commandLists.push_back(commandList);

            return CommandListID(static_cast<CommandListID::type>(id));
        }
    }
}