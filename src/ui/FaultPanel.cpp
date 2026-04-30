#include "ui/FaultPanel.h"

#include "dbc/DbcModel.h"
#include "imgui.h"

#include <cmath>
#include <cstdio>

namespace
{
const std::vector<std::string> kRequestedDetailMessages = {
    "DebugRequestedFaultStatusDetail1",
    "DebugRequestedFaultStatusDetail2",
    "DebugRequestedFaultStatusDetail3",
    "DebugRequestedFaultStatusDetail4",
};

const std::vector<std::string> kFaultEventMessages = {
    "FaultEventDebug1",
    "FaultEventDebug2",
    "FaultEventDebug3",
    "FaultEventDebug4",
};

bool isWatchedMessage(const std::string &name)
{
    if (name == "DebugFaultRequest")
        return true;

    for (const std::string &msg : kRequestedDetailMessages)
    {
        if (name == msg)
            return true;
    }
    for (const std::string &msg : kFaultEventMessages)
    {
        if (name == msg)
            return true;
    }
    return false;
}
} // namespace

void FaultPanel::setDbc(const DbcModel *dbc)
{
    dbc_ = dbc;
    findMessages();
}

void FaultPanel::setCanBus(CanBus *bus)
{
    bus_ = bus;
}

void FaultPanel::pushFrame(const DecodedFrame &frame)
{
    if (!dbc_ || !watchIds_.count(frame.raw.id))
        return;

    std::string msgName;
    std::vector<std::pair<std::string, double>> signals;
    if (!dbc_->decode(frame.raw, msgName, signals))
        return;

    if (!isWatchedMessage(msgName))
        return;

    messageSeen_[msgName] = true;
    for (const auto &[sig, val] : signals)
    {
        auto it = liveVals_.find(sig);
        if (it != liveVals_.end())
        {
            it->second.value = val;
            it->second.received = true;
        }
    }
}

