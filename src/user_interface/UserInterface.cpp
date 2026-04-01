#include "UserInterface.h"
#include <Preferences.h>

UserInterface* UserInterface::instance = nullptr;

UserInterface::UserInterface() : _state(SCREEN_SPLASH), _stateStartTime(millis()), _currentMode(MODE_PRODUCTION) {}

UserInterface* UserInterface::getInstance() {
    if (!instance) instance = new UserInterface();
    return instance;
}

void UserInterface::initialize(SystemContext* ctx, ModbusMaster* rs485) {
    Encoder::getInstance()->initialize();
    
    _ctx = ctx;
    _rs485 = rs485;
    
    setupMenuTree();
}

void UserInterface::addDisplay(Display* display) {
    _displays.push_back(display);
    display->begin();
}

void UserInterface::setupMenuTree() {
    MenuNode* root = new MenuNode("主菜单");
    MenuNode* commands = new MenuNode("系统命令", root);
    MenuNode* diagConfig = new MenuNode("诊断与设置", root);

    // 1. Dashboard
    root->addItem(MenuItem("1. 仪表盘", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_PRODUCTION;
        _state = SCREEN_MAIN;
    }));

    // 2. System Commands Submenu
    commands->addItem(MenuItem("1. 全局置零", MENU_TYPE_ACTION, nullptr, [this](){
        _rs485->broadcastTare();
        _messageBoxText = "广播置零指令已发送";
        _messageTimer = millis();
        _state = SCREEN_MESSAGE;
    }));
    commands->addItem(MenuItem("2. 逐个置零", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_SEQUENTIAL_CTRL;
        _state = SCREEN_SEQUENTIAL_PROGRESS;
        _sequentialProgress = 0;
        _currentSequentialCmd = 3;
        _sequentialLabel = "正在逐个置零...";
        _lastSequentialStepTime = millis();
    }));
    commands->addItem(MenuItem("3. 逐个打开", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_SEQUENTIAL_CTRL;
        _state = SCREEN_SEQUENTIAL_PROGRESS;
        _sequentialProgress = 0;
        _currentSequentialCmd = 1;
        _sequentialLabel = "正在逐个打开...";
        _lastSequentialStepTime = millis();
    }));
    commands->addItem(MenuItem("4. 逐个关闭", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_SEQUENTIAL_CTRL;
        _state = SCREEN_SEQUENTIAL_PROGRESS;
        _sequentialProgress = 0;
        _currentSequentialCmd = 2;
        _sequentialLabel = "正在逐个关闭...";
        _lastSequentialStepTime = millis();
    }));
    commands->addItem(MenuItem("5. 清除累计", MENU_TYPE_ACTION, nullptr, [this](){
        _ctx->config.accumulatedWeight = 0;
        _messageBoxText = "累计重量已清零";
        _messageTimer = millis();
        _state = SCREEN_MESSAGE;
    }));
    commands->addItem(MenuItem("6. < 返回", MENU_TYPE_BACK));
    root->addItem(MenuItem("2. 系统命令", MENU_TYPE_SUBMENU, commands));

    // 3. Diagnosis & Configuration Submenu
    diagConfig->addItem(MenuItem("1. 扫描节点", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_SCAN;
        _state = SCREEN_SCAN;
        _rs485->startScan();
    }));
    diagConfig->addItem(MenuItem("2. 节点状态", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_DETAIL;
        _state = SCREEN_DETAIL;
        _selectedNode = 1;
    }));
    diagConfig->addItem(MenuItem("3. 总线测试", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_PULSE;
        _state = SCREEN_RS485_DIAG;
        _diagRxCount = 0;
        _rs485->resetStats();
    }));
    diagConfig->addItem(MenuItem("4. 目标最低值", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 0;
    }));
    diagConfig->addItem(MenuItem("5. 目标最高值", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 1;
    }));
    diagConfig->addItem(MenuItem("6. < 返回", MENU_TYPE_BACK));
    root->addItem(MenuItem("3. 诊断与设置", MENU_TYPE_SUBMENU, diagConfig));

    // 4. About
    root->addItem(MenuItem("4. 关于", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_ABOUT;
        _state = SCREEN_MAIN;
    }));

    _menu.setRootMenu(root);
}

