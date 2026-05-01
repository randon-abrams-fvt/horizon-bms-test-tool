#include "ui/FaultPanel.h"

#include "dbc/DbcModel.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr const char *kSafetyFaultsMessage = "safety_app_faults_1";
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

    if (msgName != kSafetyFaultsMessage)
        return;

    faultsMessageSeen_ = true;
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
    ImGui::Text("Fault Status");
    ImGui::Separator();

    if (!faultsMessageFound_)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            "%s not found in fault DBC.",
            kSafetyFaultsMessage);
        return;
    }

    ImGui::TextDisabled(
        "Message: %s   ID: 0x%08X", kSafetyFaultsMessage, faultsMsgId_);
    if (!faultsMessageSeen_)
        ImGui::TextDisabled("Waiting for first frame...");

    ImGui::Spacing();
    renderFaultGrid();
}

void FaultPanel::findMessages()
{
    faultsMessageFound_ = false;
    faultsMessageSeen_ = false;
    faultsMsgId_ = 0;

    watchIds_.clear();
    liveVals_.clear();
    faultSignals_.clear();

    if (!dbc_)
        return;

    for (const auto &msg : dbc_->messages())
    {
        if (msg.name != kSafetyFaultsMessage)
            continue;

        faultsMsgId_ = msg.id;
        faultsMessageFound_ = true;
        watchIds_.insert(msg.id);

        std::vector<std::string> names;
        names.reserve(msg.signals.size());
        for (const auto &sig : msg.signals)
        {
            names.push_back(sig.name);
            liveVals_.emplace(sig.name, LiveVal{});
        }

        faultSignals_.reserve(names.size());
        for (const std::string &sigName : names)
        {
            auto liveIt = liveVals_.find(sigName);
            if (liveIt != liveVals_.end())
                faultSignals_.push_back(FaultSignal{sigName, &liveIt->second});
        }
        break;
    }
}

void FaultPanel::renderFaultGrid()
{
    if (faultSignals_.empty())
    {
        ImGui::TextDisabled("No fault signals configured in message.");
        return;
    }

    const float minCellWidth = 230.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float availW = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(
        1, static_cast<int>((availW + spacing) / (minCellWidth + spacing)));

    constexpr ImGuiTableFlags kGrid = ImGuiTableFlags_SizingStretchSame;
    if (!ImGui::BeginTable("##SafetyFaultGrid", cols, kGrid))
        return;

    for (size_t i = 0; i < faultSignals_.size(); ++i)
    {
        const FaultSignal &fault = faultSignals_[i];
        const LiveVal &lv = *fault.live;

        const bool hasValue = lv.received;
        const int64_t iv = static_cast<int64_t>(lv.value);
        const bool isActive = hasValue && (std::fabs(lv.value) > 1e-6);

        const ImVec4 bg = !hasValue
                              ? ImVec4(0.22f, 0.22f, 0.22f, 0.80f)
                              : (isActive ? ImVec4(0.55f, 0.16f, 0.16f, 0.86f)
                                          : ImVec4(0.16f, 0.36f, 0.20f, 0.86f));
        const ImVec4 border =
            !hasValue ? ImVec4(0.55f, 0.55f, 0.55f, 0.50f)
                      : (isActive ? ImVec4(1.00f, 0.42f, 0.42f, 0.92f)
                                  : ImVec4(0.50f, 1.00f, 0.56f, 0.92f));

        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(i));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        if (ImGui::BeginChild(
                "##FaultCell",
                ImVec2(0.0f, 88.0f),
                true,
                ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse))
        {
            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(
                p0, p1, ImGui::ColorConvertFloat4ToU32(border), 4.0f, 0, 1.6f);

            ImGui::TextWrapped("%s", fault.name.c_str());
            ImGui::Separator();

            if (!hasValue)
            {
                ImGui::TextDisabled("NO DATA");
            }
            else
            {
                ImGui::TextColored(
                    isActive ? ImVec4(1.0f, 0.86f, 0.86f, 1.0f)
                             : ImVec4(0.86f, 1.0f, 0.88f, 1.0f),
                    "%s",
                    isActive ? "ACTIVE" : "CLEAR");

                const char *lbl =
                    dbc_ ? dbc_->valueLabel(fault.name, iv) : nullptr;
                if (lbl)
                    ImGui::TextDisabled(
                        "raw: %lld  (%s)", static_cast<long long>(iv), lbl);
                else
                    ImGui::TextDisabled(
                        "raw: %lld", static_cast<long long>(iv));
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ImGui::EndTable();
}
