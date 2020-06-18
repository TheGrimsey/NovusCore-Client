#pragma once
#include <NovusTypes.h>
#include <entt.hpp>

#include "asUITransform.h"

#include "../../../ECS/Components/UI/UITransformEvents.h"
#include "../../../ECS/Components/UI/UIInputField.h"

namespace UI
{
    class asLabel;
    class asPanel;

    class asInputField : public asUITransform
    {
    public:
        asInputField(entt::entity entityId);

        static void RegisterType();

        //InputField Functions
        void AppendInput(const std::string& input);
        void AppendInput(const char input) 
        {
            std::string strInput = "";
            strInput.append(1, input);
            AppendInput(strInput);
        }

        void RemovePreviousCharacter();
        void RemoveNextCharacter();

        void MovePointerLeft();
        void MovePointerRight();

        void SetWriteHeadPosition(u32 position);

        //Transform Functions.
        virtual void SetSize(const vec2& size);

        // TransformEvents Functions
        const bool IsFocusable() const { return _events.IsFocusable(); }
        void SetOnFocusCallback(asIScriptFunction* callback);
        void SetOnUnFocusCallback(asIScriptFunction* callback);

        //Label Functions
        void SetText(const std::string& text);
        const std::string& GetText() const;

        void SetTextColor(const Color& color);
        const Color& GetTextColor() const;

        void SetTextOutlineColor(const Color& outlineColor);
        const Color& GetTextOutlineColor() const;

        void SetTextOutlineWidth(f32 outlineWidth);
        const f32 GetTextOutlineWidth() const;

        void SetTextFont(std::string fontPath, f32 fontSize);

        static asInputField* CreateInputField();

    private:
        asLabel* _label;

        UITransformEvents _events;
        UIInputField _inputField;
    };
}