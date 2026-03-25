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
}

void UserInterface::setupMenuTree() {
    MenuNode* root = new MenuNode("MAIN MENU");
    MenuNode* diag = new MenuNode("DIAGNOSIS", root);
    MenuNode* config = new MenuNode("CONFIGURE", root);

    // 1. Dashboard (Main Monitoring)
    root->addItem(MenuItem("1. DASHBOARD", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_PRODUCTION;
        _state = SCREEN_MAIN;
    }));

    // 2. Diagnosis Submenu
    diag->addItem(MenuItem("Node Status", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_DETAIL;
        _state = SCREEN_DETAIL;
        _selectedNode = 1;
    }));
    diag->addItem(MenuItem("RS485 Pulse Test", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_PULSE;
        _state = SCREEN_RS485_DIAG;
        _loopbackResult = -1;
        _rs485->resetStats();
    }));
    diag->addItem(MenuItem("Scan Nodes", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAG_SCAN;
        _state = SCREEN_SCAN;
        _rs485->startScan();
    }));
    diag->addItem(MenuItem("< Back", MENU_TYPE_BACK));
    root->addItem(MenuItem("2. DIAGNOSIS", MENU_TYPE_SUBMENU, diag));

    // 3. Configure Submenu
    config->addItem(MenuItem("Target Min", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 0;
    }));
    config->addItem(MenuItem("Target Max", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 1;
    }));
    config->addItem(MenuItem("< Back", MENU_TYPE_BACK));
    root->addItem(MenuItem("3. CONFIGURE", MENU_TYPE_SUBMENU, config));

    // 4. About
    root->addItem(MenuItem("4. ABOUT", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_ABOUT;
        _state = SCREEN_MAIN;
    }));

    _menu.setRootMenu(root);
}

void UserInterface::update(const std::vector<float>& weights, const String& status) {
    handleInput();
    
    // RS485 诊断屏幕下的 1Hz 脉冲逻辑
    if (_state == SCREEN_RS485_DIAG) {
        if (millis() - _lastPulseTime >= 1000) {
            _lastPulseTime = millis();
            _rs485->sendRawByte(_diagTxByte++);
        }
    }

    if (millis() - _lastUpdate < 50) return;
    _lastUpdate = millis();

    for (auto d : _displays) {
        d->clear();
        switch (_state) {
            case SCREEN_SPLASH:
                d->drawSplash();
                if (millis() - _stateStartTime > 2000) {
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
                if (_editParamIdx == 0) d->drawParamEdit("Min Target", *_targetMin);
                else d->drawParamEdit("Max Target", *_targetMax);
                break;
            case SCREEN_RS485_DIAG:
                d->drawRs485Diag(_rs485->getPacketsSent(), _rs485->getPacketsDropped(), _loopbackResult, _diagTxByte);
                break;
            case SCREEN_SCAN:
                d->drawScan(_rs485->getScanProgress(), !_rs485->isScanning(), _rs485->getOnlineStatusArray());
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
