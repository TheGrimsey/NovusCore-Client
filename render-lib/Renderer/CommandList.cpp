#pragma once
#include "CommandList.h"
#include "Renderer.h"
#include <tracy/Tracy.hpp>

// Commands
#include "Commands/Clear.h"
#include "Commands/Draw.h"
#include "Commands/DrawBindless.h"
#include "Commands/DrawIndexedBindless.h"
#include "Commands/DrawIndexed.h"
#include "Commands/DrawIndexedIndirect.h"
#include "Commands/DrawIndexedIndirectCount.h"
#include "Commands/Dispatch.h"
#include "Commands/DispatchIndirect.h"
#include "Commands/PopMarker.h"
#include "Commands/PushMarker.h"
#include "Commands/SetPipeline.h"
#include "Commands/SetScissorRect.h"
#include "Commands/SetViewport.h"
#include "Commands/SetVertexBuffer.h"
#include "Commands/SetIndexBuffer.h"
#include "Commands/SetBuffer.h"
#include "Commands/BindDescriptorSet.h"
#include "Commands/MarkFrameStart.h"
#include "Commands/BeginTrace.h"
#include "Commands/EndTrace.h"
#include "Commands/AddSignalSemaphore.h"
#include "Commands/AddWaitSemaphore.h"
#include "Commands/CopyBuffer.h"
#include "Commands/PipelineBarrier.h"
#include "Commands/DrawImgui.h"
#include "Commands/PushConstant.h"

namespace Renderer
{
    ScopedGPUProfilerZone::ScopedGPUProfilerZone(CommandList& commandList, const tracy::SourceLocationData* sourceLocation)
        : _commandList(commandList)
    {
        _commandList.BeginTrace(sourceLocation);
    }

    ScopedGPUProfilerZone::~ScopedGPUProfilerZone()
    {
        _commandList.EndTrace();
    }

    void CommandList::Execute()
    {
#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        _renderer->EndCommandList(_immediateCommandList);
#else
        assert(_markerScope == 0); // We need to pop all markers that we push

        CommandListID commandList = _renderer->BeginCommandList();

        {
            ZoneScopedNC("Record commandlist", tracy::Color::Red2)
            // Execute each command
            for (int i = 0; i < _functions.Count(); i++)
            {
                _functions[i](_renderer, commandList, _data[i]);
            }
        }
        _renderer->EndCommandList(commandList);
#endif
    }

    CommandList::CommandList(Renderer* renderer, Memory::Allocator* allocator)
        : _renderer(renderer)
        , _allocator(allocator)
        , _markerScope(0)
        , _functions(allocator, 32)
        , _data(allocator, 32)
    {
#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        _immediateCommandList = _renderer->BeginCommandList();
#endif
    }

