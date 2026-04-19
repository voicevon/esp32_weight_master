#ifndef I_COMMAND_BUS_H
#define I_COMMAND_BUS_H

/**
 * @interface ICommandBus
 * @brief 命令总线接口，用于解耦 UI 层与应用业务层。
 * 
 * UIManager 通过此接口发送指令，而不直接依赖全局函数。
 * AppController 将实现此接口并注入到 UIManager。
 */
class ICommandBus {
public:
    virtual ~ICommandBus() = default;

    // --- 核心业务命令 ---
    virtual void cmdGlobalTare() = 0;
    virtual void cmdStartScan() = 0;
    virtual void cmdCancelScan() = 0;
    virtual void cmdUpdateTargetBase(float delta) = 0;
    virtual void cmdUpdateTargetOffset(float delta) = 0;
    virtual void cmdUpdateTargets(float dMin, float dMax) = 0;
    virtual void cmdToggleDiagnosis(bool active) = 0;
    virtual void cmdServoTest(int id, bool open) = 0;
    virtual void cmdGlobalServo(bool open) = 0;
    virtual void cmdBeltTest(int beltId, int distanceMm) = 0;
    virtual void cmdBeltRun(int beltId, bool run) = 0;
    virtual void cmdTriggerBeltScan() = 0;

    // 串口助手功能
    virtual void cmdSerialSendHex(const char* hexStr) = 0;
    virtual void cmdSerialToggleAuto(bool enable) = 0;
    virtual void cmdSetDiagSubMode(int mode) = 0;
    virtual void cmdSetDiagTarget(int id) = 0;
    virtual void cmdDiagAction(int actionId) = 0;

    virtual void updateOperationMode(OperationMode mode) = 0;
};

#endif // I_COMMAND_BUS_H
