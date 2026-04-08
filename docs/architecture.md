# Architecture

## Layer Diagram

```mermaid
graph TD
    subgraph UI["UI Layer (ImGui / ImPlot)"]
        TX["TX Panel\n(ImGui)"]
        RX["RX Panel\n(ImGui)"]
        Plot["Plot Panel\n(ImPlot)"]
        CellViz["Cell Viz\n(ImDrawList)"]
    end

    subgraph DBC["DBC Model (dbcppp)"]
        DBCParser["DBC Parser\nmessages · signals · factor/offset/range"]
    end

    subgraph CAN["CAN Interface (abstract)"]
        direction LR
        PCAN["PCAN-Basic\n(Peak Hardware)"]
        Virtual["Virtual / Loopback\n(dev/test)"]
    end

    TX --> DBCParser
    RX --> DBCParser
    Plot --> DBCParser
    CellViz --> DBCParser
    DBCParser --> CAN
```

---

## Panels

| Panel | Library | Description |
|---|---|---|
| **TX Panel** | ImGui | Lists all DBC messages the BMU would normally transmit. Each signal has an editable input field. Messages are sent at a configurable cycle rate. |
| **RX Panel** | ImGui | Displays incoming CAN messages decoded against DBC signal definitions — shows signal name, scaled value, and unit. |
| **Plot Panel** | ImPlot | User selects signals from a list; selected signals are rendered as scrolling time-series graphs with pan, zoom, and legend. |
| **Cell Temp Visualization** | ImDrawList | *(Planned)* Grid of cells color-coded by temperature. Hover shows a per-cell popup with detailed info. |
| **Cell Voltage Visualization** | ImDrawList | *(Planned)* Same grid layout, color-coded by voltage. |
| **Cell Balancing Visualization** | ImDrawList | *(Planned)* Grid showing active/inactive balancing state per cell. |

---

## Threading Model

```mermaid
sequenceDiagram
    participant HW as PCAN Hardware
    participant RX as CAN RX Thread
    participant Buf as Ring Buffer (mutex)
    participant UI as UI Thread (main)

    loop Every CAN frame
        HW->>RX: Raw CAN frame
        RX->>RX: Decode via dbcppp
        RX->>Buf: Push decoded signal values
    end

    loop Every render frame (~60 Hz)
        UI->>Buf: Drain pending values
        Buf-->>UI: Decoded signal values
        UI->>UI: Update plots & panels
    end
```

- The **CAN RX thread** runs independently of the UI, blocking on `CAN_Read` from PCAN-Basic.
- Decoded signal values are pushed into a **thread-safe ring buffer** (mutex-guarded).
- The **UI thread** drains the buffer at the start of each render frame to keep display data fresh without blocking rendering.

---

## Cell Visualization Design *(Planned)*

Each cell visualization panel shares the same grid layout concept:

```mermaid
graph LR
    Grid["Cell Grid\n(rows × cols = physical cells)"]
    Grid -->|color encodes value| Color["Heat-map color\n(green → yellow → red)"]
    Grid -->|hover| Popup["Per-cell popup\n(index, value, unit, status)"]
```

- Grid dimensions are configured to match the physical cell module layout.
- Color mapping uses a configurable min/max range per visualization type.
- The same `ImDrawList` helper will be reused across all three cell panels.
