#include "UserInterface.h"

UserInterface* UserInterface::instance = nullptr;

UserInterface::UserInterface() : _state(SCREEN_SPLASH), _stateStartTime(millis()) {}

UserInterface* UserInterface::getInstance() {
    if (!instance) instance = new UserInterface();
    return instance;
}

void UserInterface::initialize(float* targetMin, float* targetMax, ModbusMaster* rs485) {
    Encoder::getInstance()->initialize();
    
    _targetMin = targetMin;
    _targetMax = targetMax;
    _rs485 = rs485;
    
    setupMenuTree();
}

void UserInterface::addDisplay(Display* display) {
    _displays.push_back(display);
    display->begin();
}

void UserInterface::setupMenuTree() {
    MenuNode* root = new MenuNode("主菜单");
    MenuNode* diag = new MenuNode("诊断", root);
    MenuNode* config = new MenuNode("设置", root);

    // 1. Dashboard (Main Monitoring)
    root->addItem(MenuItem("1. 仪表盘", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_PRODUCTION;
        _state = SCREEN_MAIN;
    }));

    // 2. Diagnosis Submenu
    diag->addItem(MenuItem("1. 扫描节点", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_SCAN;
        _state = SCREEN_SCAN;
        _rs485->startScan();
    }));
    diag->addItem(MenuItem("2. 节点状态", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_DETAIL;
        _state = SCREEN_DETAIL;
        _selectedNode = 1;
    }));
    diag->addItem(MenuItem("3. 总线测试", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_PULSE;
        _state = SCREEN_RS485_DIAG;
        _diagRxCount = 0; // 重置接收计数
        _rs485->resetStats();
    }));
    diag->addItem(MenuItem("4. < 返回", MENU_TYPE_BACK));
    root->addItem(MenuItem("2. 诊断", MENU_TYPE_SUBMENU, diag));

    // 3. Configure Submenu
    config->addItem(MenuItem("1. 目标最小值", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 0;
    }));
    config->addItem(MenuItem("2. 目标最大值", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 1;
    }));
    config->addItem(MenuItem("3. < 返回", MENU_TYPE_BACK));
    root->addItem(MenuItem("3. 设置", MENU_TYPE_SUBMENU, config));

    // 4. About
    root->addItem(MenuItem("4. 关于", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_ABOUT;
        _state = SCREEN_MAIN;
    }));

    _menu.setRootMenu(root);
}

void UserInterface::update(const std::vector<float>& weights, const String& status) {
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
        bool scanJustFinished = (!isScanning && _lastScanFinishTime == 0 && progress >= 20);

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
                    d->drawAbout("v1.3.0", __DATE__);
                } else {
                    String modeTag = (_currentMode == MODE_PRODUCTION) ? "[RUN]" : "[IDLE]";
                    d->drawDashboard(weights, *_targetMin, *_targetMax - *_targetMin, modeTag + " " + status);
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
                if (_editParamIdx == 0) d->drawParamEdit("最小值", *_targetMin, *_targetMax, true);
                else d->drawParamEdit("最大值", *_targetMax, *_targetMin, false);
                break;
            case SCREEN_RS485_DIAG:
                d->drawRs485Diag(_diagTxByte, _diagRxByte, _diagRxCount);
                break;
            case SCREEN_SCAN:
                d->drawScan(_rs485->getScanProgress(), !_rs485->isScanning(), _scanHistory, _currentScrollY, _totalScans);
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
                *_targetMin = constrain(*_targetMin + delta * 10.0f, 10.0f, *_targetMax);
            } else {
                *_targetMax = constrain(*_targetMax + delta * 10.0f, *_targetMin, 5000.0f);
            }
            
            if (clicked) {
                _state = SCREEN_MENU;
            }
            break;
        case SCREEN_RS485_DIAG:
            if (clicked || delta != 0) {
                _state = SCREEN_MENU;
                _currentMode = MODE_IDLE;
            }
            break;
        case SCREEN_SCAN:
            if (clicked && !_rs485->isScanning()) {
                _state = SCREEN_MENU;
            }
            break;
        default: break;
    }
}
