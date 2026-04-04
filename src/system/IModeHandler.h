#ifndef I_MODE_HANDLER_H
#define I_MODE_HANDLER_H

#include "SystemTypes.h"

/**
 * @class IModeHandler
 * @brief 抽象模式处理器接口。
 * 每个具体模式（如生产、扫描、诊断）都需实现此接口。
 */
class IModeHandler {
public:
    virtual ~IModeHandler() {}

    // 进入模式：初始化所需资源
    virtual void onEnter() = 0;

    // 主循环逻辑：由 AppController 调度
    virtual void onLoop() = 0;

    // 退出模式：清理或保存状态
    virtual void onExit() = 0;

    // 获取当前模式类型
    virtual OperationMode getMode() const = 0;
};

#endif // I_MODE_HANDLER_H
