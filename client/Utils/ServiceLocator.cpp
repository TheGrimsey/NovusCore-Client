#include "ServiceLocator.h"

entt::registry* ServiceLocator::_gameRegistry = nullptr;
entt::registry* ServiceLocator::_uiRegistry = nullptr;
MessageHandler* ServiceLocator::_authSocketMessageHandler = nullptr;
MessageHandler* ServiceLocator::_gameSocketMessageHandler = nullptr;
Window* ServiceLocator::_window = nullptr;
InputManager* ServiceLocator::_inputManager = nullptr;
ClientRenderer* ServiceLocator::_clientRenderer = nullptr;
CameraFreelook* ServiceLocator::_camera = nullptr;
Renderer::Renderer* ServiceLocator::_renderer = nullptr;
SceneManager* ServiceLocator::_sceneManager = nullptr;

moodycamel::ConcurrentQueue<Message>* ServiceLocator::_mainInputQueue = nullptr;

void ServiceLocator::SetGameRegistry(entt::registry* registry)
{
    assert(_gameRegistry == nullptr);
    _gameRegistry = registry;
}
void ServiceLocator::SetUIRegistry(entt::registry* registry)
{
    assert(_uiRegistry == nullptr);
    _uiRegistry = registry;
}
void ServiceLocator::SetAuthSocketMessageHandler(MessageHandler* messageHandler)
{
    assert(_authSocketMessageHandler == nullptr);
    _authSocketMessageHandler = messageHandler;
}
void ServiceLocator::SetGameSocketMessageHandler(MessageHandler* messageHandler)
{
    assert(_gameSocketMessageHandler == nullptr);
    _gameSocketMessageHandler = messageHandler;
}
void ServiceLocator::SetWindow(Window* window)
{
    assert(_window == nullptr);
    _window = window;
}

void ServiceLocator::SetInputManager(InputManager* inputManager)
{
    assert(_inputManager == nullptr);
    _inputManager = inputManager;
}

void ServiceLocator::SetClientRenderer(ClientRenderer* clientRenderer)
{
    assert(_clientRenderer == nullptr);
    _clientRenderer = clientRenderer;
}

void ServiceLocator::SetCamera(CameraFreelook* camera)
{
    assert(_camera == nullptr);
    _camera = camera;
}

void ServiceLocator::SetMainInputQueue(moodycamel::ConcurrentQueue<Message>* mainInputQueue)
{
    assert(_mainInputQueue == nullptr);
    _mainInputQueue = mainInputQueue;
}

void ServiceLocator::SetRenderer(Renderer::Renderer* renderer)
{
    assert(_renderer == nullptr);
    _renderer = renderer;
}

void ServiceLocator::SetSceneManager(SceneManager* sceneManager)
{
    assert(_sceneManager == nullptr);
    _sceneManager = sceneManager;
}
