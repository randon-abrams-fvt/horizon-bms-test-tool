#include "can/VirtualInterface.h"

#include <chrono>

static double nowMs()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch())
        .count();
}

bool VirtualInterface::open(uint32_t /*channel*/, uint32_t /*baudrate*/)
{
    isOpen_ = true;
    lastError_.clear();
    return true;
}

void VirtualInterface::close()
{
    isOpen_ = false;
}

bool VirtualInterface::send(const CanFrame &frame)
{
    if (!isOpen_)
    {
        lastError_ = "Not open";
        return false;
    }

    // Loopback: echo the frame back into the RX queue
    CanFrame loopback = frame;
    loopback.timestampMs = nowMs();
    std::lock_guard<std::mutex> lock(mutex_);
    rxQueue_.push(loopback);
    return true;
}

bool VirtualInterface::receive(CanFrame &frameOut)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (rxQueue_.empty())
        return false;
    frameOut = rxQueue_.front();
    rxQueue_.pop();
    return true;
}

bool VirtualInterface::isOpen() const
{
    return isOpen_;
}
const char *VirtualInterface::lastError() const
{
    return lastError_.c_str();
}

void VirtualInterface::inject(const CanFrame &frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rxQueue_.push(frame);
}