void UserInterface::update(const std::vector<float>& weights, float stableSum, float unstableSum, int unstableCount, float totalSum, float accumulatedWeight, uint32_t whitelistMask, SystemStatus status, uint32_t selectionMask) {
    handleInput();
    
    // RS485 诊断屏幕下的 1Hz 脉冲与 RX 监听逻辑
    if (_state == SCREEN_RS485_DIAG) {
        if (millis() - _lastPulseTime >= 1000) {
            _lastPulseTime = millis();
            _rs485->sendRawByte(_diagTxByte++);
        }
        
        // 实时读取 RX 原始字节
        while (_rs485->availableRaw()) {
            _diagRxByte = _rs485->readRawByte();
            _diagRxCount++;
        }
    }

    if (millis() - _lastUpdate < 30) return; // Slightly faster for smoother scrolling
    _lastUpdate = millis();

    // 1. Scan Mode Logic (Auto-cycling and History)
    if (_state == SCREEN_SCAN) {
        int progress = _rs485->getScanProgress();
        bool isScanning = _rs485->isScanning();

        // Capture results when a scan cycle is COMPLETED
        // We detect the transition from scanning to finished
        bool scanJustFinished = (!isScanning && _lastScanFinishTime == 0 && progress >= 20); // 恢复全量扫描 1-20

        if (scanJustFinished) {
            _totalScans++;
            ScanRow row;
            const bool* currentStatus = _rs485->getOnlineStatusArray();
            for (int id = 1; id <= 20; id++) {
                row.online[id] = currentStatus[id];
            }
            _scanHistory.push_back(row);
            
            if (_scanHistory.size() > 50) _scanHistory.erase(_scanHistory.begin());
            
            // Adjust scroll target to keep the latest 4 rows in view
            if (_scanHistory.size() > 4) {
                _targetScrollY = (_scanHistory.size() - 4) * 12; // 12 is rowHeight in OLEDDisplay
            }
            
            _lastScanFinishTime = millis(); // Mark as finished to prevent multiple captures
        }

        // Smooth scroll interpolation
        _currentScrollY += (_targetScrollY - _currentScrollY) * 0.15f;

        // Auto-restart scan loop (5 seconds after finish)
        if (!isScanning) {
            if (_lastScanFinishTime != 0 && millis() - _lastScanFinishTime > 5000) {
                _rs485->startScan();
                _lastScanFinishTime = 0;
            }
        }
    }

    // 2. Sequential Action Logic (State-Driven & Whitelist-Aware)
    if (_state == SCREEN_SEQUENTIAL_PROGRESS) {
        // 如果当前 Modbus 正在忙碌（发送或等待），则不进行下一步
        if (_rs485->getStatus() == ModbusMaster::ST_IDLE) {
            bool foundNext = false;
            while (_sequentialProgress < 20) {
                _sequentialProgress++;
                if (_rs485->isWhitelisted(_sequentialProgress)) {
                    foundNext = true;
                    break;
                }
            }
            
            if (foundNext) {
                // 核心规则：仅对白名单中的节点发起指令
                if (_currentSequentialCmd == 1) _rs485->openDischarge(_sequentialProgress);
                else if (_currentSequentialCmd == 2) _rs485->closeDischarge(_sequentialProgress);
                else if (_currentSequentialCmd == 3) _rs485->tare(_sequentialProgress);
                
                _lastSequentialStepTime = millis();
            } else {
                // 全部节点扫描/执行完毕
                _messageBoxText = "操作已全部完成";
                _messageTimer = millis();
                _state = SCREEN_MESSAGE;
            }
        }
    }

    // 3. Status Polling Push
    if (_currentMode == MODE_SEQUENTIAL_CTRL || _state == SCREEN_SEQUENTIAL_PROGRESS) {
        _rs485->update(); // Ensure communication flows during these operations
    }

    for (auto d : _displays) {
        d->clear();
        switch (_state) {
            case SCREEN_SPLASH:
                d->drawSplash(millis() - _stateStartTime);
                if (millis() - _stateStartTime > 4000) {
                    _state = SCREEN_MAIN;
                }
                break;
            case SCREEN_MAIN:
                if (_currentMode == MODE_ABOUT) {
                    d->drawAbout("v1.5.0", __DATE__);
                } else {
                    d->drawDashboard(weights, stableSum, unstableSum, unstableCount, totalSum, accumulatedWeight, whitelistMask, _ctx->config.targetMin, _ctx->config.targetMax - _ctx->config.targetMin, status, selectionMask);
                }
                break;
            case SCREEN_MENU: {
                auto node = _menu.getCurrentNode();
                std::vector<String> items;
                for (auto& item : node->items) items.push_back(item.label);
                d->drawMenu(node->title, items, _menu.getCursorIndex(), _menu.getScrollOffset());
                break;
            }
            case SCREEN_DETAIL:
                d->drawNodeDetail(_selectedNode, weights[_selectedNode-1], _rs485->isNodeOnline(_selectedNode));
                break;
            case SCREEN_EDIT:
                if (_editParamIdx == 0) d->drawParamEdit("最小值", _ctx->config.targetMin, _ctx->config.targetMax, true);
                else d->drawParamEdit("最大值", _ctx->config.targetMax, _ctx->config.targetMin, false);
                break;
            case SCREEN_RS485_DIAG:
                d->drawRs485Diag(_diagTxByte, _diagRxByte, _diagRxCount);
                break;
            case SCREEN_SCAN:
                d->drawScan(_rs485->getScanProgress(), !_rs485->isScanning(), _scanHistory, _currentScrollY, _totalScans);
                break;
            case SCREEN_MESSAGE:
                d->drawMessage(_messageBoxText);
                if (millis() - _messageTimer > 2000) { // Auto-return after 2s
                    _state = SCREEN_MENU;
                }
                break;
            case SCREEN_SEQUENTIAL_PROGRESS:
                d->drawSequentialProgress(_sequentialLabel, _sequentialProgress, 20);
                break;
        }
        d->display();
    }
}

