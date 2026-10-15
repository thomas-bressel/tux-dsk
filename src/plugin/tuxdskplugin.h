#pragma once

#include <QObject>
#include <QPointer>

#include "plugin.h"

class ManagerWindow;

// What TuxAPE loads: it puts TuxDSK's entry in the Plugins menu and its
// button on the control panel, and shows the manager's window when either
// is chosen. It has an entry in the menu of TuxAPE's Find Graphics window
// too: the sprites or tiles selected there are put on the manager's disc,
// as a file.
class TuxDskPlugin : public QObject, public tuxape::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.tuxape.Plugin/1")
    Q_INTERFACES(tuxape::Plugin)

public:
    bool start(tuxape::PluginHost* host, QString* error) override;
    void stop() override;

protected:
    // The application's look has changed: the button's picture is drawn
    // again for it.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    tuxape::PluginHost* host_ = nullptr;
    QAction* action_ = nullptr;
    QPointer<ManagerWindow> window_;

    void showWindow();
    void takeTiles();
};
