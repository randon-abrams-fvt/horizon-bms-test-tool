#include "dbc/DbcModel.h"

#include <dbcppp/Network.h>

#include <cstring>
#include <regex>
#include <sstream>

bool DbcModel::loadFromString(const std::string &content)
{
    std::istringstream ss(content);
    auto net = dbcppp::INetwork::LoadDBCFromIs(ss);
    if (!net)
        return false;

    network_ = std::move(net);
    messages_.clear();
    msgById_.clear();
    sigValueLabels_.clear();

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

    buildValueLabels(content);

    return true;
}

bool DbcModel::isLoaded() const
{
    return network_ != nullptr;
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

// ── Value label lookup
// ────────────────────────────────────────────────────────

void DbcModel::buildValueLabels(const std::string &content)
{
    // ── Step 1: build value-table-name → {value → label} from dbcppp ─────────
    std::unordered_map<std::string, std::unordered_map<int64_t, std::string>>
        tableByName;

    for (const dbcppp::IValueTable &vt : network_->ValueTables())
    {
        auto &m = tableByName[vt.Name()];
        for (uint64_t i = 0; i < vt.ValueEncodingDescriptions_Size(); ++i)
        {
            const auto &ved = vt.ValueEncodingDescriptions_Get(i);
            m[ved.Value()] = ved.Description();
        }
    }

    // ── Step 2: inline VAL_ entries directly on signals
    // ───────────────────────
    for (const dbcppp::IMessage &msg : network_->Messages())
    {
        for (const dbcppp::ISignal &sig : msg.Signals())
        {
            if (sig.ValueEncodingDescriptions_Size() == 0)
                continue;
            auto &m = sigValueLabels_[sig.Name()];
            for (uint64_t i = 0; i < sig.ValueEncodingDescriptions_Size(); ++i)
            {
                const auto &ved = sig.ValueEncodingDescriptions_Get(i);
                m[ved.Value()] = ved.Description();
            }
        }
    }

    // ── Step 3: SIG_TYPE_REF_ — dbcppp doesn't parse this into structured
    // ─────
    //           data, so scan the raw text with a regex.
    //           Format: SIG_TYPE_REF_ <msg_id> <signal_name> <table_name> ;
    static const std::regex kSigTypeRef(
        R"(SIG_TYPE_REF_\s+\d+\s+(\w+)\s+(\w+)\s*;)");

    for (std::sregex_iterator it(content.begin(), content.end(), kSigTypeRef),
         end;
         it != end;
         ++it)
    {
        const std::string &sigName = (*it)[1];
        const std::string &tableName = (*it)[2];

        auto tit = tableByName.find(tableName);
        if (tit == tableByName.end())
            continue;

        // Only set if not already populated by an inline VAL_ entry
        sigValueLabels_.emplace(sigName, tit->second);
    }
}

const char *DbcModel::valueLabel(
    const std::string &sigName, int64_t value) const
{
    auto sit = sigValueLabels_.find(sigName);
    if (sit == sigValueLabels_.end())
        return nullptr;
    auto vit = sit->second.find(value);
    if (vit == sit->second.end())
        return nullptr;
    return vit->second.c_str();
}
