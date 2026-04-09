#pragma once

#include "can/ICanInterface.h"

#include <mutex>
#include <queue>
#include <string>

/// Loopback CAN interface.
/// Transmitted frames are immediately echoed into the receive queue.
/// Used for development and testing without physical hardware.
class VirtualInterface : public ICanInterface
{
public:
    bool        open(uint32_t channel, uint32_t baudrate) override;
    void        close()                                   override;
    bool        send(const CanFrame& frame)               override;
    bool        receive(CanFrame& frameOut)               override;
    bool        isOpen()    const                         override;
    const char* lastError() const                         override;

    /// Inject a frame directly into the receive queue (for testing/simulation).
    void inject(const CanFrame& frame);

private:
    bool                 isOpen_   = false;
    std::string          lastError_;
    std::queue<CanFrame> rxQueue_;
    mutable std::mutex   mutex_;
};
