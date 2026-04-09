#pragma once

#ifdef BMU_PCAN_AVAILABLE

#include "can/ICanInterface.h"

#include <string>
#include <PCANBasic.h>

/// CAN interface backed by the Peak PCAN-Basic API.
class PcanInterface : public ICanInterface
{
public:
    PcanInterface()  = default;
    ~PcanInterface() override;

    bool        open(uint32_t channel, uint32_t baudrate) override;
    void        close()                                   override;
    bool        send(const CanFrame& frame)               override;
    bool        receive(CanFrame& frameOut)               override;
    bool        isOpen()    const                         override;
    const char* lastError() const                         override;

private:
    static TPCANBaudrate mapBaudrate(uint32_t bps);

    TPCANHandle handle_    = PCAN_NONEBUS;
    bool        isOpen_    = false;
    std::string lastError_;
};

#endif // BMU_PCAN_AVAILABLE
