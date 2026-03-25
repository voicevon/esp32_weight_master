# ESP32 Weighing Master: Multi-tasking Architecture Analysis

## 1. Current Architecture Oversight
The system currently operates on a single-loop asynchronous model (Event Loop). While `ModbusMaster` is non-blocking, it still shares CPU time with UI rendering and logic calculations on a single core.

### Real-time Constraints:
- **Modbus (RS485)**: Requires consistent polling and timely handling of UART interrupts.
- **OLED UI**: Rendering a full 128x64 display via I2C takes ~20-40ms, which can cause jitter in Modbus polling.
- **Encoder**: Handled via ISR (Interrupt Service Routine), which is already high-priority.

---

## 2. Multi-tasking (Dual-Core FreeRTOS) Tradeoffs

### Benefits (+)
1. **Parallelism**: We can isolate the **Control Logic & Modbus (Core 1)** from the **HMI & Rendering (Core 0)**. Rendering the screen will no longer delay Modbus packet handling.
2. **Deterministic Timing**: Real-time control loops (e.g., motor position updates) can be given higher priority and guaranteed scan times.
3. **Responsive UI**: The HMI can run at a steady 30FPS without being throttled by slow RS485 responses or complex combination searches.

### Risks (-)
1. **Race Conditions**: Shared variables like `targetMin`, `targetMax`, and `cachedWeights` must be protected by **Mutexes** or **Semaphores**.
2. **Synchronization Overhead**: Inter-task communication (Queues/Message Buffers) adds slight latency.
3. **Complexity**: Debugging stack overflows or deadlock conditions becomes harder in a multi-tasking environment.
4. **Memory Usage**: Each task requires its own stack (typically 2KB-4KB), which consumes SRAM.

---

## 3. Implementation Proposal (Dual-Core)

| Task Name | Core | Priority | Description |
| :--- | :---: | :---: | :--- |
| **ControlTask** | 1 | 10 (High) | Modbus polling, Combination Engine, Motor control. |
| **UITask** | 0 | 5 (Med) | OLED Rendering, Menu navigation, Parameter editing. |
| **SystemTask** | 0 | 1 (Low) | NVS storage, Serial logging, Diagnostics. |

### Data Protection Strategy:
- **Global Config**: Use a `PortMUX` or `Mutex` for any variable that both the UI (Core 0) and Logic (Core 1) modify (e.g., `targetMin`).
- **Weight Cache**: Use a "Snapshot" pattern or Mutex to prevent the UI from reading a half-updated 32-bit float.

---

## 4. Troubleshooting the Node Scan (N81/Timing)
The failure to discover nodes during the initial scan may be due to:
- **Baud Rate Shift**: At 115200bps, even minor clock deviations can cause errors.
- **Task Preemption**: If the I2C OLED update is happening exactly when the UART interrupt fires, we might lose a byte.
- **Logic Level/Drive Capacity**: Ensure the RE/DE pins on the RS485 transceivers are switching fast enough.

### Recommendation
**Move to Multi-tasking immediately.** By isolating Modbus on a dedicated core, we eliminate "Display-induced" communication failures.

---
*FENGS WEIGH SCALE Systems Department*
