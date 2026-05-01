#pragma once

#include "can/CanBus.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DbcModel;

/// Fault diagnostics tab.
/// Displays fault bits from safety_app_faults_1 as an active/inactive grid.
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

    struct FaultSignal
    {
        std::string name;
        LiveVal *live = nullptr;
    };

    void findMessages();
    void renderFaultGrid();

    const DbcModel *dbc_ = nullptr;
    CanBus *bus_ = nullptr;

    uint32_t faultsMsgId_ = 0;
    bool faultsMessageFound_ = false;
    bool faultsMessageSeen_ = false;

    std::unordered_set<uint32_t> watchIds_;
    std::unordered_map<std::string, LiveVal> liveVals_;
    std::vector<FaultSignal> faultSignals_;
};
