#pragma once

#include <cstdint>

/// A single CAN bus frame.
struct CanFrame
{
    uint32_t id          = 0;      ///< 11-bit (standard) or 29-bit (extended) CAN ID
    uint8_t  dlc         = 8;      ///< data length code (0–8)
    uint8_t  data[8]     = {};     ///< payload bytes
    bool     isExtended  = false;  ///< true for 29-bit extended frames
    double   timestampMs = 0.0;    ///< time of reception in ms (from app start)
};
