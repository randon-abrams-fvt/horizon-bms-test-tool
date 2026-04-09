#pragma once

#include "can/CanFrame.h"

/// Abstract CAN hardware interface.
/// Concrete implementations: PcanInterface (PCAN-Basic), VirtualInterface (loopback).
class ICanInterface
{
public:
    virtual ~ICanInterface() = default;

    /// Open the interface on the given hardware channel at the specified baud rate.
    /// @param channel  Hardware channel (e.g. PCAN_USBBUS1 = 0x51 for PCAN).
    /// @param baudrate Baud rate in bits/s (e.g. 500000).
    virtual bool open(uint32_t channel, uint32_t baudrate) = 0;

    virtual void close() = 0;

    /// Non-blocking transmit. Returns false on error.
    virtual bool send(const CanFrame& frame) = 0;

    /// Non-blocking receive. Returns false if the queue is empty.
    virtual bool receive(CanFrame& frameOut) = 0;

    virtual bool        isOpen()    const = 0;
    virtual const char* lastError() const = 0;
};
