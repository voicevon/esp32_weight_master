#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <Adafruit_SSD1306.h>
#include "Encoder.h"
#include "Display.h"
#include "MenuSystem.h"
#include "system/ModbusMaster.h"

#include "system/SystemTypes.h"

class UserInterface {
private:
    static UserInterface* instance;
    UserInterface();

    std::vector<Display*> _displays;
    MenuSystem _menu;
    
    OperationMode _currentMode = MODE_IDLE;
    UIState _state = SCREEN_SPLASH;
    unsigned long _lastUpdate = 0;
    unsigned long _stateStartTime = 0;
    unsigned long _lastButtonTime = 0;

    // System pointers
    float* _targetMin;
    float* _targetMax;
    ModbusMaster* _rs485;

    // Detail/Edit context
    int _selectedNode = 1;
    int _editParamIdx = 0; 
    uint8_t _diagTxByte = 0;
    uint8_t _diagRxByte = 0;
    uint32_t _diagRxCount = 0;
    unsigned long _lastPulseTime = 0;

public:
    static UserInterface* getInstance();
    void initialize(float* targetMin, float* targetMax, ModbusMaster* rs485);
    void addDisplay(Display* display);
    
    void update(const std::vector<float>& weights, const String& status);
    
    void setMode(OperationMode mode) { _currentMode = mode; }
    OperationMode getMode() const { return _currentMode; }
    
private:
    void setupMenuTree();
    void handleInput();
};

#endif
