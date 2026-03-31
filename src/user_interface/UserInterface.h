#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

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

    // System pointers
    float* _targetMin;
    float* _targetMax;
    float* _accumulatedTotalWeight; // 控制累计重量归零
    ModbusMaster* _rs485;

    // Detail/Edit context
    int _selectedNode = 1;
    int _editParamIdx = 0; 
    uint8_t _diagTxByte = 0;
    uint8_t _diagRxByte = 0;
    uint32_t _diagRxCount = 0;
    unsigned long _lastPulseTime = 0;

    // UI Feedback
    String _messageBoxText = "";
    unsigned long _messageTimer = 0;
    int _sequentialProgress = 0;
    int _currentSequentialCmd = 0; // 1:开, 2:关, 3:置零
    String _sequentialLabel = "";
    unsigned long _lastSequentialStepTime = 0;

    // Scan Mode specific
    std::vector<ScanRow> _scanHistory;
    unsigned long _lastScanFinishTime = 0;
    float _currentScrollY = 0;
    float _targetScrollY = 0;
    uint32_t _totalScans = 0;

public:
    static UserInterface* getInstance();
    void initialize(float* targetMin, float* targetMax, float* accumulatedTotalWeight, ModbusMaster* rs485);
    void addDisplay(Display* display);
    
    void update(const std::vector<float>& weights, float stableSum, float unstableSum, float totalSum, float accumulatedWeight, const String& status, uint32_t selectionMask = 0);
    
    void setMode(OperationMode mode) { _currentMode = mode; }
    OperationMode getMode() const { return _currentMode; }
    
private:
    void setupMenuTree();
    void handleInput();
};

#endif
