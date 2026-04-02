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
    OperationMode curMode;     // 当前全系统运行模式 (互斥控制)
    SystemStatus status;      // 全局业务状态 (SYS_READY, SYS_DISCHARGING 等)
    float currentWeights[20]; // 各斗实时重量
    bool stableNodes[20];     // 各斗稳定状态
    float lastBatchWeight;    // 最近一次组合成功的总重
    uint32_t selectionMask;   // 下料位掩码
    int scanProgress;         // 当前轮扫描进度 0-20
    int currentScanCycle;     // 当前重试轮次 0-4
    bool onlineNodes[20];     // 实时在线状态 (兼容原有逻辑)
    bool scanResults[5][20];  // 5轮扫描的完整历史记录
    
    // --- 诊断模式状态 ---
    uint8_t diagLastSent;     // 最后一次发送的递增字节
    char diagRxHex[128];      // 最近接收到的 16 进制字符串显示
};

/**
 * @brief 整合上下文，作为系统的状态中心
 */
struct SystemContext {
    ProductionParams config;
    RuntimeState state;
};

#endif // SYSTEM_CONTEXT_H
