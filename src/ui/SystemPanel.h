#pragma once

#include "can/CanBus.h"

#include <string>
#include <vector>

class DbcModel;

/// Tab 2 — left pane: system_commands TX controls (BMU → BMS).
///           right pane: system_status live decoded display (BMS → BMU).
/// Renders inline into the current tab item (no Begin/End window).
class SystemPanel
{
  public:
    void setDbc(const DbcModel *dbc);
    void setCanBus(CanBus *bus);
    void pushFrame(const DecodedFrame &frame);
    void render();

  private:
    void findMessages();
    void transmit();
    void renderCommands();
    void renderStatus();

    const DbcModel *dbc_ = nullptr;
    CanBus *bus_ = nullptr;

    // ── TX: system_commands ───────────────────────────────────────────────
    uint32_t cmdId_ = 0;
    bool cmdFound_ = false;

    bool operationReq_ = false;
    bool enableImd_ = false;
    bool serviceRequest_ = false;
    bool externalBusDisconnected_ = false;

    bool cyclicEnabled_ = false;
    float cycleMs_ = 100.0f;
    double lastSentMs_ = 0.0;

    // ── RX: system_status ─────────────────────────────────────────────────
    uint32_t statusId_ = 0;
    bool statusFound_ = false;

    struct StatusVal
    {
        std::string name;
        double value = 0.0;
        bool received = false;
        double timeMs = 0.0;
    };
    std::vector<StatusVal> statusVals_;
};