void UserInterface::handleInput() {
    int delta = Encoder::getInstance()->getDelta();
    bool clicked = Encoder::getInstance()->isClicked();

    switch (_state) {
        case SCREEN_MAIN:
            if (clicked) {
                _state = SCREEN_MENU;
                _menu.reset();
            }
            break;
        case SCREEN_MENU: {
            _menu.handleInput(delta, clicked);
            break;
        }
        case SCREEN_DETAIL:
            _selectedNode = constrain(_selectedNode + delta, 1, 20);
            if (clicked) {
                _state = SCREEN_MENU;
            }
            break;
        case SCREEN_EDIT:
            if (_editParamIdx == 0) {
                _ctx->config.targetMin = constrain(_ctx->config.targetMin + delta * 10.0f, 10.0f, _ctx->config.targetMax);
            } else {
                _ctx->config.targetMax = constrain(_ctx->config.targetMax + delta * 10.0f, _ctx->config.targetMin, 5000.0f);
            }
            
            if (clicked) {
                // 保存修改后的参数到 NVS (持久化)
                Preferences prefs;
                prefs.begin("production", false);
                prefs.putFloat("tmin", _ctx->config.targetMin);
                prefs.putFloat("tmax", _ctx->config.targetMax);
                prefs.end();
                
                _messageBoxText = "参数已保存";
                _messageTimer = millis();
                _state = SCREEN_MESSAGE;
            }
            break;
        case SCREEN_RS485_DIAG:
            if (clicked || delta != 0) {
                _state = SCREEN_MENU;
                _currentMode = MODE_IDLE;
            }
            break;
        case SCREEN_SCAN:
            if (clicked) {
                if (_rs485->isScanning()) {
                    _rs485->stopScan();
                    _currentMode = MODE_IDLE;
                    _state = SCREEN_MENU;
                } else {
                    _rs485->savePollWhitelist();
                    _messageBoxText = "扫描结果已保存并生效";
                    _messageTimer = millis();
                    _state = SCREEN_MESSAGE;
                }
            }
            break;
        case SCREEN_MESSAGE:
            if (clicked || delta != 0) {
                _state = SCREEN_MENU;
            }
            break;
        case SCREEN_SEQUENTIAL_PROGRESS:
            // Cannot escape easily until done or button clicked
            if (clicked) {
                _state = SCREEN_MENU;
                _currentMode = MODE_IDLE;
            }
            break;
        default: break;
    }
}
