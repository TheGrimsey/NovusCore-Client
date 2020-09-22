#include "BaseElement.h"
#include <tracy/Tracy.hpp>
#include "../../Utils/ServiceLocator.h"

#include "../ECS/Components/Singletons/UIDataSingleton.h"
#include "../ECS/Components/Singletons/UIEntityPoolSingleton.h"
#include "../ECS/Components/Singletons/UILockSingleton.h"

#include "../ECS/Components/Transform.h"
#include "../ECS/Components/SortKey.h"
#include "../ECS/Components/Visibility.h"
#include "../ECS/Components/Visible.h"
#include "../ECS/Components/Collision.h"
#include "../ECS/Components/Collidable.h"
#include "../ECS/Components/Dirty.h"
#include "../ECS/Components/BoundsDirty.h"
#include "../Utils/TransformUtils.h"
#include "../Utils/SortUtils.h"
#include "../Utils/VisibilityUtils.h"

namespace UIScripting
{
    BaseElement::BaseElement(UI::UIElementType elementType, bool collisionEnabled) : _elementType(elementType)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        _entityId = registry->ctx<UISingleton::UIEntityPoolSingleton>().GetId();
        registry->ctx<UISingleton::UIDataSingleton>().entityToElement[_entityId] = this;

        // Set up base components.
        UIComponent::Transform* transform = &registry->emplace<UIComponent::Transform>(_entityId);
        transform->asObject = this;

        UIComponent::SortKey* sortKey = &registry->emplace<UIComponent::SortKey>(_entityId);
        sortKey->data.entId = _entityId;
        sortKey->data.type = _elementType;

        registry->emplace<UIComponent::Visibility>(_entityId);
        registry->emplace<UIComponent::Visible>(_entityId);

