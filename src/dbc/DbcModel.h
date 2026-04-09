#pragma once

#include "can/CanFrame.h"

#include <dbcppp/Network.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/// Flat description of a signal (used for UI display).
struct DSignal
{
    std::string name;
    std::string unit;
    double factor = 1.0;
    double offset = 0.0;
    double min = 0.0;
    double max = 0.0;
};

/// Flat description of a message (used for UI display).
struct DMessage
{
    uint32_t id = 0; ///< 11 or 29-bit CAN ID (bit 31 stripped)
    std::string name;
    uint8_t dlc = 8;
    bool isExtended = false;
    std::string transmitter;
    std::vector<DSignal> signals;
};

/// Parses a .dbc file via dbcppp.
/// Provides encode/decode helpers used by CanBus and the TX panel.
class DbcModel
{
  public:
    bool load(const std::string &path);
    void unload();

    bool isLoaded() const;
    const std::string &filePath() const;
    const std::vector<DMessage> &messages() const;

    /// Decode a raw CAN frame's signals. Returns false if the ID is not in the
    /// DBC.
    bool decode(
        const CanFrame &frame,
        std::string &msgNameOut,
        std::vector<std::pair<std::string, double>> &signalsOut) const;

    /// Encode physical signal values into a CAN frame payload.
    /// Signals not listed in signalValues are encoded at their offset (zero
    /// physical).
    bool encode(
        uint32_t msgId,
        const std::vector<std::pair<std::string, double>> &signalValues,
        CanFrame &frameOut) const;

  private:
    std::string filePath_;
    std::unique_ptr<dbcppp::INetwork> network_;
    std::vector<DMessage> messages_;

    /// CAN ID (bit 31 stripped) → pointer to IMessage owned by network_.
    std::unordered_map<uint32_t, const dbcppp::IMessage *> msgById_;
};
