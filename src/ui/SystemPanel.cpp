#include "ui/SystemPanel.h"

#include "can/CanBus.h"
#include "dbc/DbcModel.h"
#include "imgui.h"

#include <chrono>
#include <cstdio>

static double spNowMs()
{
    using namespace std::chrono;
    static const auto kStart = steady_clock::now();
    return duration<double, std::milli>(steady_clock::now() - kStart).count();
}

// ── Human-readable labels for 4-bit contactor / state signals ─────────────
static const char *contactorLabel(int v)
{
    switch (v)
    {
    case 0:
        return "Open";
    case 1:
        return "Closed";
    case 2:
        return "Closing";
    case 3:
        return "Opening";
    case 14:
        return "Fault";
    case 15:
        return "Unknown";
    default:
        return "—";
    }
}

// ── Public API ───────────────────────────────────────────────────────────────

void SystemPanel::setDbc(const DbcModel *dbc)
{
    dbc_ = dbc;
    findMessages();
}

void SystemPanel::setCanBus(CanBus *bus)
{
    bus_ = bus;
}

void SystemPanel::pushFrame(const DecodedFrame &frame)
{
    if (!statusFound_ || frame.raw.id != statusId_)
        return;

    const double now = spNowMs();
    for (const auto &[sigName, val] : frame.signals)
    {
        for (auto &sv : statusVals_)
        {
            if (sv.name == sigName)
            {
                sv.value = val;
                sv.received = true;
                sv.timeMs = now;
                break;
            }
        }
    }
}

void SystemPanel::render()
{
    // Cyclic TX check
    if (cyclicEnabled_ && bus_ && bus_->isOpen())
    {
        const double now = spNowMs();
        if (now - lastSentMs_ >= static_cast<double>(cycleMs_))
        {
            transmit();
            lastSentMs_ = now;
        }
    }

    const float halfW =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) *
        0.5f;

    // ── Left: Commands ───────────────────────────────────────────────────
    ImGui::BeginChild("##CmdPane", ImVec2(halfW, 0.0f), true);
    renderCommands();
    ImGui::EndChild();

    ImGui::SameLine();

    // ── Right: Status ────────────────────────────────────────────────────
    ImGui::BeginChild("##StatusPane", ImVec2(0.0f, 0.0f), true);
    renderStatus();
    ImGui::EndChild();
}

// ── Private ──────────────────────────────────────────────────────────────────

void SystemPanel::findMessages()
{
    cmdFound_ = false;
    statusFound_ = false;
    statusVals_.clear();
    if (!dbc_)
        return;

    for (const auto &msg : dbc_->messages())
    {
        if (msg.name == "system_commands")
        {
            cmdId_ = msg.id;
            cmdFound_ = true;
        }
        else if (msg.name == "system_status")
        {
            statusId_ = msg.id;
            statusFound_ = true;
            for (const auto &sig : msg.signals)
            {
                StatusVal sv;
                sv.name = sig.name;
                statusVals_.push_back(std::move(sv));
            }
        }
    }
}

void SystemPanel::transmit()
{
    if (!dbc_ || !bus_ || !cmdFound_)
        return;

    // system_commands has scale=1, offset=0 for all signals so physical == raw
    std::vector<std::pair<std::string, double>> vals = {
        {"operation_req", operationReq_ ? 1.0 : 0.0},
        {"enable_imd", enableImd_ ? 1.0 : 0.0},
        {"service_request", serviceRequest_ ? 1.0 : 0.0},
        {"external_bus_disconnected", externalBusDisconnected_ ? 1.0 : 0.0},
    };

    CanFrame frame;
    if (dbc_->encode(cmdId_, vals, frame))
        bus_->send(frame);
}

void SystemPanel::renderCommands()
{
    ImGui::TextDisabled("BMU  \u2192  BMS");
    ImGui::Text("system_commands");
    if (!cmdFound_)
    {
        ImGui::TextColored(
            ImVec4(1, 0.4f, 0.4f, 1), "Message not found in DBC");
        return;
    }
    ImGui::Text("ID: 0x%08X", cmdId_);
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::Checkbox("operation_req", &operationReq_);
    ImGui::Checkbox("enable_imd", &enableImd_);
    ImGui::Checkbox("service_request", &serviceRequest_);
    ImGui::Checkbox("external_bus_disconnected", &externalBusDisconnected_);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Cyclic controls
    ImGui::Checkbox("Cyclic TX", &cyclicEnabled_);
    if (cyclicEnabled_)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("ms##cyc", &cycleMs_, 10.0f, 100.0f, "%.0f");
        if (cycleMs_ < 1.0f)
            cycleMs_ = 1.0f;
    }

    ImGui::Spacing();
    const bool canSend = bus_ && bus_->isOpen();
    if (!canSend)
        ImGui::BeginDisabled();
    if (ImGui::Button("Send Once", ImVec2(100.0f, 0.0f)))
    {
        transmit();
        lastSentMs_ = spNowMs();
    }
    if (!canSend)
    {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(CAN not connected)");
    }
}

void SystemPanel::renderStatus()
{
    ImGui::TextDisabled("BMS  \u2192  BMU");
    ImGui::Text("system_status");
    if (!statusFound_)
    {
        ImGui::TextColored(
            ImVec4(1, 0.4f, 0.4f, 1), "Message not found in DBC");
        return;
    }
    ImGui::Text("ID: 0x%08X", statusId_);
    ImGui::Separator();

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;

    if (!ImGui::BeginTable("##StatusTbl", 3, kFlags))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableHeadersRow();

    for (const auto &sv : statusVals_)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(sv.name.c_str());

        ImGui::TableSetColumnIndex(1);
        if (!sv.received)
        {
            ImGui::TextDisabled("—");
        }
        else
        {
            // Color: green for "safe" values, yellow/red for notable states
            const int iv = static_cast<int>(sv.value);
            if (sv.name == "disconnect_forewarning")
            {
                if (iv == 0)
                    ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), "0");
                else
                    ImGui::TextColored(ImVec4(1, 0.8f, 0.0f, 1), "1");
            }
            else
            {
                ImGui::Text("%d", iv);
            }
        }

        ImGui::TableSetColumnIndex(2);
        if (sv.received)
        {
            const int iv = static_cast<int>(sv.value);
            // Show a friendly label for contactor/state signals
            if (sv.name.find("contactor") != std::string::npos)
                ImGui::TextDisabled("%s", contactorLabel(iv));
            else if (sv.name == "disconnect_forewarning")
                ImGui::TextDisabled(iv ? "WARN" : "OK");
        }
    }

    ImGui::EndTable();
}