void FaultPanel::render()
{
    ImGui::Text("Fault Diagnostics");
    ImGui::Separator();

    if (!requestFound_)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            "DebugFaultRequest not found in fault DBC.");
        return;
    }

    const bool canSend = (bus_ && bus_->isOpen());

    ImGui::Text("Request Fault Detail");
    ImGui::TextDisabled(
        "Message: DebugFaultRequest   ID: 0x%08X", requestMsgId_);

    ImGui::SetNextItemWidth(280.0f);
    const char *faultPreview =
        dbc_ ? dbc_->valueLabel("DebugFaultCommandTargetID", selectedFaultId_)
             : nullptr;
    if (ImGui::BeginCombo("Fault", faultPreview ? faultPreview : "Unknown"))
    {
        for (int id = 0; id <= 96; ++id)
        {
            const char *lbl =
                dbc_ ? dbc_->valueLabel("DebugFaultCommandTargetID", id)
                     : nullptr;
            char itemText[128];
            if (lbl)
                std::snprintf(itemText, sizeof(itemText), "%d - %s", id, lbl);
            else
                std::snprintf(itemText, sizeof(itemText), "%d", id);

            const bool selected = (id == selectedFaultId_);
            if (ImGui::Selectable(itemText, selected))
                selectedFaultId_ = id;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const char *reqLabel =
        dbc_ ? dbc_->valueLabel("DebugFaultCommandRequest", commandRequest_)
             : nullptr;
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::BeginCombo("Command", reqLabel ? reqLabel : "-"))
    {
        for (int cmd = 0; cmd <= 1; ++cmd)
        {
            const char *lbl =
                dbc_ ? dbc_->valueLabel("DebugFaultCommandRequest", cmd)
                     : nullptr;
            char itemText[64];
            if (lbl)
                std::snprintf(itemText, sizeof(itemText), "%d - %s", cmd, lbl);
            else
                std::snprintf(itemText, sizeof(itemText), "%d", cmd);

            const bool selected = (cmd == commandRequest_);
            if (ImGui::Selectable(itemText, selected))
                commandRequest_ = cmd;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (!canSend)
        ImGui::BeginDisabled();
    if (ImGui::Button("Send Request", ImVec2(120.0f, 0.0f)))
        sendRequest();
    if (!canSend)
        ImGui::EndDisabled();

    if (!canSend)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(CAN not connected)");
    }

    if (!status_.empty())
        ImGui::TextWrapped("%s", status_.c_str());

    ImGui::Spacing();
    renderMessageTable("Requested Fault Status", kRequestedDetailMessages);

    ImGui::Spacing();
    renderMessageTable("Fault Event Stream", kFaultEventMessages);
}

void FaultPanel::findMessages()
{
    requestFound_ = false;
    requestMsgId_ = 0;

    watchIds_.clear();
    messageSignals_.clear();
    messageSeen_.clear();
    liveVals_.clear();

    if (!dbc_)
        return;

    for (const auto &msg : dbc_->messages())
    {
        if (!isWatchedMessage(msg.name))
            continue;

        if (msg.name == "DebugFaultRequest")
        {
            requestMsgId_ = msg.id;
            requestFound_ = true;
        }

        watchIds_.insert(msg.id);

        std::vector<std::string> sigs;
        sigs.reserve(msg.signals.size());
        for (const auto &sig : msg.signals)
        {
            sigs.push_back(sig.name);
            liveVals_.emplace(sig.name, LiveVal{});
        }
        messageSignals_[msg.name] = std::move(sigs);
        messageSeen_[msg.name] = false;
    }
}

void FaultPanel::sendRequest()
{
    if (!dbc_ || !bus_ || !requestFound_)
        return;

    std::vector<std::pair<std::string, double>> values = {
        {"DebugFaultCommandRequest", static_cast<double>(commandRequest_)},
        {"DebugFaultCommandTargetID", static_cast<double>(selectedFaultId_)},
    };

    CanFrame frame;
    if (!dbc_->encode(requestMsgId_, values, frame))
    {
        status_ = "Failed to encode DebugFaultRequest.";
        return;
    }

    if (!bus_->send(frame))
    {
        status_ = "Failed to send DebugFaultRequest.";
        return;
    }

    const char *faultLbl =
        dbc_->valueLabel("DebugFaultCommandTargetID", selectedFaultId_);
    const char *reqLbl =
        dbc_->valueLabel("DebugFaultCommandRequest", commandRequest_);

    char msg[256];
    if (faultLbl && reqLbl)
    {
        std::snprintf(
            msg,
            sizeof(msg),
            "Sent %s request for fault %d (%s).",
            reqLbl,
            selectedFaultId_,
            faultLbl);
    }
    else
    {
        std::snprintf(
            msg,
            sizeof(msg),
            "Sent request=%d targetFault=%d.",
            commandRequest_,
            selectedFaultId_);
    }
    status_ = msg;
}

void FaultPanel::renderMessageTable(
    const char *title, const std::vector<std::string> &messageOrder)
{
    ImGui::Text("%s", title);
    ImGui::Separator();

    constexpr ImGuiTableFlags kTbl = ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_SizingStretchProp;

    for (const std::string &msg : messageOrder)
    {
        auto sigIt = messageSignals_.find(msg);
        if (sigIt == messageSignals_.end())
            continue;

        const bool seen = messageSeen_[msg];
        if (seen)
            ImGui::Text("%s", msg.c_str());
        else
            ImGui::TextDisabled("%s (waiting)", msg.c_str());

        char tableId[128];
        std::snprintf(tableId, sizeof(tableId), "##FaultTbl_%s", msg.c_str());

        if (!ImGui::BeginTable(tableId, 3, kTbl))
            continue;

        ImGui::TableSetupColumn(
            "Signal", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn(
            "Value", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn(
            "Label", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableHeadersRow();

        for (const std::string &sig : sigIt->second)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(sig.c_str());

            ImGui::TableSetColumnIndex(1);
            renderSignalValue(liveVals_, sig);

            ImGui::TableSetColumnIndex(2);
            auto lvIt = liveVals_.find(sig);
            const bool has = (lvIt != liveVals_.end() && lvIt->second.received);
            if (!has || !dbc_)
            {
                ImGui::TextDisabled("-");
            }
            else
            {
                const int64_t iv = static_cast<int64_t>(lvIt->second.value);
                const char *lbl = dbc_->valueLabel(sig, iv);
                ImGui::TextDisabled("%s", lbl ? lbl : "-");
            }
        }

        ImGui::EndTable();
        ImGui::Spacing();
    }
}

void FaultPanel::renderSignalValue(
    const std::unordered_map<std::string, LiveVal> &liveVals,
    const std::string &sigName)
{
    auto it = liveVals.find(sigName);
    if (it == liveVals.end() || !it->second.received)
    {
        ImGui::TextDisabled("-");
        return;
    }

    const double v = it->second.value;
    const double iv = static_cast<double>(static_cast<int64_t>(v));
    if (std::fabs(v - iv) < 1e-6)
        ImGui::Text("%lld", static_cast<long long>(static_cast<int64_t>(v)));
    else
        ImGui::Text("%.3f", v);
}
