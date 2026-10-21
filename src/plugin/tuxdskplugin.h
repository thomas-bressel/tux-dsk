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
//
// Other plugins find it by its id, "tuxdsk", through TuxAPE (version 3 of
// plugin.h), and call by name, with QMetaObject, what it offers them: a
// file put on the manager's first disc, and the name of that disc.
class TuxDskPlugin : public QObject, public tuxape::Plugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.tuxape.Plugin/1")
    Q_INTERFACES(tuxape::Plugin)

public:
    bool start(tuxape::PluginHost* host, QString* error) override;
    void stop() override;

    // A file put on the first disc as it is, its AMSDOS header in
    // `content` if it is to have one; a disc is made if there is none, and
    // the window shows it. A file of that name is replaced if `replace`,
    // else only once the user says so. False if the file is not there:
    // the user has been told why.
    Q_INVOKABLE bool putFile(const QString& name, const QByteArray& content, bool replace);
    // What the first disc is called in the window; empty if it has none.
    Q_INVOKABLE QString discName() const;

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
