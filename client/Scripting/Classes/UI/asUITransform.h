#pragma once
#include <NovusTypes.h>
#include <entity/entity.hpp>

#include "../../ScriptEngine.h"
#include "../../../ECS/Components/UI/UITransform.h"
#include "../../../ECS/Components/UI/UIVisibility.h"
#include "../../../ECS/Components/UI/Singletons/UIDataSingleton.h"

namespace UI
{
    class asUITransform
    {
    public:
        asUITransform(UIElementType elementType);

        static void RegisterType()
        {
            i32 r = ScriptEngine::RegisterScriptClass("UITransform", 0, asOBJ_REF | asOBJ_NOCOUNT);
            assert(r >= 0);
            {
                RegisterBase<asUITransform>();
            }
        }

        template<class T>
        static void RegisterBase()
        {
            i32 r = ScriptEngine::RegisterScriptClassFunction("Entity GetEntityId()", asMETHOD(T, GetEntityId)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void SetTransform(vec2 position, vec2 size)", asMETHOD(T, SetTransform)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("vec2 GetScreenPosition()", asMETHOD(T, GetScreenPosition)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("vec2 GetLocalPosition()", asMETHOD(T, GetLocalPosition)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("vec2 GetParentPosition()", asMETHOD(T, GetParentPosition)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("vec2 GetPosition()", asMETHOD(T, GetPosition)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void SetPosition(vec2 position)", asMETHOD(T, SetPosition)); assert(r >= 0);

            r = ScriptEngine::RegisterScriptClassFunction("vec2 GetLocalAnchor()", asMETHOD(T, GetLocalAnchor)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void SetLocalAnchor(vec2 anchor)", asMETHOD(T, SetLocalAnchor)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("vec2 GetSize()", asMETHOD(T, GetSize)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void SetSize(vec2 size)", asMETHOD(T, SetSize)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("bool GetFillParentSize()", asMETHOD(T, GetFillParentSize)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void SetFillParentSize(bool fillParent)", asMETHOD(T, SetFillParentSize)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("float GetDepth()", asMETHOD(T, GetDepth)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void SetDepth(uint16 depth)", asMETHOD(T, SetDepth)); assert(r >= 0);

            r = ScriptEngine::RegisterScriptClassFunction("void SetParent(UITransform@ parent)", asMETHOD(T, SetParent)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void Destroy()", asMETHOD(T, Destroy)); assert(r >= 0);

            r = ScriptEngine::RegisterScriptClassFunction("void SetExpandBoundsToChildren(bool enabled)", asMETHOD(T, SetExpandBoundsToChildren)); assert(r >= 0);

            r = ScriptEngine::RegisterScriptClassFunction("bool IsVisible()", asMETHOD(T, IsVisible)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("bool IsLocallyVisible()", asMETHOD(T, IsLocallyVisible)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("bool IsParentVisible()", asMETHOD(T, IsParentVisible)); assert(r >= 0);
            r = ScriptEngine::RegisterScriptClassFunction("void SetVisible(bool visible)", asMETHOD(T, SetVisible)); assert(r >= 0);
        }

        virtual const entt::entity GetEntityId() const
        {
            return _entityId;
        }

        virtual const UIElementType GetType() const
        {
            return _elementType;
        }

        // Transform Functions
        virtual void SetTransform(const vec2& position, const vec2& size);

        virtual const vec2 GetScreenPosition() const
        {
            return _transform.position + _transform.localPosition;
        }
        virtual const vec2 GetLocalPosition() const
        {
            return _transform.parent ? _transform.localPosition : vec2(0, 0);
        }
        virtual const vec2 GetParentPosition() const
        {
            return _transform.parent ? _transform.position : vec2(0, 0);
        }
        virtual const vec2 GetPosition() const
        {
            return _transform.position + _transform.localPosition;
        }
        virtual void SetPosition(const vec2& position);
        
        virtual const vec2 GetAnchor() const
        {
            return _transform.anchor;
        }
        virtual void SetAnchor(const vec2& anchor);

        virtual const vec2 GetLocalAnchor() const
        {
            return _transform.localAnchor;
        }
        virtual void SetLocalAnchor(const vec2& localAnchor);
        
        virtual const vec2 GetSize() const
        {
            return _transform.size;
        }
        virtual void SetSize(const vec2& size);
        
        virtual const bool GetFillParentSize()
        {
            return _transform.fillParentSize;
        }
        virtual void SetFillParentSize(bool fillParent);

        virtual const u16 GetDepth() const
        {
            return _transform.sortData.depth;
        }
        virtual void SetDepth(const u16 depth);

        virtual void SetParent(asUITransform* parent);
        
        virtual void SetExpandBoundsToChildren(bool expand);

        const bool IsVisible() const { return _visibility.visible && _visibility.parentVisible; }
        const bool IsLocallyVisible() const { return _visibility.visible; }
        const bool IsParentVisible() const { return _visibility.parentVisible; }
        virtual void SetVisible(bool visible);
    
        void SetCollisionEnabled(bool enabled);

        virtual void Destroy();

protected:
        static void MarkDirty(entt::registry* registry, entt::entity entId);
    
        static void UpdateChildTransforms(entt::registry* uiRegistry, UITransform& parent);
        static void UpdateChildTransformsAngelScript(UI::UIDataSingleton& uiDataSingleton, UITransform& parent);

        static void UpdateChildVisibility(entt::registry* uiRegistry, const UITransform& parent, bool parentVisibility);
        static void UpdateChildVisibilityAngelScript(UI::UIDataSingleton& uiDataSingleton, const UITransform& parent, bool parentVisibility);

        static void UpdateBounds(entt::registry* uiRegistry, UITransform& transform);
        static void ShallowUpdateBounds(entt::registry* uiRegistry, UITransform& parent);

    protected:
        entt::entity _entityId;
        UIElementType _elementType;

        UITransform _transform;
        UIVisibility _visibility;
    };
}
