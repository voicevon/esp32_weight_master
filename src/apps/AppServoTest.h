#ifndef APP_SERVO_TEST_H
#define APP_SERVO_TEST_H

#include "apps/IApp.h"
#include "system/SystemContext.h"
#include "drivers/ModbusMaster.h"

/**
 * @class AppServoTest
 * @brief 舵机手动维护应用。
 * 允许在 Admin 界面手动控制 1-20 号节点的称重斗门开关。
 */
class AppServoTest : public IApp {
public:
    AppServoTest(SystemContext* ctx, ModbusMaster* rs485);

    void onEnter() override;
    void onLoop() override;
    void onExit() override;
    bool isFinished() const override { return false; }
    OperationMode getMode() const override { return MODE_SERVO_TEST; }

    // 指令接口
    void triggerServo(int id, bool open);

private:
    SystemContext* _ctx;
    ModbusMaster*  _rs485;
};

#endif
