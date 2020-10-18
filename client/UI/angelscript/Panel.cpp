#include "Panel.h"
#include "../../Scripting/ScriptEngine.h"
#include "../../Utils/ServiceLocator.h"

#include "../ECS/Components/Image.h"
#include "../ECS/Components/Renderable.h"

namespace UIScripting
{
    Panel::Panel(bool collisionEnabled) : BaseElement(UI::ElementType::UITYPE_PANEL, collisionEnabled)
    {
        ZoneScoped;
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        registry->emplace<UIComponent::TransformEvents>(_entityId);
        registry->emplace<UIComponent::Image>(_entityId);
        registry->emplace<UIComponent::Renderable>(_entityId).renderType = UI::RenderType::Image;
    }

    void Panel::RegisterType()
    {
        i32 r = ScriptEngine::RegisterScriptClass("Panel", 0, asOBJ_REF | asOBJ_NOCOUNT);
        r = ScriptEngine::RegisterScriptInheritance<BaseElement, Panel>("BaseElement");
        r = ScriptEngine::RegisterScriptFunction("Panel@ CreatePanel(bool collisionEnabled = true)", asFUNCTION(Panel::CreatePanel)); assert(r >= 0);

        // TransformEvents Functions
        r = ScriptEngine::RegisterScriptClassFunction("void SetEventFlag(int8 flags)", asMETHOD(Panel, SetEventFlag)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void UnsetEventFlag(int8 flags)", asMETHOD(Panel, UnsetEventFlag)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("bool IsClickable()", asMETHOD(Panel, IsClickable)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("bool IsDraggable()", asMETHOD(Panel, IsDraggable)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("bool IsFocusable()", asMETHOD(Panel, IsFocusable)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptFunctionDef("void PanelEventCallback(Panel@ panel)"); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void OnClick(PanelEventCallback@ cb)", asMETHOD(Panel, SetOnClickCallback)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void OnDragStarted(PanelEventCallback@ cb)", asMETHOD(Panel, SetOnDragStartedCallback)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void OnDragEnded(PanelEventCallback@ cb)", asMETHOD(Panel, SetOnDragEndedCallback)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void OnFocused(PanelEventCallback@ cb)", asMETHOD(Panel, SetOnFocusCallback)); assert(r >= 0);

        // Renderable Functions
        r = ScriptEngine::RegisterScriptClassFunction("string GetTexture()", asMETHOD(Panel, GetTexture)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void SetTexture(string Texture)", asMETHOD(Panel, SetTexture)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("Color GetColor()", asMETHOD(Panel, GetColor)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void SetColor(Color color)", asMETHOD(Panel, SetColor)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void SetSlicing(uint topOffset, uint rightOffset, uint bottomOffset, uint leftOffset)", asMETHOD(Panel, SetSlicing)); assert(r >= 0);
    }

    const bool Panel::IsClickable() const
    {
        const UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        return events->IsClickable();
    }
    const bool Panel::IsDraggable() const
    {
        const UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        return events->IsDraggable();
    }
    const bool Panel::IsFocusable() const
    {
        const UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        return events->IsFocusable();
    }

    void Panel::SetEventFlag(const UI::TransformEventsFlags flags)
    {
        UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        events->SetFlag(flags);
    }
    void Panel::UnsetEventFlag(const UI::TransformEventsFlags flags)
    {
        UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        events->UnsetFlag(flags);
    }

    void Panel::SetOnClickCallback(asIScriptFunction* callback)
    {
        UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        events->onClickCallback = callback;
        events->SetFlag(UI::TransformEventsFlags::UIEVENTS_FLAG_CLICKABLE);
    }

    void Panel::SetOnDragStartedCallback(asIScriptFunction* callback)
    {
        UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        events->onDragStartedCallback = callback;
        events->SetFlag(UI::TransformEventsFlags::UIEVENTS_FLAG_DRAGGABLE);
    }
    void Panel::SetOnDragEndedCallback(asIScriptFunction* callback)
    {
        UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        events->onDragEndedCallback = callback;
        events->SetFlag(UI::TransformEventsFlags::UIEVENTS_FLAG_DRAGGABLE);
    }

    void Panel::SetOnFocusCallback(asIScriptFunction* callback)
    {
        UIComponent::TransformEvents* events = &ServiceLocator::GetUIRegistry()->get<UIComponent::TransformEvents>(_entityId);
        events->onFocusedCallback = callback;
        events->SetFlag(UI::TransformEventsFlags::UIEVENTS_FLAG_FOCUSABLE);
    }

    const std::string& Panel::GetTexture() const
    {
        const UIComponent::Image* image = &ServiceLocator::GetUIRegistry()->get<UIComponent::Image>(_entityId);
        return image->style.texture;
    }
    void Panel::SetTexture(const std::string& texture)
    {
        UIComponent::Image* image = &ServiceLocator::GetUIRegistry()->get<UIComponent::Image>(_entityId);
        image->style.texture = texture;
    }

    const Color Panel::GetColor() const
    {
        const UIComponent::Image* image = &ServiceLocator::GetUIRegistry()->get<UIComponent::Image>(_entityId);
        return image->style.color;

    }
    void Panel::SetColor(const Color& color)
    {
        UIComponent::Image* image = &ServiceLocator::GetUIRegistry()->get<UIComponent::Image>(_entityId);
        image->style.color = color;
    }

    void Panel::SetSlicing(const u32 topOffset, const u32 rightOffset, const u32 bottomOffset, const u32 leftOffset)
    {
        UIComponent::Image* image = &ServiceLocator::GetUIRegistry()->get<UIComponent::Image>(_entityId);
        image->style.slicingOffset.top = topOffset;
        image->style.slicingOffset.right = rightOffset;
        image->style.slicingOffset.bottom = bottomOffset;
        image->style.slicingOffset.left = leftOffset;
    }

    Panel* Panel::CreatePanel(bool collisionEnabled)
    {
        Panel* panel = new Panel(collisionEnabled);

        return panel;
    }
}