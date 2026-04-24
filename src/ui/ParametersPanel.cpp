#include "ui/ParametersPanel.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace
{
constexpr uint16_t kParamIndex = 0x23E0;

std::string trim(const std::string &s)
{
    size_t start = 0;
    while (start < s.size() &&
           std::isspace(static_cast<unsigned char>(s[start])))
        ++start;

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;

    return s.substr(start, end - start);
}

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool parseUnsigned(const std::string &text, uint64_t &out)
{
    char *end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 0);
    if (errno != 0 || end == text.c_str() || *end != '\0')
        return false;
    out = static_cast<uint64_t>(value);
    return true;
}

bool parseSigned(const std::string &text, int64_t &out)
{
    char *end = nullptr;
    errno = 0;
    const long long value = std::strtoll(text.c_str(), &end, 0);
    if (errno != 0 || end == text.c_str() || *end != '\0')
        return false;
    out = static_cast<int64_t>(value);
    return true;
}

bool parseDouble(const std::string &text, double &out)
{
    char *end = nullptr;
    errno = 0;
    const double value = std::strtod(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0')
        return false;
    out = value;
    return true;
}

std::string formatSub(uint8_t sub)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned>(sub));
    return std::string(buf);
}
} // namespace

void ParametersPanel::setCanBus(CanBus *bus)
{
    bus_ = bus;
    pending_ = {};
    readQueue_.clear();

    if (autoReadOnConnect_ && bus_ && bus_->isOpen() && !params_.empty())
        queueReadAll();
}

void ParametersPanel::pushFrame(const DecodedFrame &frame)
{
    if (frame.raw.id != sdoRxCobId())
        return;

    const uint8_t *d = frame.raw.data;
    const uint16_t idx = static_cast<uint16_t>(d[1] | (d[2] << 8));
    const uint8_t sub = d[3];
    if (idx != kParamIndex)
        return;

    const int paramIdx = findParamIndex(sub);
    if (paramIdx < 0)
        return;

    ParamEntry &param = params_[static_cast<size_t>(paramIdx)];

    if (d[0] == 0x80)
    {
        const uint32_t abortCode = static_cast<uint32_t>(
            d[4] | (d[5] << 8) | (d[6] << 16) | (d[7] << 24));
        char msg[96];
        std::snprintf(
            msg,
            sizeof(msg),
            "SDO abort for sub %s (0x%08X)",
            formatSub(sub).c_str(),
            static_cast<unsigned>(abortCode));
        status_ = msg;
        pending_ = {};
        dispatchNextRequest();
        return;
    }

    if ((d[0] & 0xE0) == 0x40)
    {
        const bool expedited = (d[0] & 0x02) != 0;
        const bool sizeIndicated = (d[0] & 0x01) != 0;
        if (!expedited || !sizeIndicated)
        {
            status_ =
                "Segmented SDO responses are not supported by this panel.";
            pending_ = {};
            dispatchNextRequest();
            return;
        }

        const uint8_t nUnused = static_cast<uint8_t>((d[0] >> 2) & 0x03);
        const uint8_t dataSize = static_cast<uint8_t>(4U - nUnused);
        uint32_t raw = 0;
        for (uint8_t i = 0; i < dataSize; ++i)
            raw |= static_cast<uint32_t>(d[4 + i]) << (8U * i);

        param.currentValue = formatTypedValue(param.dataType, raw, dataSize);
        param.hasCurrent = true;

        char msg[96];
        std::snprintf(
            msg,
            sizeof(msg),
            "Read %s (%s)",
            param.name.c_str(),
            formatSub(sub).c_str());
        status_ = msg;

        pending_ = {};
        dispatchNextRequest();
        return;
    }

    if (d[0] == 0x60)
    {
        char msg[96];
        std::snprintf(
            msg,
            sizeof(msg),
            "Wrote %s (%s)",
            param.name.c_str(),
            formatSub(sub).c_str());
        status_ = msg;

        pending_ = {};
        queueRead(sub);
        dispatchNextRequest();
        return;
    }
}

