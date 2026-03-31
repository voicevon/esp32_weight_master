#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>
#include <vector>
#include "SystemTypes.h"
#include "SystemConfig.h"

/**
 * @brief 系统生产设置结构体
 * 记录用户在 HMI 端修改的所有持久化参数
 */
struct ProductionParams {
    float targetMin;
    float targetMax;
    float accumulatedWeight;
    bool isProductionEnabled;
};

/**
 * @brief 系统实时状态快照
 * 记录当前的运行动态，主要供 UI 渲染使用
 */
struct RuntimeState {
    SystemStatus status;
    float currentWeights[20]; // 各斗实时重量
    bool stableNodes[20];     // 各斗稳定状态
    float lastBatchWeight;    // 最近一次组合成功的总重
    uint32_t selectionMask;   // 下料位掩码
};

/**
 * @brief 整合上下文，作为系统的状态中心
 */
struct SystemContext {
    ProductionParams config;
    RuntimeState state;
};

#endif // SYSTEM_CONTEXT_H
