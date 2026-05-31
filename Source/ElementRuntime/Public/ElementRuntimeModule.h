#pragma once

#include "Modules/ModuleManager.h"

class FElementRuntimeModule final : public IModuleInterface {
public:
    void StartupModule() override;
    void ShutdownModule() override;
};
