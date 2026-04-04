#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>
#include <vector>
#include "system/SystemTypes.h"
#include "system/SystemConfig.h"

/**
 * @brief 整合上下文体
 */
struct SystemContext {
    ProductionParams   config;
    WSProductionState  prog;
    UISnapshot       ui;
};

#endif // SYSTEM_CONTEXT_H
