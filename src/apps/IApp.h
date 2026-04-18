#ifndef I_APP_H
#define I_APP_H

#include "system/SystemTypes.h"

/**
 * @class IApp
 * @brief 抽象应用实例接口。
 * 系统中每个独立业务场景（如生产、扫描、诊断）都需实现此接口。
 */
class IApp {
public:
    virtual ~IApp() {}

    // 进入应用：初始化所需资源
    virtual void onEnter() = 0;

    // 主循环逻辑：由 Dispatcher 调度
    virtual void onLoop() = 0;

    // 退出应用：清理或保存状态
    virtual void onExit() = 0;

    // 请求取消操作（软取消，供 UI 触发）
    virtual void requestCancel() {}

    // 获取当前应用对应的模式
    virtual OperationMode getMode() const = 0;

    // 任务是否已完成（用于自动切回生产模式）
    virtual bool isFinished() const { return false; }

    // 是否有进度需要展示在 UI 上
    virtual bool hasUIProgress() { return false; }

    // 获取进度百分比 (0-100)
    virtual int getUIProgress() { return 0; }
};

#endif // I_APP_H
