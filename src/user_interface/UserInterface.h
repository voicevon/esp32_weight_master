#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <Adafruit_SSD1306.h>
#include <AiEsp32RotaryEncoder.h>
#include "Display.h"
#include "MenuSystem.h"
#include "system/Rs485Master.h"

enum OperationMode {
    MODE_STANDBY,
    MODE_PRODUCTION,
    MODE_DIAGNOSIS,
    MODE_CONFIGURATION,
    MODE_ABOUT
};

enum UIState {
    SCREEN_SPLASH,
    SCREEN_MAIN,
    SCREEN_MENU,
    SCREEN_DETAIL,
    SCREEN_EDIT
};

class UserInterface {
private:
    static UserInterface* instance;
    UserInterface();

    std::vector<Display*> _displays;
    MenuSystem _menu;
    AiEsp32RotaryEncoder* _encoder;
    
    OperationMode _currentMode = MODE_STANDBY;
    UIState _state = SCREEN_SPLASH;
    unsigned long _lastUpdate = 0;
    unsigned long _stateStartTime = 0;
    unsigned long _lastButtonTime = 0;

    // System pointers
    float* _targetWeight;
    float* _tolerance;
    Rs485Master* _rs485;

    // Detail/Edit context
    int _selectedNode = 1;
    int _editParamIdx = 0; // 0: Target, 1: Tolerance

public:
    static UserInterface* getInstance();
    void initialize(float* target, float* tolerance, Rs485Master* rs485);
    void addDisplay(Display* display);
    
    void update(const std::vector<float>& weights, const String& status);
    
    void setMode(OperationMode mode) { _currentMode = mode; }
    OperationMode getMode() const { return _currentMode; }
    
private:
    void setupMenuTree();
    void handleInput();
};

#endif
