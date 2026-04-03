#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>
#include <vector>
#include "SystemTypes.h"
#include "SystemConfig.h"

/**
 * @brief 整合上下文体
 */
struct SystemContext {
    ProductionParams config;
    ProductionState  prog;
    DiagContext      diag;
    UISnapshot       ui;
};

#endif // SYSTEM_CONTEXT_H
