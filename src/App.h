#pragma once

#include "can/CanBus.h"
#include "dbc/DbcModel.h"
#include "ui/BusMonitorPanel.h"
#include "ui/PlotPanel.h"
#include "ui/SystemPanel.h"

#include <memory>
#include <string>

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
    void openDbcDialog();
    void loadDbc(const std::string &path);
    void connectCan();
    void disconnectCan();

    DbcModel dbc_;
    std::unique_ptr<CanBus> bus_;

    BusMonitorPanel busMonitor_;
    SystemPanel systemPanel_;
    PlotPanel plotPanel_;

    // Connection dialog state
    bool showConnectWindow_ = false;
    int ifaceType_ = 0;      // 0 = Virtual, 1 = PCAN
    int pcanChannel_ = 0x51; // PCAN_USBBUS1
    int baudrateIdx_ = 1;    // index into kBaudrates[]
};