    void CommandList::MarkFrameStart(u32 frameIndex)
    {
        Commands::MarkFrameStart* command = AddCommand<Commands::MarkFrameStart>();
        command->frameIndex = frameIndex;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::MarkFrameStart::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::BeginTrace(const tracy::SourceLocationData* sourceLocation)
    {
        Commands::BeginTrace* command = AddCommand<Commands::BeginTrace>();
        command->sourceLocation = sourceLocation;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::BeginTrace::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::EndTrace()
    {
        Commands::EndTrace* command = AddCommand<Commands::EndTrace>();

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::BeginTrace::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::PushMarker(std::string marker, Color color)
    {
        Commands::PushMarker* command = AddCommand<Commands::PushMarker>();
        assert(marker.length() < 16); // Max length of marker names is enforced to 15 chars since we have to store the string internally
        strcpy_s(command->marker, marker.c_str());
        command->color = color;

        _markerScope++;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::PushMarker::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::PopMarker()
    {
        Commands::PopMarker* command = AddCommand<Commands::PopMarker>();

        assert(_markerScope > 0); // We tried to pop a marker we never pushed
        _markerScope--;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::PopMarker::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::BeginPipeline(GraphicsPipelineID pipelineID)
    {
        Commands::BeginGraphicsPipeline* command = AddCommand<Commands::BeginGraphicsPipeline>();
        command->pipeline = pipelineID;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::BeginGraphicsPipeline::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::EndPipeline(GraphicsPipelineID pipelineID)
    {
        Commands::EndGraphicsPipeline* command = AddCommand<Commands::EndGraphicsPipeline>();
        command->pipeline = pipelineID;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::EndGraphicsPipeline::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::BeginPipeline(ComputePipelineID pipelineID)
    {
        Commands::BeginComputePipeline* command = AddCommand<Commands::BeginComputePipeline>();
        command->pipeline = pipelineID;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::BeginComputePipeline::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::EndPipeline(ComputePipelineID pipelineID)
    {
        Commands::EndComputePipeline* command = AddCommand<Commands::EndComputePipeline>();
        command->pipeline = pipelineID;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::EndComputePipeline::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::BindDescriptorSet(DescriptorSetSlot slot, DescriptorSet* descriptorSet, u32 frameIndex)
    {
        const std::vector<Descriptor>& descriptors = descriptorSet->GetDescriptors();
        size_t numDescriptors = descriptors.size();

        Commands::BindDescriptorSet* command = AddCommand<Commands::BindDescriptorSet>();
        command->slot = slot;

        // Make a copy of the current state of this DescriptorSets descriptors, this uses our per-frame stack allocator so it's gonna be fast and not leak
        command->descriptors = Memory::Allocator::NewArray<Descriptor>(_allocator, numDescriptors);
        memcpy(command->descriptors, descriptors.data(), sizeof(Descriptor) * numDescriptors);

        command->numDescriptors = static_cast<u32>(numDescriptors);
        command->frameIndex = frameIndex;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::BindDescriptorSet::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::SetScissorRect(u32 left, u32 right, u32 top, u32 bottom)
    {
        Commands::SetScissorRect* command = AddCommand<Commands::SetScissorRect>();
        command->scissorRect.left = left;
        command->scissorRect.right = right;
        command->scissorRect.top = top;
        command->scissorRect.bottom = bottom;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::SetScissorRect::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::SetViewport(f32 topLeftX, f32 topLeftY, f32 width, f32 height, f32 minDepth, f32 maxDepth)
    {
        Commands::SetViewport* command = AddCommand<Commands::SetViewport>();
        command->viewport.topLeftX = topLeftX;
        command->viewport.topLeftY = topLeftY;
        command->viewport.width = width;
        command->viewport.height = height;
        command->viewport.minDepth = minDepth;
        command->viewport.maxDepth = maxDepth;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::SetViewport::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::SetVertexBuffer(u32 slot, BufferID buffer)
    {
        Commands::SetVertexBuffer* command = AddCommand<Commands::SetVertexBuffer>();
        command->slot = slot;
        command->bufferID = buffer;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::SetVertexBuffer::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::SetIndexBuffer(BufferID buffer, IndexFormat indexFormat)
    {
        Commands::SetIndexBuffer* command = AddCommand<Commands::SetIndexBuffer>();
        command->bufferID = buffer;
        command->indexFormat = indexFormat;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::SetIndexBuffer::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::SetBuffer(u32 slot, BufferID buffer)
    {
        Commands::SetBuffer* command = AddCommand<Commands::SetBuffer>();
        command->slot = slot;
        command->buffer = buffer;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::SetBuffer::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::Clear(ImageID imageID, Color color)
    {
        Commands::ClearImage* command = AddCommand<Commands::ClearImage>();                                                                                                       
        command->image = imageID;
        command->color = color;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::ClearImage::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::Clear(DepthImageID imageID, f32 depth, DepthClearFlags flags, u8 stencil)
    {
        Commands::ClearDepthImage* command = AddCommand<Commands::ClearDepthImage>();                                                                                                      
        command->image = imageID;
        command->depth = depth;
        command->flags = flags;
        command->stencil = stencil;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::ClearDepthImage::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::DrawBindless(u32 numVertices, u32 numInstances)
    {
        assert(numVertices > 0);
        assert(numInstances > 0);
        Commands::DrawBindless* command = AddCommand<Commands::DrawBindless>();
        command->numVertices = numVertices;
        command->numInstances = numInstances;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::DrawBindless::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::DrawIndexedBindless(ModelID modelID, u32 numVertices, u32 numInstances)
    {
        assert(modelID != ModelID::Invalid());
        assert(numVertices > 0);
        assert(numInstances > 0);
        Commands::DrawIndexedBindless* command = AddCommand<Commands::DrawIndexedBindless>();
        command->modelID = modelID;
        command->numVertices = numVertices;
        command->numInstances = numInstances;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::DrawIndexedBindless::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::Draw(u32 numVertices, u32 numInstances, u32 vertexOffset, u32 instanceOffset)
    {
        Commands::Draw* command = AddCommand<Commands::Draw>();
        command->vertexCount = numVertices;
        command->instanceCount = numInstances;
        command->vertexOffset = vertexOffset;
        command->instanceOffset = instanceOffset;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::Draw::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::DrawIndexed(u32 numIndices, u32 numInstances, u32 indexOffset, u32 vertexOffset, u32 instanceOffset)
    {
        Commands::DrawIndexed* command = AddCommand<Commands::DrawIndexed>();
        command->indexCount = numIndices;
        command->instanceCount = numInstances;
        command->indexOffset = indexOffset;
        command->vertexOffset = vertexOffset;
        command->instanceOffset = instanceOffset;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::DrawIndexed::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::DrawIndexedIndirect(BufferID argumentBuffer, u32 argumentBufferOffset, u32 drawCount)
    {
        assert(argumentBuffer != BufferID::Invalid());
        Commands::DrawIndexedIndirect* command = AddCommand<Commands::DrawIndexedIndirect>();
        command->argumentBuffer = argumentBuffer;
        command->argumentBufferOffset = argumentBufferOffset;
        command->drawCount = drawCount;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::DrawIndexedIndirect::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::DrawIndexedIndirectCount(BufferID argumentBuffer, u32 argumentBufferOffset, BufferID drawCountBuffer, u32 drawCountBufferOffset, u32 maxDrawCount)
    {
        assert(argumentBuffer != BufferID::Invalid());
        assert(drawCountBuffer != BufferID::Invalid());
        Commands::DrawIndexedIndirectCount* command = AddCommand<Commands::DrawIndexedIndirectCount>();
        command->argumentBuffer = argumentBuffer;
        command->argumentBufferOffset = argumentBufferOffset;
        command->drawCountBuffer = drawCountBuffer;
        command->drawCountBufferOffset = drawCountBufferOffset;
        command->maxDrawCount = maxDrawCount;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::DrawIndexedIndirectCount::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::Dispatch(u32 numThreadGroupsX, u32 numThreadGroupsY, u32 numThreadGroupsZ)
    {
        assert(numThreadGroupsX > 0);
        assert(numThreadGroupsY > 0);
        assert(numThreadGroupsZ > 0);
        Commands::Dispatch* command = AddCommand<Commands::Dispatch>();
        command->threadGroupCountX = numThreadGroupsX;
        command->threadGroupCountY = numThreadGroupsY;
        command->threadGroupCountZ = numThreadGroupsZ;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::Dispatch::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::DispatchIndirect(BufferID argumentBuffer, u32 argumentBufferOffset)
    {
        assert(argumentBuffer != BufferID::Invalid());
        Commands::DispatchIndirect* command = AddCommand<Commands::DispatchIndirect>();
        command->argumentBuffer = argumentBuffer;
        command->argumentBufferOffset = argumentBufferOffset;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::DispatchIndirect::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::AddSignalSemaphore(GPUSemaphoreID semaphoreID)
    {
        Commands::AddSignalSemaphore* command = AddCommand<Commands::AddSignalSemaphore>();
        command->semaphore = semaphoreID;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::AddSignalSemaphore::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::AddWaitSemaphore(GPUSemaphoreID semaphoreID)
    {
        Commands::AddWaitSemaphore* command = AddCommand<Commands::AddWaitSemaphore>();
        command->semaphore = semaphoreID;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::AddWaitSemaphore::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::CopyBuffer(BufferID dstBuffer, u64 dstBufferOffset, BufferID srcBuffer, u64 srcBufferOffset, u64 region)
    {
        assert(dstBuffer != BufferID::Invalid());
        assert(srcBuffer != BufferID::Invalid());
        Commands::CopyBuffer* command = AddCommand<Commands::CopyBuffer>();
        command->dstBuffer = dstBuffer;
        command->dstBufferOffset = dstBufferOffset;
        command->srcBuffer = srcBuffer;
        command->srcBufferOffset = srcBufferOffset;
        command->region = region;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::CopyBuffer::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::PipelineBarrier(PipelineBarrierType type, BufferID buffer)
    {
        assert(buffer != BufferID::Invalid());
        Commands::PipelineBarrier* command = AddCommand<Commands::PipelineBarrier>();
        command->barrierType = type;
        command->buffer = buffer;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::PipelineBarrier::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }

    void CommandList::DrawImgui()
    {
        Commands::DrawImgui* command = AddCommand<Commands::DrawImgui>();

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::DrawImgui::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }    
    
    void CommandList::PushConstant(void* data, u32 offset, u32 size)
    {
        assert(data != nullptr);
        Commands::PushConstant* command = AddCommand<Commands::PushConstant>();
        command->data = data;
        command->offset = offset;
        command->size = size;

#if COMMANDLIST_DEBUG_IMMEDIATE_MODE
        Commands::PushConstant::DISPATCH_FUNCTION(_renderer, _immediateCommandList, command);
#endif
    }
}