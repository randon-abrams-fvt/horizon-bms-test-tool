#include <windows.h>

#include <commdlg.h>

#include "App.h"
#include "can/VirtualInterface.h"
#include "imgui.h"
#ifdef BMU_PCAN_AVAILABLE
#include "can/PcanInterface.h"
#endif

static constexpr int kBaudrates[] = {1000000, 500000, 250000, 125000};
static constexpr const char *kBaudrateLabels[] = {
    "1 Mbit/s", "500 kbit/s", "250 kbit/s", "125 kbit/s"};

App::App() = default;
App::~App()
{
    disconnectCan();
}

void App::render()
{
    // ── Full-screen host window ───────────────────────────────────────────
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##Host", nullptr, kHostFlags);
    ImGui::PopStyleVar(2);

    renderMenuBar();

    // ── Drain CAN RX frames and distribute to all panels ─────────────────
    if (bus_ && bus_->isOpen())
    {
        auto frames = bus_->drainRxFrames();
        for (const auto &f : frames)
        {
            busMonitor_.pushFrame(f);
            systemPanel_.pushFrame(f);
            plotPanel_.pushFrame(f);
        }
    }

    // ── Tab bar ───────────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("##MainTabs"))
    {
        if (ImGui::BeginTabItem("Bus Monitor"))
        {
            busMonitor_.render();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("System Control"))
        {
            systemPanel_.render();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Plots"))
        {
            plotPanel_.render();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    if (showConnectWindow_)
        renderConnectWindow();
}

void App::renderMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open DBC\u2026", "Ctrl+O"))
            openDbcDialog();
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            PostQuitMessage(0);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("CAN"))
    {
        const bool connected = bus_ && bus_->isOpen();
        if (!connected && ImGui::MenuItem("Connect\u2026"))
            showConnectWindow_ = true;
        if (connected && ImGui::MenuItem("Disconnect"))
            disconnectCan();
        ImGui::EndMenu();
    }

    // Right-aligned status indicators
    const char *busLabel =
        (bus_ && bus_->isOpen()) ? "[CAN connected]" : "[CAN disconnected]";
    const char *dbcLabel = dbc_.isLoaded() ? dbc_.filePath().c_str() : "No DBC";

    float rightWidth = ImGui::CalcTextSize(busLabel).x +
                       ImGui::CalcTextSize("  ").x +
                       ImGui::CalcTextSize(dbcLabel).x + 16.0f;
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - rightWidth);
    ImGui::TextDisabled("%s  %s", busLabel, dbcLabel);

    ImGui::EndMenuBar();
}

void App::renderConnectWindow()
{
    ImGui::SetNextWindowSize(ImVec2(360.0f, 180.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(
            "CAN Connection",
            &showConnectWindow_,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    const char *ifaceItems[] = {"Virtual (loopback)", "PCAN-Basic (USB)"};
    ImGui::Combo("Interface", &ifaceType_, ifaceItems, 2);

    if (ifaceType_ == 1)
    {
        ImGui::InputInt(
            "PCAN Channel (hex)",
            &pcanChannel_,
            0,
            0,
            ImGuiInputTextFlags_CharsHexadecimal);
    }

    ImGui::Combo("Baud rate", &baudrateIdx_, kBaudrateLabels, 4);

    ImGui::Spacing();
    if (ImGui::Button("Connect", ImVec2(110.0f, 0.0f)))
    {
        connectCan();
        showConnectWindow_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
        showConnectWindow_ = false;

    ImGui::End();
}

void App::openDbcDialog()
{
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"DBC Files\0*.dbc\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Open DBC File";

    if (!GetOpenFileNameW(&ofn))
        return;

    char narrow[MAX_PATH] = {};
    WideCharToMultiByte(
        CP_UTF8, 0, path, -1, narrow, MAX_PATH, nullptr, nullptr);
    loadDbc(narrow);
}

void App::loadDbc(const std::string &path)
{
    if (!dbc_.load(path))
        return;

    systemPanel_.setDbc(&dbc_);
    plotPanel_.setDbc(&dbc_);
    if (bus_)
        bus_->setDbc(&dbc_);
}

void App::connectCan()
{
    disconnectCan();

    std::unique_ptr<ICanInterface> iface;
    if (ifaceType_ == 0)
    {
        iface = std::make_unique<VirtualInterface>();
    }
#ifdef BMU_PCAN_AVAILABLE
    else
    {
        iface = std::make_unique<PcanInterface>();
    }
#else
    else
    {
        return; // PCAN not compiled in
    }
#endif

    bus_ = std::make_unique<CanBus>(std::move(iface));
    if (dbc_.isLoaded())
        bus_->setDbc(&dbc_);
    systemPanel_.setCanBus(bus_.get());

    const uint32_t baud = static_cast<uint32_t>(kBaudrates[baudrateIdx_]);
    if (!bus_->open(static_cast<uint32_t>(pcanChannel_), baud))
    {
        bus_.reset();
        systemPanel_.setCanBus(nullptr);
    }
}

void App::disconnectCan()
{
    if (bus_)
    {
        bus_->close();
        bus_.reset();
        systemPanel_.setCanBus(nullptr);
    }
}
