#include "ModbusMaster.h"
#include <Arduino.h>
#include <Preferences.h>
#include "SystemConfig.h"

// Helper union for float to Modbus register conversion
union FloatConverter {
    float f;
    uint16_t r[2];
};

ModbusMaster* ModbusMaster::_instance = nullptr;

ModbusMaster::ModbusMaster(int rxPin, int txPin, int enPin, long baud)
    : _rxPin(rxPin), _txPin(txPin), _enPin(enPin), _baud(baud) {
    _instance = this;
    for (int i = 0; i < 21; i++) {
        _cachedWeights[i] = 0.0f;
        _isStable[i] = false;
        _doorPhases[i] = 0;
        _lastDataIds[i] = 0;
        _targetDataIds[i] = 0;
        _onlineStatus[i] = false;
        _pollWhitelist[i] = true; // 默认所有节点都在白名单
        _nodeStatus[i] = NODE_DIRTY;
        _failCounters[i] = 0;
        _nodeErrorStats[i] = 0;
        _nodeLastResult[i] = Modbus::EX_SUCCESS;
    }
    _mutexBus = xSemaphoreCreateMutex();
}

void ModbusMaster::begin() {
    Serial1.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
    _mb.begin(&Serial1, _enPin);
    _mb.master();
    loadPollWhitelist();
}

void ModbusMaster::update(OperationMode curMode) {
    _currentMode = curMode; // 统一状态记录
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(10)) != pdTRUE) return; 
    
    // 基础协议栈心跳 (只要不是 1Hz 原始脉冲诊断，就允许 Modbus 协议栈运行)
    if (curMode != MODE_DIAG_PULSE) {
        _mb.task(); 
    }

    // 根据模式决定资源分发策略
    switch (curMode) {
        case MODE_DIAG_PULSE:
            // 诊模式完全接管总线，不进行任何自动 Modbus 轮询
            _status = ST_IDLE;
            break;

        case MODE_DIAG_SCAN:
        case MODE_PRODUCTION:
            // 扫描模式和生产模式共用轮询状态机，但 ID 来源不同
            handlePollingCycle(curMode);
            break;

        case MODE_CONFIGURATION:
        case MODE_IDLE:
        default:
            // 配置菜单或其他后台模式下，建议停止自动轮询以腾出 CPU/总线资源
            _status = ST_IDLE;
            break;
    }
    
    xSemaphoreGive(_mutexBus);
}

