# Horizon BMS — BMU Simulator Application

A Windows GUI application that simulates a Battery Management Unit (BMU) for subpack BMS development and testing. The simulator reads DBC files to enumerate CAN messages and signals, allowing the user to:

- Transmit BMU messages with per-signal configurable values
- Receive and decode BMS messages against DBC signal definitions
- Plot signals over time with pan/zoom
- Visualize cell-level temperature, voltage, and balancing status *(planned)*

---

## Documentation

| Document | Description |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Layer diagram, panel descriptions, threading model |
| [docs/design-decisions.md](docs/design-decisions.md) | Library choices, build system, open decisions |

---

## Repository Layout

```
horizon-bms-bmu-sim-app/
├── docs/                  # Design documentation
├── src/                   # Application source
├── third_party/           # Git submodules (ImGui, ImPlot, dbcppp, PCAN SDK)
├── CMakeLists.txt
└── README.md
```
