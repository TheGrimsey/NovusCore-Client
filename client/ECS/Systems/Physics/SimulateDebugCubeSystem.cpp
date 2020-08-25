#include "SimulateDebugCubeSystem.h"
#include <entt.hpp>
#include <InputManager.h>
#include <GLFW/glfw3.h>
#include <tracy/Tracy.hpp>
#include <Renderer/Renderer.h>
#include <InputManager.h>
#include <Math/Geometry.h>
#include "../../../Utils/ServiceLocator.h"
#include "../../../Utils/MapUtils.h"
#include "../../../Rendering/DebugRenderer.h"
#include "../../../Rendering/CameraFreelook.h"

#include "../../Components/Singletons/TimeSingleton.h"
#include "../../Components/Transform.h"
#include "../../Components/Physics/Rigidbody.h"
#include "../../Components/Rendering/DebugBox.h"

void SimulateDebugCubeSystem::Init(entt::registry& registry)
{
    InputManager* inputManager = ServiceLocator::GetInputManager();

    inputManager->RegisterKeybind("SpawnDebugBox", GLFW_KEY_B, KEYBIND_ACTION_PRESS, KEYBIND_MOD_ANY, [&registry](Window* window, std::shared_ptr<Keybind> keybind)
    {
        CameraFreelook* camera = ServiceLocator::GetCamera();

        // Create ENTT entity
        entt::entity entity = registry.create();

        Transform& transform = registry.emplace<Transform>(entity);
        transform.position = camera->GetPosition();
        transform.scale = vec3(0.5f, 1.2f, 0.5f); // "Ish" scale for humans
        transform.isDirty = true;

        registry.emplace<Rigidbody>(entity);
        registry.emplace<DebugBox>(entity);

        NC_LOG_MESSAGE("Spawned debug cube!");

        return true;
    });
}

void SimulateDebugCubeSystem::Update(entt::registry& registry, DebugRenderer* debugRenderer)
{
    TimeSingleton& timeSingleton = registry.ctx<TimeSingleton>();

    auto rigidbodyView = registry.view<Transform, Rigidbody>();
    rigidbodyView.each([&](const auto entity, Transform& transform)
    {
        // Make all rigidbodies "fall"
        f32 dist = GRAVITY_SCALE * timeSingleton.deltaTime;

        Geometry::AABoundingBox box;
        box.min = transform.position - transform.scale;
        box.max = transform.position + transform.scale;

        Geometry::Triangle triangle;
        f32 height = 0;

        // If we 
        f32 distToCollision = dist;

        if (Terrain::MapUtils::Intersect_AABB_TERRAIN_SWEEP(box, triangle, height, dist, distToCollision))
        {
            dist = distToCollision;
            registry.remove<Rigidbody>(entity);
        }

        transform.position.y -= dist;
    });

    auto debugCubeView = registry.view<Transform, DebugBox>();
    debugCubeView.each([&](const auto entity, Transform& transform)
    {
        vec3 min = transform.position - transform.scale;
        vec3 max = transform.position + transform.scale;

        u32 color = 0xff0000ff; // Red if it doesn't have a rigidbody
        if (registry.has<Rigidbody>(entity))
        {
            color = 0xff00ff00; // Green if it does
        }

        // This registers the model to be rendered THIS frame.
        debugRenderer->DrawAABB3D(min, max, color);
    });
}