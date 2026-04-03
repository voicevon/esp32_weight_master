#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>
#include <vector>
#include "SystemTypes.h"
#include "SystemConfig.h"

/**
 * @brief 系统生产设置 (持久化参数)
 */
struct ProductionParams {
    float targetMin;
    float targetMax;
    float accumulatedWeight;
    bool  isProductionEnabled;
};

/**
 * @brief 生产运行动态 (非持久化)
 */
struct ProductionState {
    SystemStatus status;          // READY, DISCHARGING...
    float        lastBatchWeight; // 最近成功的组合重量
    uint32_t     selectionMask;   // 下料掩码
};

/**
 * @brief 诊断与全系统扫描数据
 */
struct DiagContext {
    int     scanProgress;      // 0-20
    int     currentScanCycle;  // 0-4
    bool    scanResults[5][21]; // 1-based id index -> size 21
    uint8_t diagLastSent;
    char    diagRxHex[128];
};

/**
 * @brief UI 渲染快照 (无锁副本，由 uiLoop 填充)
 */
struct UISnapshot {
    OperationMode curMode;
    float         currentWeights[21]; // 1-20
    bool          stableNodes[21];
    bool          onlineNodes[21];
    bool          whitelistedNodes[21];
};

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
