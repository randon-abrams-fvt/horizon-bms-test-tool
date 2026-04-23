#pragma once

#include "can/CanBus.h"
#include "dbc/DbcModel.h"
#include "ui/BusMonitorPanel.h"
#include "ui/PlotPanel.h"
#include "ui/SystemPanel.h"

#include <memory>
#include <string>
#include <vector>

#include "can/PcanInterface.h"

/// Top-level application class.
/// Owns the DBC model, CAN bus, and all UI panels.
class App
{
  public:
    App();
    ~App();

    /// Called once per render frame by the main loop.
    void render();

  private:
    void renderMenuBar();
    void renderConnectWindow();
    void renderSettingsWindow();
    void applyTheme();
    void connectCan();
    void disconnectCan();

    DbcModel dbc_;
    std::unique_ptr<CanBus> bus_;

    BusMonitorPanel busMonitor_;
    SystemPanel systemPanel_;
    PlotPanel plotPanel_;

    // Connection dialog state
    bool showConnectWindow_ = false;
    int ifaceType_ = 0;   // 0 = Virtual, 1 = PCAN
    int baudrateIdx_ = 1; // index into kBaudrates[]

    // Application settings window state
    bool showSettingsWindow_ = false;
    float textScale_ = 1.0f;
    int themeIdx_ = 0; // 0 = Dark, 1 = Light, 2 = Classic
    bool dockingEnabled_ = true;
    bool showDemoWindow_ = false;
    bool showMetricsWindow_ = false;

    std::vector<PcanDeviceInfo> pcanDevices_;
    int pcanDeviceIdx_ = 0;
};