void ModbusMaster::handlePollingCycle(OperationMode curMode) {
    // 1. 确定运行背景：是全量扫描模式还是普通生产模式
    bool isScanning = (curMode == MODE_DIAG_SCAN);
    
    switch (_status) {
        case ST_IDLE: { 
            // --- 进入空闲态：准备发起下一次通讯请求 ---
            _status = ST_WAITING;       // 立即标为等待状态，防止重复触发
            _lastPollTime = millis();    // 记录发起时间，用于超时判定
            _packetsSent++;             // 统计发送包总数
            
            // 2. 确定目标 ID
            uint8_t id = isScanning ? _scanProgress : _currentPollId;

            // 3. 生产模式下的白名单过滤逻辑
            // 如果不是在扫描，且当前 ID 不在白名单内，则顺延寻找下一个在白名单的节点
            if (!isScanning && !_pollWhitelist[_currentPollId]) {
                uint8_t nextId = _currentPollId;
                for (int i = 0; i < 20; i++) {
                    nextId = (nextId % 20) + 1; // ID 在 1-20 之间循环
                    if (_pollWhitelist[nextId]) break; // 找到白名单节点则停止
                }
                _currentPollId = nextId;
                id = _currentPollId;
            }

            // 4. 发出 Modbus 异步读取指令
            // 读取从 0x0000 开始的 6 个寄存器：重量(2), 状态(1), ADC(2), 数据ID(1)
            // 结果将异步回调给 cbPoll 函数处理
            _mb.readHreg(id, 0x0000, _tempRegs, 6, cbPoll);
            break;
        }

        case ST_WAITING:
            // --- 进入等待态：监控是否收到回复或超时 ---
            if (millis() - _lastPollTime > MODBUS_POLL_TIMEOUT_MS) {
                // 情况 A：发生超时（对方没回应）
                _status = ST_TIMEOUT;
                _packetsDropped++;              // 统计丢包/超时总数
                uint8_t id = isScanning ? _scanProgress : _currentPollId;
                _onlineStatus[id] = false;       // 标记此节点此刻掉线
                _failCounters[id]++;             // 连续失败计数加一
                
                if (isScanning) {
                    // 扫描模式下的步进逻辑
                    _scanHistory[_scanCycle][id] = false; // 记录本轮扫描失败
                    _scanProgress++;                      // 移动到下一个物理 ID
                    if (_scanProgress > 20) {             // 如果 20 个都碰过了
                        _scanCycle++;                     // 开启下一轮扫描（共 5 轮）
                        if (_scanCycle >= 5) {
                            generateWhitelistFromScan();  // 5 轮全部结束，生成最终白名单
                        } else {
                            _scanProgress = 1;            // 重置进度，开始新一轮
                        }
                    }
                } else {
                    // 生产模式下的步进逻辑：简单地跳到下一个 ID
                    _currentPollId = (_currentPollId % 20) + 1;
                }
                _status = ST_IDLE; // 超时处理完毕，重回空闲态等待下一次循环
            }
            break;

        case ST_SUCCESS: // 指令成功（由异步回调 cbPoll 设置）
        case ST_ERROR:   // 协议层错误（如 CRC 校验失败，由 cbPoll 设置）
        case ST_TIMEOUT: // 已在上面处理过了
            _status = ST_IDLE; // 状态复位，准备下一轮轮询
            break;

        default:
            break;
    }
}

