#include "dbc/DbcModel.h"

#include <dbcppp/Network.h>

#include <cstring>
#include <fstream>

bool DbcModel::load(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open())
        return false;

    auto net = dbcppp::INetwork::LoadDBCFromIs(f);
    if (!net)
        return false;

    network_ = std::move(net);
    filePath_ = path;
    messages_.clear();
    msgById_.clear();

    for (const dbcppp::IMessage &msg : network_->Messages())
    {
        // In DBC files, extended (29-bit) IDs have bit 31 set.
        const uint64_t rawId = msg.Id();
        const bool isExt = (rawId & 0x80000000ULL) != 0;
        const uint32_t canId = static_cast<uint32_t>(rawId & 0x1FFFFFFFUL);

        DMessage dm;
        dm.id = canId;
        dm.name = msg.Name();
        dm.dlc = static_cast<uint8_t>(msg.MessageSize());
        dm.isExtended = isExt;
        dm.transmitter = msg.Transmitter();

        for (const dbcppp::ISignal &sig : msg.Signals())
        {
            DSignal ds;
            ds.name = sig.Name();
            ds.unit = sig.Unit();
            ds.factor = sig.Factor();
            ds.offset = sig.Offset();
            ds.min = sig.Minimum();
            ds.max = sig.Maximum();
            dm.signals.push_back(std::move(ds));
        }

        msgById_[canId] = &msg;
        messages_.push_back(std::move(dm));
    }

    return true;
}

void DbcModel::unload()
{
    network_.reset();
    messages_.clear();
    msgById_.clear();
    filePath_.clear();
}

bool DbcModel::isLoaded() const
{
    return network_ != nullptr;
}
const std::string &DbcModel::filePath() const
{
    return filePath_;
}
const std::vector<DMessage> &DbcModel::messages() const
{
    return messages_;
}

bool DbcModel::decode(
    const CanFrame &frame,
    std::string &msgNameOut,
    std::vector<std::pair<std::string, double>> &signalsOut) const
{
    if (!network_)
        return false;

    auto it = msgById_.find(frame.id);
    if (it == msgById_.end())
        return false;

    const dbcppp::IMessage *msg = it->second;
    msgNameOut = msg->Name();
    signalsOut.clear();

    // dbcppp requires at least 8 bytes in the buffer
    uint8_t buf[8] = {};
    const uint8_t copyLen = frame.dlc < 8 ? frame.dlc : 8;
    std::memcpy(buf, frame.data, copyLen);

    for (const dbcppp::ISignal &sig : msg->Signals())
    {
        const auto raw = sig.Decode(buf);
        const double phys = sig.RawToPhys(raw);
        signalsOut.emplace_back(sig.Name(), phys);
    }
    return true;
}

bool DbcModel::encode(
    uint32_t msgId,
    const std::vector<std::pair<std::string, double>> &signalValues,
    CanFrame &frameOut) const
{
    if (!network_)
        return false;

    auto it = msgById_.find(msgId);
    if (it == msgById_.end())
        return false;

    const dbcppp::IMessage *msg = it->second;
    const uint64_t rawId = msg->Id();

    frameOut.id = msgId;
    frameOut.dlc = static_cast<uint8_t>(msg->MessageSize());
    frameOut.isExtended = (rawId & 0x80000000ULL) != 0;
    std::memset(frameOut.data, 0, 8);

    for (const dbcppp::ISignal &sig : msg->Signals())
    {
        // Default physical value is the signal's zero (offset only)
        double phys = sig.Offset();
        for (const auto &[name, val] : signalValues)
        {
            if (name == sig.Name())
            {
                phys = val;
                break;
            }
        }
        const auto raw = sig.PhysToRaw(phys);
        sig.Encode(raw, frameOut.data);
    }
    return true;
}
