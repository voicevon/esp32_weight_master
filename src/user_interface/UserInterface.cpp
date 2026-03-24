#include "UserInterface.h"

UserInterface* UserInterface::instance = nullptr;

UserInterface::UserInterface() : _state(SCREEN_SPLASH), _stateStartTime(millis()) {}

UserInterface* UserInterface::getInstance() {
    if (!instance) instance = new UserInterface();
    return instance;
}

void UserInterface::initialize(float* target, float* tolerance, Rs485Master* rs485) {
    _encoder = new AiEsp32RotaryEncoder(ENCODER_A, ENCODER_B, ENCODER_BUTTON, -1, 4);
    _encoder->begin();
    _encoder->setup([]() {});
    _encoder->setBoundaries(0, 1000, false);
    
    _targetWeight = target;
    _tolerance = tolerance;
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

    // 1. Production Mode
    root->addItem(MenuItem("1. START PRODUCTION", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_PRODUCTION;
        _state = SCREEN_MAIN;
        _encoder->setBoundaries(0, 5000, false);
        _encoder->setEncoderValue((long)(*_targetWeight * 10));
    }));

    // 2. Standby / Stop
    root->addItem(MenuItem("2. STOP / STANDBY", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_STANDBY;
        _state = SCREEN_MAIN;
    }));

    // 3. Diagnosis Submenu
    diag->addItem(MenuItem("Node Status", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_DIAGNOSIS;
        _state = SCREEN_DETAIL;
        _selectedNode = 1;
        _encoder->setBoundaries(1, 20, true);
        _encoder->setEncoderValue(1);
    }));
    diag->addItem(MenuItem("< Back", MENU_TYPE_BACK));
    root->addItem(MenuItem("3. DIAGNOSIS", MENU_TYPE_SUBMENU, diag));

    // 4. Configure Submenu
    config->addItem(MenuItem("Target Weight", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 0;
        _encoder->setBoundaries(100, 5000, false);
        _encoder->setEncoderValue((long)(*_targetWeight * 10));
    }));
    config->addItem(MenuItem("Tolerance", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_CONFIGURATION;
        _state = SCREEN_EDIT;
        _editParamIdx = 1;
        _encoder->setBoundaries(0, 500, false);
        _encoder->setEncoderValue((long)(*_tolerance * 10));
    }));
    config->addItem(MenuItem("< Back", MENU_TYPE_BACK));
    root->addItem(MenuItem("4. CONFIGURE", MENU_TYPE_SUBMENU, config));

    // 5. About
    root->addItem(MenuItem("5. ABOUT", MENU_TYPE_ACTION, nullptr, [this](){
        _currentMode = MODE_ABOUT;
        _state = SCREEN_MAIN; // Case for About is handled in draw based on mode
    }));

    _menu.setRootMenu(root);
}

void UserInterface::update(const std::vector<float>& weights, const String& status) {
    handleInput();
    
    if (millis() - _lastUpdate < 50) return;
    _lastUpdate = millis();

    for (auto d : _displays) {
        d->clear();
        switch (_state) {
            case SCREEN_SPLASH:
                d->drawSplash();
                if (millis() - _stateStartTime > 2000) {
                    _state = SCREEN_MAIN;
                    _encoder->setBoundaries(0, 5000, false);
                    _encoder->setEncoderValue((long)(*_targetWeight * 10));
                }
                break;
            case SCREEN_MAIN:
                if (_currentMode == MODE_ABOUT) {
                    d->drawAbout("v1.2.5", __DATE__);
                } else {
                    String modeTag = (_currentMode == MODE_PRODUCTION) ? "[RUN]" : "[IDLE]";
                    d->drawDashboard(weights, *_targetWeight, *_tolerance, modeTag + " " + status);
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
                if (_editParamIdx == 0) d->drawParamEdit("Target", (float)_encoder->readEncoder()/10.0f);
                else d->drawParamEdit("Tolerance", (float)_encoder->readEncoder()/10.0f);
                break;
        }
        d->display();
    }
}

void UserInterface::handleInput() {
    long rawVal = _encoder->readEncoder();
    bool clicked = _encoder->isEncoderButtonClicked();

    // Lockout logic (250ms)
    if (clicked) {
        if (millis() - _lastButtonTime < 250) clicked = false;
        else _lastButtonTime = millis();
    }

    switch (_state) {
        case SCREEN_MAIN:
            if (clicked) {
                _state = SCREEN_MENU;
                _menu.reset();
            }
            break;
        case SCREEN_MENU: {
            static long lastVal = 0;
            // IMPORTANT: Initialize lastVal when first entering SCREEN_MENU to avoid delta spikes
            static UIState lastState = SCREEN_SPLASH;
            if (lastState != SCREEN_MENU) {
                lastVal = rawVal;
                lastState = SCREEN_MENU;
            }
            _menu.handleInput((int)(rawVal - lastVal), clicked);
            lastVal = rawVal;
            break;
        }
        case SCREEN_DETAIL:
            _selectedNode = (int)rawVal;
            if (clicked) {
                _state = SCREEN_MENU;
            }
            break;
        case SCREEN_EDIT:
            if (_editParamIdx == 0) *_targetWeight = (float)rawVal / 10.0f;
            else *_tolerance = (float)rawVal / 10.0f;
            if (clicked) {
                _state = SCREEN_MENU;
            }
            break;
        default: break;
    }
}