bool ModbusMaster::cbPoll(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    ModbusMaster* instance = _instance;
    // 1. 获取当前背景：是否处于全量扫描模式
    bool isScanning = (instance->_currentMode == MODE_DIAG_SCAN);
    uint8_t id = isScanning ? instance->_scanProgress : instance->_currentPollId;

    if (event == Modbus::EX_SUCCESS) {
        // --- 情况 A：通讯成功，正确拿到了 6 个寄存器的数据 ---
        instance->_status = ST_SUCCESS;
        instance->_onlineStatus[id] = true;
        instance->_failCounters[id] = 0; // 成功后清除连续失败计数

        // 2. 解析重量 (寄存器 0, 1 -> Float)
        // 工业 Modbus 通常用两个 16 位寄存器拼成一个 32 位浮点数
        FloatConverter conv;
        conv.r[0] = instance->_tempRegs[0];
        conv.r[1] = instance->_tempRegs[1];
        instance->_cachedWeights[id] = conv.f;

        // 3. 解析节点状态与门相位 (寄存器 2)
        // 高位字节存放稳定标志，低位字节存放门开启/关闭的阶段信息
        uint16_t statusReg = instance->_tempRegs[2];
        instance->_isStable[id] = (statusReg >> 8) & 0x01;
        instance->_doorPhases[id] = (statusReg & 0xFF); 
        
        // 4. 读取新鲜度 ID (寄存器 5)
        // 用于判定从机是否已经处理了上一次的控制指令（比如去皮或下料）
        uint16_t currentDataId = instance->_tempRegs[5];
        instance->_lastDataIds[id] = currentDataId;

        // 5. [生产模式独有] 加强版数据新鲜度与状态机判定
        if (!isScanning) {
            NodeStatus currentStatus = instance->_nodeStatus[id];
            bool isHardwareStable = instance->_isStable[id];
            
            // 初次上电，同步 DataID
            if (instance->_targetDataIds[id] == 0) {
                instance->_targetDataIds[id] = currentDataId;
            }

            if (currentStatus == NODE_DIRTY) {
                // 如果是脏数据状态（刚下完料），强制同步新的 ID 开始监测
                instance->_targetDataIds[id] = currentDataId;
                instance->_nodeStatus[id] = NODE_REFRESHING;
            } else {
                // 只有满足：ID 更新 + 门关严 + 物理读数稳定，才判定为 NODE_STABLE (可参与组合)
                bool isFresh = (currentDataId == instance->_targetDataIds[id]);
                bool isDoorClosed = (instance->_doorPhases[id] == 0); 

                if (isFresh && isDoorClosed && isHardwareStable) {
                    instance->_nodeStatus[id] = NODE_STABLE;
                } else if (!isHardwareStable || !isFresh) {
                    instance->_nodeStatus[id] = NODE_REFRESHING;
                }
            }
        }
    } else {
        // --- 情况 B：通讯失败（报错或超时） ---
        // 为了防止串口日志刷屏，仅在原本在线、正在扫描或错误累积时打印日志
        if (instance->_onlineStatus[id] || isScanning || instance->_nodeErrorStats[id] % 100 == 0) {
            Serial.printf("[MB] ID %d ERROR: 0x%02X (Packets: %d/%d)\n", 
                          id, (uint8_t)event, instance->_packetsSent, instance->_packetsDropped);
        }
        instance->_status = ST_ERROR;
        instance->_onlineStatus[id] = false; // 标记节点离线
        instance->_nodeLastResult[id] = event;
        instance->_nodeErrorStats[id]++;     // 累加该节点的错误次数
    }

    // 6. 实时统计：计算当前总线上正在“抖动”不稳的节点数量
    instance->_unstableCount = 0;
    for (int i = 1; i <= 20; i++) {
        if (instance->_onlineStatus[i] && !instance->_isStable[i]) {
            instance->_unstableCount++;
        }
    }

    // 7. 步进逻辑 (ID 跳转)
    if (isScanning) {
        // 扫描模式：记录本轮此 ID 的生还情况
        instance->_scanHistory[instance->_scanCycle][id] = (event == Modbus::EX_SUCCESS);
        instance->_scanProgress++;
        if (instance->_scanProgress > 20) {
            // 到达 20 号节点，重置进度
            instance->_scanCycle++;
            if (instance->_scanCycle >= 5) {
                // 5 轮全部扫完，生成结果并自动退回到“配置”模式，防止持续盲扫
                instance->generateWhitelistFromScan();
                extern void updateOperationMode(OperationMode mode);
                updateOperationMode(MODE_CONFIGURATION);
            } else {
                instance->_scanProgress = 1; // 进入下一轮循环
            }
        }
    } else {
        // 生产模式：简单循环跳转到下一个 ID
        instance->_currentPollId = (instance->_currentPollId % 20) + 1;
    }

    return true; // 通知 Modbus 协议栈，回调处理已完成
}

float ModbusMaster::getWeight(int id) {
    if (id < 1 || id > 20) return 0.0f;
    return _cachedWeights[id];
}

bool ModbusMaster::isStable(int id) {
    if (id < 1 || id > 20) return false;
    return _isStable[id];
}

uint8_t ModbusMaster::getDoorPhase(int id) {
    if (id < 1 || id > 20) return 0;
    return _doorPhases[id];
}

bool ModbusMaster::isNodeOnline(int id) {
    if (id < 1 || id > 20) return false;
    return _onlineStatus[id];
}

void ModbusMaster::setNodeStatus(int id, NodeStatus s) {
    if (id >= 1 && id <= 20) _nodeStatus[id] = s;
}

bool ModbusMaster::execCmd(uint8_t id, std::function<bool()> startFunc) {
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    
    _status = ST_PENDING;
    if (!startFunc()) {
        xSemaphoreGive(_mutexBus);
        return false;
    }
    
    _status = ST_WAITING;
    bool result = waitTransaction(id);
    xSemaphoreGive(_mutexBus);
    return result;
}

bool ModbusMaster::waitTransaction(uint8_t id) {
    unsigned long start = millis();
    while (millis() - start < MODBUS_POLL_TIMEOUT_MS) {
        _mb.task();
        if (_status == ST_SUCCESS) return true;
        if (_status == ST_ERROR || _status == ST_TIMEOUT) return false;
        vTaskDelay(1);
    }
    return false;
}

