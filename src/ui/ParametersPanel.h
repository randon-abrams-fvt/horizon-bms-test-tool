#pragma once

#include "can/CanBus.h"

#include <cstdint>
#include <string>
#include <vector>

class ParametersPanel
{
  public:
    void setCanBus(CanBus *bus);
    void pushFrame(const DecodedFrame &frame);
    void render();

  private:
    struct ParamEntry
    {
        uint8_t subIndex = 0;
        std::string name;
        uint16_t dataType = 0;
        std::string accessType;
        std::string defaultValue;
        std::string currentValue = "-";
        bool hasCurrent = false;
        char newValueInput[64] = {};
    };

    enum class PendingKind
    {
        None,
        Read,
        Write,
    };

    struct PendingRequest
    {
        PendingKind kind = PendingKind::None;
        uint8_t subIndex = 0;
        uint16_t dataType = 0;
    };

    bool ensureEdsLoaded();
    bool loadObject23E0FromEds(const std::string &edsPath);

    void queueReadAll();
    void queueRead(uint8_t subIndex);
    void dispatchNextRequest();

    bool sendReadRequest(uint8_t subIndex);
    bool sendWriteRequest(uint8_t subIndex, const std::string &text);

    int findParamIndex(uint8_t subIndex) const;

    uint32_t sdoTxCobId() const;
    uint32_t sdoRxCobId() const;

    static bool parseTypedValue(
        uint16_t dataType,
        const std::string &text,
        uint32_t &raw,
        uint8_t &size);
    static std::string formatTypedValue(
        uint16_t dataType, uint32_t raw, uint8_t size);

    CanBus *bus_ = nullptr;

    std::vector<ParamEntry> params_;
    std::vector<uint8_t> readQueue_;

    PendingRequest pending_{};

    bool edsLoadAttempted_ = false;
    bool edsLoaded_ = false;
    std::string edsPath_;

    int nodeId_ = 1;
    bool autoReadOnConnect_ = true;

    std::string status_;
};
