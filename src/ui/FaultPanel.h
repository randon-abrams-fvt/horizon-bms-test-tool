#pragma once

#include "can/CanBus.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DbcModel;

/// Fault diagnostics tab.
/// Sends DebugFaultRequest and renders decoded fault debug/status frames.
class FaultPanel
{
  public:
    void setDbc(const DbcModel *dbc);
    void setCanBus(CanBus *bus);
    void pushFrame(const DecodedFrame &frame);
    void render();

  private:
    struct LiveVal
    {
        double value = 0.0;
        bool received = false;
    };

    void findMessages();
    void sendRequest();
    void renderMessageTable(
        const char *title, const std::vector<std::string> &messageOrder);
    static void renderSignalValue(
        const std::unordered_map<std::string, LiveVal> &liveVals,
        const std::string &sigName);

    const DbcModel *dbc_ = nullptr;
    CanBus *bus_ = nullptr;

    uint32_t requestMsgId_ = 0;
    bool requestFound_ = false;

    std::unordered_set<uint32_t> watchIds_;

    std::unordered_map<std::string, std::vector<std::string>> messageSignals_;
    std::unordered_map<std::string, bool> messageSeen_;
    std::unordered_map<std::string, LiveVal> liveVals_;

    int selectedFaultId_ = 0;
    int commandRequest_ = 1; // 0 = NoCommand, 1 = ReportDetail
    std::string status_;
};
