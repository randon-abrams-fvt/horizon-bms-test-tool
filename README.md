# Horizon BMS — BMU Simulator Application

A Windows GUI application that simulates a Battery Management Unit (BMU) for subpack BMS development and testing. The simulator reads DBC files to enumerate CAN messages and signals, allowing the user to:

- Transmit BMU messages with per-signal configurable values
- Receive and decode BMS messages against DBC signal definitions
- Plot signals over time with pan/zoom
- Visualize module-based cell temperature, voltage, and balancing status

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

---

## Current UI Highlights

- `Signals` tab with sub-tabs for:
	- `Control` (BMU command TX + BMS state RX)
	- `Cell Temperatures`
	- `Cell Voltages`
	- `Cell Balancing`
- Module-oriented grid layout:
	- 2 thermistor values per module on the temperature page
	- 12 cell voltages per module on the voltage page
	- 12 cell balancing states/currents per module on the balancing page
- Balancing indicator lights per cell:
	- Blue = commanded to balance
	- Green = actively balancing
- Global `Settings` window for text scale, theme, docking, and ImGui debug windows