void ParametersPanel::render()
{
    ensureEdsLoaded();

    if (!edsLoaded_)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            "Could not load goldengate-0.4.0+0.eds");
        if (!status_.empty())
            ImGui::TextWrapped("%s", status_.c_str());

        if (ImGui::Button("Retry EDS load"))
        {
            edsLoadAttempted_ = false;
            ensureEdsLoaded();
        }
        return;
    }

    dispatchNextRequest();

    ImGui::Text("CANopen Parameters (0x23E0)");
    ImGui::TextDisabled("EDS source: %s", edsPath_.c_str());
    ImGui::Separator();

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Node ID", &nodeId_);
    if (nodeId_ < 1)
        nodeId_ = 1;
    if (nodeId_ > 127)
        nodeId_ = 127;

    const bool canUseBus = bus_ && bus_->isOpen();
    const bool busy = (pending_.kind != PendingKind::None);

    if (!canUseBus)
        ImGui::BeginDisabled();
    if (ImGui::Button("Read All"))
        queueReadAll();
    ImGui::SameLine();
    ImGui::Checkbox("Auto read on connect", &autoReadOnConnect_);
    if (!canUseBus)
        ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled(
        "SDO TX: 0x%03X   RX: 0x%03X", sdoTxCobId(), sdoRxCobId());

    if (!status_.empty())
        ImGui::TextWrapped("%s", status_.c_str());

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

    const float tableHeight = ImGui::GetContentRegionAvail().y;
    if (!ImGui::BeginTable(
            "##Param23E0Table", 8, kTableFlags, ImVec2(0.0f, tableHeight)))
        return;

    ImGui::TableSetupColumn("Sub", ImGuiTableColumnFlags_WidthFixed, 56.0f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.8f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn(
        "New Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn(
        "Current", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn(
        "Default", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn(
        "Actions", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < params_.size(); ++i)
    {
        ParamEntry &param = params_[i];
        const bool writable =
            toLower(param.accessType).find('w') != std::string::npos;

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(formatSub(param.subIndex).c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(param.name.c_str());

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("0x%04X", static_cast<unsigned>(param.dataType));

        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(param.accessType.c_str());

        ImGui::TableSetColumnIndex(4);
        {
            char id[32];
            std::snprintf(
                id,
                sizeof(id),
                "##new_%u",
                static_cast<unsigned>(param.subIndex));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText(
                id, param.newValueInput, sizeof(param.newValueInput));
        }

        ImGui::TableSetColumnIndex(5);
        if (param.hasCurrent)
            ImGui::TextUnformatted(param.currentValue.c_str());
        else
            ImGui::TextDisabled("-");

        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(param.defaultValue.c_str());

        ImGui::TableSetColumnIndex(7);
        {
            char readId[32];
            std::snprintf(
                readId,
                sizeof(readId),
                "Read##%u",
                static_cast<unsigned>(param.subIndex));

            if (!canUseBus || busy)
                ImGui::BeginDisabled();
            if (ImGui::SmallButton(readId))
                queueRead(param.subIndex);
            if (!canUseBus || busy)
                ImGui::EndDisabled();

            ImGui::SameLine();

            char writeId[32];
            std::snprintf(
                writeId,
                sizeof(writeId),
                "Write##%u",
                static_cast<unsigned>(param.subIndex));

            const bool writeDisabled = !canUseBus || busy || !writable;
            if (writeDisabled)
                ImGui::BeginDisabled();
            if (ImGui::SmallButton(writeId))
                sendWriteRequest(param.subIndex, param.newValueInput);
            if (writeDisabled)
                ImGui::EndDisabled();
        }
    }

    ImGui::EndTable();
}

bool ParametersPanel::ensureEdsLoaded()
{
    if (edsLoaded_)
        return true;
    if (edsLoadAttempted_)
        return false;

    edsLoadAttempted_ = true;

    static const char *kCandidates[] = {
        "goldengate-0.4.0+0.eds",
        "../goldengate-0.4.0+0.eds",
        "../../goldengate-0.4.0+0.eds",
        "../../../goldengate-0.4.0+0.eds",
        "../../../../goldengate-0.4.0+0.eds",
    };

    for (const char *candidate : kCandidates)
    {
        if (loadObject23E0FromEds(candidate))
        {
            edsLoaded_ = true;
            edsPath_ = candidate;
            status_ = "Loaded object 0x23E0 from EDS.";
            return true;
        }
    }

    status_ = "EDS parse failed for all known paths.";
    return false;
}

bool ParametersPanel::loadObject23E0FromEds(const std::string &edsPath)
{
    std::ifstream in(edsPath);
    if (!in)
        return false;

    struct TempEntry
    {
        bool used = false;
        std::string name;
        uint16_t dataType = 0;
        std::string access;
        std::string defaultValue;
    };

    std::vector<TempEntry> temp(256);

    std::string section;
    std::string line;
    while (std::getline(in, line))
    {
        const std::string t = trim(line);
        if (t.empty() || t[0] == ';')
            continue;

        if (t.front() == '[' && t.back() == ']')
        {
            section = toLower(t.substr(1, t.size() - 2));
            continue;
        }

        const size_t eq = t.find('=');
        if (eq == std::string::npos)
            continue;

        const std::string key = toLower(trim(t.substr(0, eq)));
        const std::string value = trim(t.substr(eq + 1));

        if (section.rfind("23e0sub", 0) != 0)
            continue;

        const std::string subHex = section.substr(7);
        if (subHex.empty())
            continue;

        unsigned long parsedSub = 0;
        try
        {
            parsedSub = std::stoul(subHex, nullptr, 16);
        }
        catch (...)
        {
            continue;
        }
        if (parsedSub > 255)
            continue;

        TempEntry &te = temp[parsedSub];
        te.used = true;

        if (key == "parametername")
            te.name = value;
        else if (key == "datatype")
        {
            uint64_t u = 0;
            if (parseUnsigned(value, u) && u <= 0xFFFF)
                te.dataType = static_cast<uint16_t>(u);
        }
        else if (key == "accesstype")
            te.access = value;
        else if (key == "defaultvalue")
            te.defaultValue = value;
    }

    std::vector<ParamEntry> parsed;
    for (size_t sub = 0; sub < temp.size(); ++sub)
    {
        const TempEntry &te = temp[sub];
        if (!te.used)
            continue;

        ParamEntry p;
        p.subIndex = static_cast<uint8_t>(sub);
        p.name = te.name.empty() ? "(unnamed)" : te.name;
        p.dataType = te.dataType;
        p.accessType = te.access.empty() ? "rw" : te.access;
        p.defaultValue = te.defaultValue.empty() ? "-" : te.defaultValue;
        std::snprintf(
            p.newValueInput,
            sizeof(p.newValueInput),
            "%s",
            p.defaultValue.c_str());
        parsed.push_back(std::move(p));
    }

    if (parsed.empty())
        return false;

    params_ = std::move(parsed);
    return true;
}

void ParametersPanel::queueReadAll()
{
    readQueue_.clear();
    for (const ParamEntry &p : params_)
        readQueue_.push_back(p.subIndex);
}

void ParametersPanel::queueRead(uint8_t subIndex)
{
    readQueue_.push_back(subIndex);
}

void ParametersPanel::dispatchNextRequest()
{
    if (pending_.kind != PendingKind::None)
        return;
    if (!bus_ || !bus_->isOpen())
        return;
    if (readQueue_.empty())
        return;

    const uint8_t sub = readQueue_.front();
    readQueue_.erase(readQueue_.begin());
    sendReadRequest(sub);
}

bool ParametersPanel::sendReadRequest(uint8_t subIndex)
{
    if (!bus_ || !bus_->isOpen())
        return false;

    const int idx = findParamIndex(subIndex);
    if (idx < 0)
        return false;

    CanFrame frame;
    frame.id = sdoTxCobId();
    frame.dlc = 8;
    frame.data[0] = 0x40;
    frame.data[1] = static_cast<uint8_t>(kParamIndex & 0xFF);
    frame.data[2] = static_cast<uint8_t>((kParamIndex >> 8) & 0xFF);
    frame.data[3] = subIndex;

    if (!bus_->send(frame))
    {
        status_ = "Failed to send SDO read request.";
        return false;
    }

    pending_.kind = PendingKind::Read;
    pending_.subIndex = subIndex;
    pending_.dataType = params_[static_cast<size_t>(idx)].dataType;
    return true;
}

bool ParametersPanel::sendWriteRequest(
    uint8_t subIndex, const std::string &text)
{
    if (!bus_ || !bus_->isOpen())
        return false;

    const int idx = findParamIndex(subIndex);
    if (idx < 0)
        return false;

    ParamEntry &param = params_[static_cast<size_t>(idx)];

    const std::string access = toLower(param.accessType);
    if (access.find('w') == std::string::npos)
    {
        status_ = "Selected subindex is read-only.";
        return false;
    }

    uint32_t raw = 0;
    uint8_t size = 0;
    if (!parseTypedValue(param.dataType, trim(text), raw, size))
    {
        status_ = "Invalid value for selected parameter type.";
        return false;
    }

    CanFrame frame;
    frame.id = sdoTxCobId();
    frame.dlc = 8;

    if (size == 1)
        frame.data[0] = 0x2F;
    else if (size == 2)
        frame.data[0] = 0x2B;
    else if (size == 3)
        frame.data[0] = 0x27;
    else
        frame.data[0] = 0x23;

    frame.data[1] = static_cast<uint8_t>(kParamIndex & 0xFF);
    frame.data[2] = static_cast<uint8_t>((kParamIndex >> 8) & 0xFF);
    frame.data[3] = subIndex;
    frame.data[4] = static_cast<uint8_t>(raw & 0xFF);
    frame.data[5] = static_cast<uint8_t>((raw >> 8) & 0xFF);
    frame.data[6] = static_cast<uint8_t>((raw >> 16) & 0xFF);
    frame.data[7] = static_cast<uint8_t>((raw >> 24) & 0xFF);

    if (!bus_->send(frame))
    {
        status_ = "Failed to send SDO write request.";
        return false;
    }

    pending_.kind = PendingKind::Write;
    pending_.subIndex = subIndex;
    pending_.dataType = param.dataType;
    return true;
}

int ParametersPanel::findParamIndex(uint8_t subIndex) const
{
    for (size_t i = 0; i < params_.size(); ++i)
    {
        if (params_[i].subIndex == subIndex)
            return static_cast<int>(i);
    }
    return -1;
}

uint32_t ParametersPanel::sdoTxCobId() const
{
    return static_cast<uint32_t>(0x600 + nodeId_);
}

uint32_t ParametersPanel::sdoRxCobId() const
{
    return static_cast<uint32_t>(0x580 + nodeId_);
}

bool ParametersPanel::parseTypedValue(
    uint16_t dataType, const std::string &text, uint32_t &raw, uint8_t &size)
{
    switch (dataType)
    {
    case 0x0002: // INTEGER8
    {
        int64_t v = 0;
        if (!parseSigned(text, v) || v < -128 || v > 127)
            return false;
        raw = static_cast<uint32_t>(static_cast<int8_t>(v));
        size = 1;
        return true;
    }
    case 0x0003: // INTEGER16
    {
        int64_t v = 0;
        if (!parseSigned(text, v) || v < -32768 || v > 32767)
            return false;
        raw = static_cast<uint32_t>(
            static_cast<uint16_t>(static_cast<int16_t>(v)));
        size = 2;
        return true;
    }
    case 0x0004: // INTEGER32
    {
        int64_t v = 0;
        if (!parseSigned(text, v) || v < std::numeric_limits<int32_t>::min() ||
            v > std::numeric_limits<int32_t>::max())
            return false;
        raw = static_cast<uint32_t>(static_cast<int32_t>(v));
        size = 4;
        return true;
    }
    case 0x0005: // UNSIGNED8
    {
        uint64_t v = 0;
        if (!parseUnsigned(text, v) || v > 0xFF)
            return false;
        raw = static_cast<uint32_t>(v);
        size = 1;
        return true;
    }
    case 0x0006: // UNSIGNED16
    {
        uint64_t v = 0;
        if (!parseUnsigned(text, v) || v > 0xFFFF)
            return false;
        raw = static_cast<uint32_t>(v);
        size = 2;
        return true;
    }
    case 0x0007: // UNSIGNED32
    {
        uint64_t v = 0;
        if (!parseUnsigned(text, v) || v > 0xFFFFFFFFULL)
            return false;
        raw = static_cast<uint32_t>(v);
        size = 4;
        return true;
    }
    case 0x0008: // REAL32
    {
        double v = 0.0;
        if (!parseDouble(text, v))
            return false;
        const float f = static_cast<float>(v);
        static_assert(
            sizeof(float) == sizeof(uint32_t), "Unexpected float size");
        std::memcpy(&raw, &f, sizeof(raw));
        size = 4;
        return true;
    }
    default:
        return false;
    }
}

std::string ParametersPanel::formatTypedValue(
    uint16_t dataType, uint32_t raw, uint8_t size)
{
    char buf[64];
    switch (dataType)
    {
    case 0x0002: {
        const int8_t v = static_cast<int8_t>(raw & 0xFF);
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(v));
        return std::string(buf);
    }
    case 0x0003: {
        const int16_t v = static_cast<int16_t>(raw & 0xFFFF);
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(v));
        return std::string(buf);
    }
    case 0x0004: {
        const int32_t v = static_cast<int32_t>(raw);
        std::snprintf(buf, sizeof(buf), "%d", v);
        return std::string(buf);
    }
    case 0x0005:
        std::snprintf(
            buf, sizeof(buf), "%u", static_cast<unsigned>(raw & 0xFF));
        return std::string(buf);
    case 0x0006:
        std::snprintf(
            buf, sizeof(buf), "%u", static_cast<unsigned>(raw & 0xFFFF));
        return std::string(buf);
    case 0x0007:
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(raw));
        return std::string(buf);
    case 0x0008: {
        if (size < 4)
            return "-";
        float f = 0.0f;
        std::memcpy(&f, &raw, sizeof(f));
        std::snprintf(buf, sizeof(buf), "%.6f", static_cast<double>(f));
        return std::string(buf);
    }
    default:
        std::snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(raw));
        return std::string(buf);
    }
}
