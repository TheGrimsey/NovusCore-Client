#include "ConfigLoader.h"
#include "../../Utils/ServiceLocator.h"
#include "../../ECS/Components/Singletons/ConfigSingleton.h"

#include <CVar/CVarSystem.h>
#include <entt.hpp>

const fs::path ConfigLoader::configFolderPath = fs::path("Data/configs").make_preferred();
const fs::path ConfigLoader::cvarConfigPath = (configFolderPath / "CVarConfig.json").make_preferred();
const fs::path ConfigLoader::uiConfigPath = (configFolderPath / "UIConfig.json").make_preferred();

bool ConfigLoader::Init(entt::registry* registry)
{
    ConfigSingleton& configSingleton = registry->set<ConfigSingleton>();

    if (!fs::exists(configFolderPath))
        fs::create_directory(configFolderPath);

    bool loadingFailed = false;

    JsonConfig& cvarConfig = configSingleton.cvarJsonConfig;
    {
        json defaultConfig;

        // Default CVar Config
        {
            defaultConfig["integer"] = json::array();
            defaultConfig["double"] = json::array();
            defaultConfig["string"] = json::array();
            defaultConfig["vector4"] = json::array();
            defaultConfig["ivector4"] = json::array();
        }

        bool didLoadOrCreate = cvarConfig.LoadOrCreate(cvarConfigPath, defaultConfig);
        if (didLoadOrCreate)
        {
            CVarSystem* cvarSystem = CVarSystem::Get();

            json& config = cvarConfig.GetConfig();
            cvarSystem->LoadCVarsFromJson(config);
            cvarSystem->LoadCVarsIntoJson(config);
        }

        loadingFailed |= !didLoadOrCreate;
    }

    JsonConfig& uiConfig = configSingleton.uiJsonConfig;
    {
        json defaultConfig;

        // Default CVar Config
        {
            defaultConfig["defaultMap"] = "None";
        }

        bool didLoadOrCreate = uiConfig.LoadOrCreate(uiConfigPath, defaultConfig);
        if (didLoadOrCreate)
        {
            json& config = uiConfig.GetConfig();
            configSingleton.uiConfig = config;
        }

        loadingFailed |= !didLoadOrCreate;
    }

    return !loadingFailed;
}

bool ConfigLoader::Save(ConfigSaveType saveType)
{
    entt::registry* registry = ServiceLocator::GetGameRegistry();
    ConfigSingleton& configSingleton = registry->ctx<ConfigSingleton>();

    bool savingFailed = false;
    bool savingAll = saveType == ConfigSaveType::ALL;

    if (savingAll || saveType == ConfigSaveType::CVAR)
    {
        json& config = configSingleton.cvarJsonConfig.GetConfig();
        CVarSystem::Get()->LoadCVarsIntoJson(config);
        savingFailed |= !configSingleton.cvarJsonConfig.Save(cvarConfigPath);
    }

    if (savingAll || saveType == ConfigSaveType::UI)
    {
        json& config = configSingleton.uiJsonConfig.GetConfig();
        config = configSingleton.uiConfig;

        savingFailed |= !configSingleton.uiJsonConfig.Save(uiConfigPath);
    }

    return !savingFailed;
}
