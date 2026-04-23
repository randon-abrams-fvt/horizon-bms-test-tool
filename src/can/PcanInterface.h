#pragma once

#include "can/ICanInterface.h"

#include <windows.h>

#include <PCANBasic.h>
#include <string>
#include <vector>

/// Information about a discovered PCAN device.
struct PcanDeviceInfo
{
    TPCANHandle handle; ///< Channel handle (e.g. PCAN_USBBUS1)
    std::string label;  ///< Human-readable label for the UI
};

/// CAN interface backed by the Peak PCAN-Basic API.
class PcanInterface : public ICanInterface
{
  public:
    PcanInterface() = default;
    ~PcanInterface() override;

    bool open(uint32_t channel, uint32_t baudrate) override;
    void close() override;
    bool send(const CanFrame &frame) override;
    bool receive(CanFrame &frameOut) override;
    bool isOpen() const override;
    const char *lastError() const override;

    /// Scan for available (plugged-in) PCAN USB devices.
    static std::vector<PcanDeviceInfo> scanDevices();

  private:
    static TPCANBaudrate mapBaudrate(uint32_t bps);

    TPCANHandle handle_ = PCAN_NONEBUS;
    bool isOpen_ = false;
    std::string lastError_;
};
