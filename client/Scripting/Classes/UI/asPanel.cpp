#include "asPanel.h"
#include "../../ScriptEngine.h"
#include "../../../Utils/ServiceLocator.h"
#include "../../../UI/ImageUtilsTransactions.h"
#include "../../../UI/TransformEventUtilsTransactions.h"

#include "../../../ECS/Components/Singletons/ScriptSingleton.h"

namespace UI
{
    asPanel::asPanel() : asUITransform(UIElementType::UITYPE_PANEL) { }

    void asPanel::RegisterType()
    {
        i32 r = ScriptEngine::RegisterScriptClass("Panel", 0, asOBJ_REF | asOBJ_NOCOUNT);
        r = ScriptEngine::RegisterScriptInheritance<asUITransform, asPanel>("UITransform");
        r = ScriptEngine::RegisterScriptFunction("Panel@ CreatePanel()", asFUNCTION(asPanel::CreatePanel)); assert(r >= 0);

        // TransformEvents Functions
        r = ScriptEngine::RegisterScriptClassFunction("void SetEventFlag(int8 flags)", asMETHOD(asPanel, SetEventFlag)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void UnsetEventFlag(int8 flags)", asMETHOD(asPanel, UnsetEventFlag)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("bool IsClickable()", asMETHOD(asPanel, IsClickable)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("bool IsDraggable()", asMETHOD(asPanel, IsDraggable)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("bool IsFocusable()", asMETHOD(asPanel, IsFocusable)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptFunctionDef("void PanelEventCallback(Panel@ panel)"); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void OnClick(PanelEventCallback@ cb)", asMETHOD(asPanel, SetOnClickCallback)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void OnDragged(PanelEventCallback@ cb)", asMETHOD(asPanel, SetOnDragCallback)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void OnFocused(PanelEventCallback@ cb)", asMETHOD(asPanel, SetOnFocusCallback)); assert(r >= 0);

        // Renderable Functions
        r = ScriptEngine::RegisterScriptClassFunction("string GetTexture()", asMETHOD(asPanel, GetTexture)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void SetTexture(string Texture)", asMETHOD(asPanel, SetTexture)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("Color GetColor()", asMETHOD(asPanel, GetColor)); assert(r >= 0);
        r = ScriptEngine::RegisterScriptClassFunction("void SetColor(Color color)", asMETHOD(asPanel, SetColor)); assert(r >= 0);
    }

    void asPanel::SetOnClickCallback(asIScriptFunction* callback)
    {
        _events.onClickCallback = callback;
        _events.SetFlag(UITransformEventsFlags::UIEVENTS_FLAG_CLICKABLE);

        UI::TransformEventUtils::Transactions::SetOnClickCallbackTransaction(_entityId, callback);
    }

    void asPanel::SetOnDragCallback(asIScriptFunction* callback)
    {
        _events.onDraggedCallback = callback;
        _events.SetFlag(UITransformEventsFlags::UIEVENTS_FLAG_DRAGGABLE);

        UI::TransformEventUtils::Transactions::SetOnDragCallbackTransaction(_entityId, callback);
    }

    void asPanel::SetOnFocusCallback(asIScriptFunction* callback)
    {
        _events.onFocusedCallback = callback;
        _events.SetFlag(UITransformEventsFlags::UIEVENTS_FLAG_FOCUSABLE);

        UI::TransformEventUtils::Transactions::SetOnFocusCallbackTransaction(_entityId, callback);
    }

    void asPanel::SetTexture(const std::string& texture)
    {
        _image.texture = texture;

        UI::ImageUtils::Transactions::SetTextureTransaction(texture, _entityId);
    }
    void asPanel::SetColor(const Color& color)
    {
        _image.color = color;

        UI::ImageUtils::Transactions::SetColorTransaction(color, _entityId);
    }

    asPanel* asPanel::CreatePanel()
    {
        asPanel* panel = new asPanel();
        
        return panel;
    }
}