bool ModbusMaster::cbSync(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    ModbusMaster* instance = _instance;
    if (event == Modbus::EX_SUCCESS) instance->_status = ST_SUCCESS;
    else instance->_status = ST_ERROR;
    return true;
}

bool ModbusMaster::openDischarge(int id) {
    return execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 1, cbSync); });
}

bool ModbusMaster::openDischarge1S(int id) {
    bool ok = execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 5, cbSync); });
    if (ok) {
        _targetDataIds[id] = _lastDataIds[id] + 1;
        setNodeStatus(id, NODE_DISCHARGING);
        Serial.printf("[FSM] Node %d Discharging. Expected ID: %d\n", id, _targetDataIds[id]);
    }
    return ok;
}

bool ModbusMaster::closeDischarge(int id) {
    return execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 2, cbSync); });
}

bool ModbusMaster::tare(int id) {
    return execCmd(id, [this, id](){ return _mb.writeHreg(id, 0x0100, 3, cbSync); });
}

bool ModbusMaster::broadcastTare() {
    // 广播不需要等待回复 (Modbus ID 0)
    if (xSemaphoreTake(_mutexBus, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    bool ok = _mb.writeHreg(0, 0x0100, 3); 
    xSemaphoreGive(_mutexBus);
    return ok;
}

bool ModbusMaster::setPosition(int id, long position, int speed) {
    // 假设从机有位置寄存器 (此处为示意)
    return execCmd(id, [this, id, position](){ 
        return _mb.writeHreg(id, 0x0200, (uint16_t)position, cbSync); 
    });
}

void ModbusMaster::startScan() {
    _currentMode = MODE_DIAG_SCAN;
    _scanProgress = 1;
    _scanCycle = 0;
    
    // 启动扫描前，重置历史在线状态，强制进入全量同步
    memset(_onlineStatus, 0, sizeof(_onlineStatus));
    memset(_pollWhitelist, 0, sizeof(_pollWhitelist));
    memset(_scanHistory, 0, sizeof(_scanHistory));
    
    Serial.println("[MB] Scan Started. All nodes cleared, polling 1-20...");
}

void ModbusMaster::stopScan() {
    // 统一通过 updateOperationMode 切换模式，此处仅为兼容保留
    _currentMode = MODE_IDLE; 
}

void ModbusMaster::generateWhitelistFromScan() {
    // 逻辑：5 次扫描中只要有 1 次成功就加入白名单 (提高鲁棒性，抵御瞬时抖动)
    int foundCount = 0;
    for (int i = 1; i <= 20; i++) {
        bool anyPass = false;
        for (int c = 0; c < 5; c++) {
            if (_scanHistory[c][i]) {
                anyPass = true;
                break;
            }
        }
        _pollWhitelist[i] = anyPass;
        if (anyPass) foundCount++;
    }
    Serial.printf("[SCAN] Completed. Found %d nodes online.\n", foundCount);
    savePollWhitelist();
}

void ModbusMaster::savePollWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", false);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        prefs.putBool(key, _pollWhitelist[i]);
    }
    prefs.end();
}

void ModbusMaster::loadPollWhitelist() {
    Preferences prefs;
    prefs.begin("modbus", true);
    for (int i = 1; i <= 20; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl%d", i);
        _pollWhitelist[i] = prefs.getBool(key, true);
    }
    prefs.end();
}

bool ModbusMaster::performLoopbackTest() {
    // 基础环回测试逻辑：向自己发送数据（由于是半双工 RS485，通常需要硬件闭环或特定节点配合）
    return false;
}

void ModbusMaster::sendRawByte(uint8_t byte) {
    Serial1.write(byte);
}

void ModbusMaster::clearRawBuffer() {
    while(Serial1.available()) Serial1.read();
}

int ModbusMaster::availableRaw() {
    return Serial1.available();
}

uint8_t ModbusMaster::readRawByte() {
    return Serial1.read();
}

uint32_t ModbusMaster::getWhitelistMask() const {
    uint32_t mask = 0;
    for (int i = 1; i <= 20; i++) {
        if (_pollWhitelist[i]) mask |= (1 << (i - 1));
    }
    return mask;
}
