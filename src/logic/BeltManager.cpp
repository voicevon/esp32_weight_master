#include "BeltManager.h"
#include <Arduino.h>

/*

485单段相对定位指令
PA-4=0 PA-14=3 PA53=1 P3-30=2 P3-38=28 P3-39=27 P4-0=1 
上述参数设置好后重启伺服

圈数设置： 0x0202 设置值可正可负
圈内脉冲数：0x0203 设置值可正可负
运行速度： 0x0204 只能为正
位置启动为沿信号触发，所以要先写0后写1。圈数和圈内脉冲数计算出来是负数，则反转。
位置1圈数： 0x0202 设置值可正可负
位置1脉冲数： 0x0203 设置值可正可负
位置1运行速度：0x0204 只能为正
点动速度： 0x0015 只能为正 位置计算说明见案例5
启动位置1： 0x011F先写0，在写1 启动位置2：0x011F先写0在写17
位置暂停： 0x011F写2 继续位置1：0x011F写1
正向点动： 0x011F写4 反向点动： 0x011F写8 停止点动0x011F写0
位置不可叠加，必须位置1走完才能触发位置2，点动运行需在伺服停止后才能执行
位置2圈数： 0x0205 设置值可正可负
位置2脉冲数： 0x0206 设置值可正可负
位置2运行速度：0x0207 只能为正

启动位置：0x011F先写0，再写1 暂停：0x011F写2 继续位置：0x011F写1
电机运行过程中执行暂停指令，电机停止运行，再次触发启动位置会把上次没走完的位置走完
*/

// 伺服电机寄存器宏定义 (十六进制地址)
#define REG_POS1_REV     0x0202  // 内部位置指令 1 的位置圈数
#define REG_POS1_PULSE   0x0203  // 内部位置指令 1 的位置圈内脉冲数
#define REG_POS1_SPEED   0x0204  // 内部位置指令控制 1 的移动速度
#define REG_VIRTUAL_IO   0x011F  // 虚拟输入端子状态值 (P3-31)

// 假设默认的编码器单圈脉冲数为 10000
#define PULSES_PER_REV   10000

// 皮带诊断专用的机械转换率宏定议：假设 1 毫米 = 100 脉冲 (可根据需要调整)
#define PULSES_PER_MM    100

/**
 * @brief 构造函数：初始化皮带电机 ID
 */
BeltManager::BeltManager(ModbusMaster* rs485, int motorId1, int motorId2)
    : _rs485(rs485), _id1(motorId1), _id2(motorId2) {}

void BeltManager::begin() {
    // 初始化逻辑：目前伺服重启后已根据物理参数配置为增量式位置指令模式 (P4-0=1)
}

void BeltManager::moveRelative(uint8_t id, int32_t pulses) {
    if (!_rs485) return;
    
    // 拆分圈数和圈内脉冲数
    int16_t revs = pulses / PULSES_PER_REV;
    int16_t pls  = pulses % PULSES_PER_REV;
    
    // 空回调函数：仅触发异步发送，不需要处理返回结果
    auto nullCb = [](Modbus::ResultCode res, uint16_t tid, void* data) { return true; };

    // 1. 设置指令 1 的相对圈数和脉冲数
    _rs485->asyncWrite(id, REG_POS1_REV, (uint16_t)revs, nullCb);
    _rs485->asyncWrite(id, REG_POS1_PULSE, (uint16_t)pls, nullCb);
    
    // 2. 触发虚拟端子运行 (0x011F 先写 0 复位沿信号，再写 1 触发位置 1)
    _rs485->asyncWrite(id, REG_VIRTUAL_IO, 0, nullCb);
    _rs485->asyncWrite(id, REG_VIRTUAL_IO, 1, nullCb);
}

void BeltManager::collectFromUnits(int32_t pulses) {
    // 一级带相对位移
    moveRelative(_id1, pulses);
}

void BeltManager::advanceOutput(int32_t pulses) {
    // 二级带相对位移
    moveRelative(_id2, pulses);
}

bool BeltManager::isMoving() {
    // 未来可通过查询状态寄存器实现，这里维持非阻塞快速返回
    return false;
}

void BeltManager::moveDistanceMm(uint8_t id, int32_t mm) {
    if (id <= 0) return;
    int32_t pulses = mm * PULSES_PER_MM;
    moveRelative(id, pulses);
}
