#pragma once

#include <cstdint>
#include <string>
#include <vector>

class DbcModel;
class CanBus;

/// Panel for composing and transmitting BMU CAN messages.
/// Shows a list of DBC messages on the left; the right pane exposes
/// per-signal sliders, a cyclic send toggle, and a manual send button.
class TxPanel
{
  public:
    void setDbc(const DbcModel *dbc);
    void setCanBus(CanBus *bus);
    void render();

  private:
    void rebuildFromDbc();
    void transmit(size_t msgIdx);

    struct SigState
    {
        std::string name;
        std::string unit;
        float value = 0.0f;
        float min = 0.0f;
        float max = 1.0f;
    };

    struct MsgState
    {
        uint32_t id;
        std::string name;
        uint8_t dlc;
        std::vector<SigState> signals;
        bool cyclicEnabled = false;
        float cycleMs = 100.0f;
        double lastSentMs = 0.0;
    };

    const DbcModel *dbc_ = nullptr;
    CanBus *bus_ = nullptr;
    std::vector<MsgState> messages_;
    int selected_ = -1;
};
