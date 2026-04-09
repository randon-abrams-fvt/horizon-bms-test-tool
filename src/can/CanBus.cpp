#include "can/CanBus.h"
#include "dbc/DbcModel.h"

#include <chrono>

CanBus::CanBus(std::unique_ptr<ICanInterface> iface) : iface_(std::move(iface))
{
}

CanBus::~CanBus()
{
    close();
}

bool CanBus::open(uint32_t channel, uint32_t baudrate)
{
    if (!iface_->open(channel, baudrate))
        return false;
    running_ = true;
    rxThread_ = std::thread(&CanBus::rxLoop, this);
    return true;
}

void CanBus::close()
{
    running_ = false;
    if (rxThread_.joinable())
        rxThread_.join();
    iface_->close();
}

bool CanBus::isOpen() const
{
    return iface_->isOpen();
}
bool CanBus::send(const CanFrame &frame)
{
    return iface_->send(frame);
}
void CanBus::setDbc(const DbcModel *dbc)
{
    dbc_ = dbc;
}
const char *CanBus::lastError() const
{
    return iface_->lastError();
}

std::vector<DecodedFrame> CanBus::drainRxFrames()
{
    std::lock_guard<std::mutex> lock(rxMutex_);
    std::vector<DecodedFrame> out;
    out.reserve(rxQueue_.size());
    while (!rxQueue_.empty())
    {
        out.push_back(std::move(rxQueue_.front()));
        rxQueue_.pop();
    }
    return out;
}

void CanBus::rxLoop()
{
    while (running_)
    {
        CanFrame raw;
        if (iface_->receive(raw))
        {
            DecodedFrame df;
            df.raw = raw;
            if (dbc_)
                dbc_->decode(raw, df.messageName, df.signals);

            std::lock_guard<std::mutex> lock(rxMutex_);
            rxQueue_.push(std::move(df));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}
