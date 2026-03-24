#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <Adafruit_SSD1306.h>
#include <AiEsp32RotaryEncoder.h>
#include "Display.h"
#include "MenuSystem.h"
#include "system/Rs485Master.h"

enum UIState {
    SPLASH_SCREEN,
    DASHBOARD_SCREEN,
    MENU_SCREEN,
    DETAIL_SCREEN,
    EDIT_SCREEN
};

class UserInterface {
private:
    static UserInterface* instance;
    UserInterface();

    std::vector<Display*> _displays;
    MenuSystem _menu;
    AiEsp32RotaryEncoder* _encoder;
    
    UIState _state = SPLASH_SCREEN;
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
    void initialize(int clk, int dt, int sw, float* target, float* tolerance, Rs485Master* rs485);
    void addDisplay(Display* display);
    
    void update(const std::vector<float>& weights, const String& status);
    
private:
    void setupMenuTree();
    void handleInput();
};

#endif
