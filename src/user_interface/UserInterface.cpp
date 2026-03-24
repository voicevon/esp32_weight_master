#include "UserInterface.h"

UserInterface* UserInterface::instance = nullptr;

UserInterface::UserInterface() : _state(SPLASH_SCREEN), _stateStartTime(millis()) {}

UserInterface* UserInterface::getInstance() {
    if (!instance) instance = new UserInterface();
    return instance;
}

void UserInterface::initialize(int clk, int dt, int sw, float* target, float* tolerance, Rs485Master* rs485) {
    _encoder = new AiEsp32RotaryEncoder(clk, dt, sw, -1, 4);
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

    // Diagnosis
    diag->addItem(MenuItem("Node Status", MENU_TYPE_ACTION, nullptr, [this](){
        _state = DETAIL_SCREEN;
        _selectedNode = 1;
        _encoder->setBoundaries(1, 20, true);
        _encoder->setEncoderValue(1);
    }));
    diag->addItem(MenuItem("Back", MENU_TYPE_BACK));

    // Configure
    config->addItem(MenuItem("Target Weight", MENU_TYPE_ACTION, nullptr, [this](){
        _state = EDIT_SCREEN;
        _editParamIdx = 0;
        _encoder->setBoundaries(100, 5000, false);
        _encoder->setEncoderValue((long)(*_targetWeight * 10));
    }));
    config->addItem(MenuItem("Tolerance", MENU_TYPE_ACTION, nullptr, [this](){
        _state = EDIT_SCREEN;
        _editParamIdx = 1;
        _encoder->setBoundaries(0, 500, false);
        _encoder->setEncoderValue((long)(*_tolerance * 10));
    }));
    config->addItem(MenuItem("Back", MENU_TYPE_BACK));

    root->addItem(MenuItem("1. Diagnosis", MENU_TYPE_SUBMENU, diag));
    root->addItem(MenuItem("2. Configure", MENU_TYPE_SUBMENU, config));
    root->addItem(MenuItem("3. Exit", MENU_TYPE_ACTION, nullptr, [this](){
        _state = DASHBOARD_SCREEN;
        _encoder->setBoundaries(0, 5000, false);
        _encoder->setEncoderValue((long)(*_targetWeight * 10));
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
            case SPLASH_SCREEN:
                d->drawSplash();
                if (millis() - _stateStartTime > 2000) {
                    _state = DASHBOARD_SCREEN;
                    _encoder->setBoundaries(0, 5000, false);
                    _encoder->setEncoderValue((long)(*_targetWeight * 10));
                }
                break;
            case DASHBOARD_SCREEN:
                d->drawDashboard(weights, *_targetWeight, *_tolerance, status);
                break;
            case MENU_SCREEN: {
                auto node = _menu.getCurrentNode();
                std::vector<String> items;
                for (auto& item : node->items) items.push_back(item.label);
                d->drawMenu(node->title, items, _menu.getCursorIndex(), _menu.getScrollOffset());
                break;
            }
            case DETAIL_SCREEN:
                d->drawNodeDetail(_selectedNode, weights[_selectedNode-1], _rs485->isNodeOnline(_selectedNode));
                break;
            case EDIT_SCREEN:
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
        case DASHBOARD_SCREEN:
            if (clicked) {
                _state = MENU_SCREEN;
                _menu.reset();
            }
            break;
        case MENU_SCREEN:
            // Since we use the raw encoder value for MenuSystem, we need to handle deltas
            static long lastVal = 0;
            _menu.handleInput((int)(rawVal - lastVal), clicked);
            lastVal = rawVal;
            break;
        case DETAIL_SCREEN:
            _selectedNode = (int)rawVal;
            if (clicked) {
                _state = MENU_SCREEN;
                // No reset needed, stay in Diagnosis menu
            }
            break;
        case EDIT_SCREEN:
            if (_editParamIdx == 0) *_targetWeight = (float)rawVal / 10.0f;
            else *_tolerance = (float)rawVal / 10.0f;
            if (clicked) {
                _state = MENU_SCREEN;
                // Stay in Configure menu
            }
            break;
        default: break;
    }
}
