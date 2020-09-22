#include "TransformUtils.h"
#include <tracy/Tracy.hpp>
#include <shared_mutex>
#include "entity/registry.hpp"
#include "../ECS/Components/Dirty.h"
#include "../ECS/Components/Singletons/UIDataSingleton.h"

namespace UIUtils::Transform
{   
    void UpdateChildTransforms(entt::registry* registry, UIComponent::Transform* parent)
    {
        ZoneScoped;
        //auto dataSingleton = &registry->ctx<UISingleton::UIDataSingleton>();
        for (const UI::UIChild& child : parent->children)
        {
            //std::lock_guard l(dataSingleton->GetMutex(child.entId));
            UIComponent::Transform* childTransform = &registry->get<UIComponent::Transform>(child.entId);

            childTransform->position = UIUtils::Transform::GetAnchorPosition(parent, childTransform->anchor);
            if (childTransform->HasFlag(UI::TransformFlags::FILL_PARENTSIZE))
                childTransform->size = parent->size;

            UpdateChildTransforms(registry, childTransform);
        }
    }

    void MarkChildrenDirty(entt::registry* registry, const UIComponent::Transform* transform)
    {
        auto dataSingleton = &registry->ctx<UISingleton::UIDataSingleton>();

        for (const UI::UIChild& child : transform->children)
        {
            //std::shared_lock l(dataSingleton->GetMutex(child.entId));

            const auto childTransform = &registry->get<UIComponent::Transform>(child.entId);
            MarkChildrenDirty(registry, childTransform);
            
            dataSingleton->dirtyQueue.enqueue(child.entId);
        }
    }
}