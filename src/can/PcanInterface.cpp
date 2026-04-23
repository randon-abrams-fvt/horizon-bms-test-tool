#include "can/PcanInterface.h"

#include <cstdio>
#include <cstring>

// All 16 possible PCAN USB channel handles.
static constexpr TPCANHandle kUsbChannels[] = {
    PCAN_USBBUS1,
    PCAN_USBBUS2,
    PCAN_USBBUS3,
    PCAN_USBBUS4,
    PCAN_USBBUS5,
    PCAN_USBBUS6,
    PCAN_USBBUS7,
    PCAN_USBBUS8,
    PCAN_USBBUS9,
    PCAN_USBBUS10,
    PCAN_USBBUS11,
    PCAN_USBBUS12,
    PCAN_USBBUS13,
    PCAN_USBBUS14,
    PCAN_USBBUS15,
    PCAN_USBBUS16,
};

std::vector<PcanDeviceInfo> PcanInterface::scanDevices()
{
    std::vector<PcanDeviceInfo> devices;

    for (int i = 0; i < 16; ++i)
    {
        TPCANHandle ch = kUsbChannels[i];
        DWORD condition = 0;
        TPCANStatus st = CAN_GetValue(
            ch, PCAN_CHANNEL_CONDITION, &condition, sizeof(condition));
        if (st != PCAN_ERROR_OK)
            continue;
        if (!(condition & PCAN_CHANNEL_AVAILABLE) &&
            !(condition & PCAN_CHANNEL_OCCUPIED))
            continue;

        // Try to read the device number for a friendlier label
        DWORD devNum = 0;
        CAN_GetValue(ch, PCAN_DEVICE_ID, &devNum, sizeof(devNum));

        char label[64];
        std::snprintf(
            label,
            sizeof(label),
            "PCAN-USB %d  (0x%02X, dev %lu)",
            i + 1,
            static_cast<unsigned>(ch),
            static_cast<unsigned long>(devNum));

        devices.push_back({ch, label});
    }

    return devices;
}

PcanInterface::~PcanInterface()
{
    close();
}

bool PcanInterface::open(uint32_t channel, uint32_t baudrate)
{
    handle_ = static_cast<TPCANHandle>(channel);
    TPCANStatus st = CAN_Initialize(handle_, mapBaudrate(baudrate));
    if (st != PCAN_ERROR_OK)
    {
        char buf[256] = {};
        CAN_GetErrorText(st, 0, buf);
        lastError_ = buf;
        return false;
    }
    isOpen_ = true;
    lastError_.clear();
    return true;
}

void PcanInterface::close()
{
    if (isOpen_)
    {
        CAN_Uninitialize(handle_);
        isOpen_ = false;
    }
}

bool PcanInterface::send(const CanFrame &frame)
{
    if (!isOpen_)
    {
        lastError_ = "Not open";
        return false;
    }

    TPCANMsg msg = {};
    msg.ID = frame.id;
    msg.LEN = frame.dlc;
    msg.MSGTYPE =
        frame.isExtended ? PCAN_MESSAGE_EXTENDED : PCAN_MESSAGE_STANDARD;
    std::memcpy(msg.DATA, frame.data, frame.dlc);

    TPCANStatus st = CAN_Write(handle_, &msg);
    if (st != PCAN_ERROR_OK)
    {
        char buf[256] = {};
        CAN_GetErrorText(st, 0, buf);
        lastError_ = buf;
        return false;
    }
    return true;
}

bool PcanInterface::receive(CanFrame &frameOut)
{
    if (!isOpen_)
        return false;

    TPCANMsg msg = {};
    TPCANTimestamp ts = {};
    TPCANStatus st = CAN_Read(handle_, &msg, &ts);

    if (st == PCAN_ERROR_QRCVEMPTY)
        return false;
    if (st != PCAN_ERROR_OK)
    {
        char buf[256] = {};
        CAN_GetErrorText(st, 0, buf);
        lastError_ = buf;
        return false;
    }

    frameOut.id = msg.ID;
    frameOut.dlc = msg.LEN;
    frameOut.isExtended = (msg.MSGTYPE & PCAN_MESSAGE_EXTENDED) != 0;
    frameOut.timestampMs = static_cast<double>(ts.millis) + ts.micros / 1000.0;
    std::memcpy(frameOut.data, msg.DATA, msg.LEN);
    return true;
}

bool PcanInterface::isOpen() const
{
    return isOpen_;
}
const char *PcanInterface::lastError() const
{
    return lastError_.c_str();
}

TPCANBaudrate PcanInterface::mapBaudrate(uint32_t bps)
{
    switch (bps)
    {
    case 1000000:
        return PCAN_BAUD_1M;
    case 500000:
        return PCAN_BAUD_500K;
    case 250000:
        return PCAN_BAUD_250K;
    case 125000:
        return PCAN_BAUD_125K;
    case 100000:
        return PCAN_BAUD_100K;
    default:
        return PCAN_BAUD_500K;
    }
}