        UIComponent::Collision* collision = &registry->emplace<UIComponent::Collision>(_entityId);
        if (collisionEnabled)
        {
            collision->SetFlag(UI::CollisionFlags::COLLISION);
            registry->emplace<UIComponent::Collidable>(_entityId);
        }
    }

    vec2 BaseElement::GetScreenPosition() const
    {
        const auto transform = &ServiceLocator::GetUIRegistry()->get<UIComponent::Transform>(_entityId);
        return UIUtils::Transform::GetScreenPosition(transform);
    }
    vec2 BaseElement::GetLocalPosition() const
    {
        const auto transform = &ServiceLocator::GetUIRegistry()->get<UIComponent::Transform>(_entityId);
        return transform->parent == entt::null ? vec2(0, 0) : transform->localPosition;
    }
    vec2 BaseElement::GetParentPosition() const
    {
        const auto transform = &ServiceLocator::GetUIRegistry()->get<UIComponent::Transform>(_entityId);
        return transform->parent == entt::null ? vec2(0, 0) : transform->position;
    }
    void BaseElement::SetPosition(const vec2& position)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        if (transform->parent == entt::null)
            transform->position = position;
        else
            transform->localPosition = position;

        UIUtils::Transform::UpdateChildTransforms(registry, transform);
    }

    vec2 BaseElement::GetSize() const
    {
        const auto transform = &ServiceLocator::GetUIRegistry()->get<UIComponent::Transform>(_entityId);
        return transform->size;
    }
    void BaseElement::SetSize(const vec2& size)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        // Early out if we are just filling parent size.
        if (transform->HasFlag(UI::TransformFlags::FILL_PARENTSIZE))
            return;
        transform->size = size;

        UIUtils::Transform::UpdateChildTransforms(registry, transform);
    }

    void BaseElement::SetTransform(const vec2& position, const vec2& size)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        if (transform->parent == entt::null)
            transform->position = position;
        else
            transform->localPosition = position;

        if (!transform->HasFlag(UI::TransformFlags::FILL_PARENTSIZE))
            transform->size = size;

        UIUtils::Transform::UpdateChildTransforms(registry, transform);
    }

    vec2 BaseElement::GetAnchor() const
    {
        const auto transform = &ServiceLocator::GetUIRegistry()->get<UIComponent::Transform>(_entityId);
        return transform->anchor;
    }
    void BaseElement::SetAnchor(const vec2& anchor)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        if (transform->anchor == hvec2(anchor))
            return;
        transform->anchor = anchor;

        if (transform->parent != entt::null)
        {
            auto dataSingleton = &registry->ctx<UISingleton::UIDataSingleton>();
            std::shared_lock pl(dataSingleton->GetMutex(transform->parent));

            transform->position = UIUtils::Transform::GetAnchorPosition(&registry->get<UIComponent::Transform>(transform->parent), anchor);
        }

        UIUtils::Transform::UpdateChildTransforms(registry, transform);
    }

    vec2 BaseElement::GetLocalAnchor() const
    {
        const auto transform = &ServiceLocator::GetUIRegistry()->get<UIComponent::Transform>(_entityId);
        return transform->localAnchor;
    }
    void BaseElement::SetLocalAnchor(const vec2& localAnchor)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        if (transform->localAnchor == hvec2(localAnchor))
            return;
        transform->localAnchor = localAnchor;

        UIUtils::Transform::UpdateChildTransforms(registry, transform);
    }

    bool BaseElement::GetFillParentSize() const
    {
        const auto transform = &ServiceLocator::GetUIRegistry()->get<UIComponent::Transform>(_entityId);
        return transform->HasFlag(UI::TransformFlags::FILL_PARENTSIZE);
    }
    void BaseElement::SetFillParentSize(bool fillParent)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        if (transform->HasFlag(UI::TransformFlags::FILL_PARENTSIZE) == fillParent)
            return;

        if (fillParent)
            transform->SetFlag(UI::TransformFlags::FILL_PARENTSIZE);
        else
            transform->UnsetFlag(UI::TransformFlags::FILL_PARENTSIZE);

        if (transform->parent == entt::null)
            return;

        auto parentTransform = &registry->get<UIComponent::Transform>(transform->parent);
        transform->size = parentTransform->size;

        UIUtils::Transform::UpdateChildTransforms(registry, transform);
    }

    UI::DepthLayer BaseElement::GetDepthLayer() const
    {
        const auto sortKey = &ServiceLocator::GetUIRegistry()->get<UIComponent::SortKey>(_entityId);
        return sortKey->data.depthLayer;
    }
    void BaseElement::SetDepthLayer(const UI::DepthLayer layer)
    {
        auto sortKey = &ServiceLocator::GetUIRegistry()->get<UIComponent::SortKey>(_entityId);
        sortKey->data.depthLayer = layer;
    }

    u16 BaseElement::GetDepth() const
    {
        const auto sortKey = &ServiceLocator::GetUIRegistry()->get<UIComponent::SortKey>(_entityId);
        return sortKey->data.depth;
    }
    void BaseElement::SetDepth(const u16 depth)
    {
        auto sortKey = &ServiceLocator::GetUIRegistry()->get<UIComponent::SortKey>(_entityId);
        sortKey->data.depth = depth;
    }

    void BaseElement::SetParent(BaseElement* parent)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        if (transform->parent == parent->GetEntityId())
            return;

        if (transform->parent != entt::null)
        {
            NC_LOG_ERROR("Tried calling SetParent() on Element(ID: %d, Type: %d) with a parent. You must call UnsetParent() first.", entt::to_integral(_entityId), _elementType)
            return;
        }

        transform->parent = parent->GetEntityId();
        
        auto parentTransform = &registry->get<UIComponent::Transform>(transform->parent);
        // Add us as parent's child.
        parentTransform->children.push_back({ _entityId, _elementType });

        // Update position. Keeping relative.
        hvec2 Origin = UIUtils::Transform::GetAnchorPosition(parentTransform, transform->anchor);
        transform->localPosition = transform->position - Origin;
        transform->position = Origin;

        // Handle fillParentSize
        if (transform->HasFlag(UI::TransformFlags::FILL_PARENTSIZE))
            transform->size = parentTransform->size;

        // Update our and children's depth. Keeping the relative offsets for all children but adding onto it how much we moved in depth.
        auto sortKey = &registry->get<UIComponent::SortKey>(_entityId);
        auto parentSortKey = &registry->get<UIComponent::SortKey>(transform->parent);

        i16 difference = parentSortKey->data.depth - sortKey->data.depth + 1;
        sortKey->data.depth = parentSortKey->data.depth + 1;
        sortKey->data.depthLayer = parentSortKey->data.depthLayer;

        UIUtils::Sort::UpdateChildDepths(registry, _entityId, difference);
        UIUtils::Transform::UpdateChildTransforms(registry, transform);
    }
    void BaseElement::UnsetParent()
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto transform = &registry->get<UIComponent::Transform>(_entityId);

        if (transform->parent == entt::null)
            return;

        UIUtils::Transform::RemoveChild(registry, transform->parent, _entityId);
    }

    bool BaseElement::GetExpandBoundsToChildren() const
    {
        const auto collision = &ServiceLocator::GetUIRegistry()->get<UIComponent::Collision>(_entityId);
        return collision->HasFlag(UI::CollisionFlags::INCLUDE_CHILDBOUNDS);
    }
    void BaseElement::SetExpandBoundsToChildren(bool expand)
    {
        auto collision = &ServiceLocator::GetUIRegistry()->get<UIComponent::Collision>(_entityId);

        if (collision->HasFlag(UI::CollisionFlags::INCLUDE_CHILDBOUNDS) == expand)
            return;

        if (expand)
            collision->SetFlag(UI::CollisionFlags::INCLUDE_CHILDBOUNDS);
        else
            collision->UnsetFlag(UI::CollisionFlags::INCLUDE_CHILDBOUNDS);
    }

    bool BaseElement::IsVisible() const
    {
        const UIComponent::Visibility* visibility = &ServiceLocator::GetUIRegistry()->get<UIComponent::Visibility>(_entityId);
        return UIUtils::Visibility::IsVisible(visibility);
    }
    bool BaseElement::IsLocallyVisible() const
    {
        const UIComponent::Visibility* visibility = &ServiceLocator::GetUIRegistry()->get<UIComponent::Visibility>(_entityId);
        return visibility->visible;
    }
    bool BaseElement::IsParentVisible() const
    {
        const UIComponent::Visibility* visibility = &ServiceLocator::GetUIRegistry()->get<UIComponent::Visibility>(_entityId);
        return visibility->parentVisible;
    }
    void BaseElement::SetVisible(bool visible)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto visibility = &registry->get<UIComponent::Visibility>(_entityId);

        if (visibility->visible == visible)
            return;
        const bool visibilityChanged = (visibility->parentVisible && visibility->visible) != (visibility->parentVisible && visible);
        visibility->visible = visible;
        
        if (!visibilityChanged)
            return;

        const bool newVisibility = UIUtils::Visibility::IsVisible(visibility);
        UIUtils::Visibility::UpdateChildVisibility(registry, _entityId, newVisibility);

        registry->ctx<UISingleton::UIDataSingleton>().visibilityToggleQueue.enqueue(_entityId);
    }

    void BaseElement::SetCollisionEnabled(bool enabled)
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        auto collision = &ServiceLocator::GetUIRegistry()->get<UIComponent::Collision>(_entityId);
        if (collision->HasFlag(UI::CollisionFlags::COLLISION) == enabled)
            return;

        if (enabled)
            collision->SetFlag(UI::CollisionFlags::COLLISION);
        else
            collision->UnsetFlag(UI::CollisionFlags::COLLISION);

        registry->ctx<UISingleton::UIDataSingleton>().collisionToggleQueue.enqueue(_entityId);
    }

    void BaseElement::Destroy(bool destroyChildren)
    {
        ServiceLocator::GetUIRegistry()->ctx<UISingleton::UIDataSingleton>().DestroyElement(_entityId, destroyChildren);
    }

    void BaseElement::MarkDirty()
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        registry->ctx<UISingleton::UIDataSingleton>().dirtyQueue.enqueue(_entityId);

        const auto transform = &registry->get<UIComponent::Transform>(_entityId);
        UIUtils::Transform::MarkChildrenDirty(registry, transform);
    }

    void BaseElement::MarkSelfDirty()
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        registry->ctx<UISingleton::UIDataSingleton>().dirtyQueue.enqueue(_entityId);
    }

    void BaseElement::MarkBoundsDirty()
    {
        entt::registry* registry = ServiceLocator::GetUIRegistry();
        registry->ctx<UISingleton::UIDataSingleton>().dirtyBoundsQueue.enqueue(_entityId);
    }
}
