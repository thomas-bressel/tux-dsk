#include "tuxdskplugin.h"

#include <QAction>
#include <QApplication>
#include <QEvent>

#include "icons.h"
#include "managerwindow.h"

bool TuxDskPlugin::start(tuxape::PluginHost* host, QString*)
{
    host_ = host;
    action_ = host->addAction(tr("TuxDSK – &Disc Manager..."), QKeySequence(Qt::CTRL | Qt::Key_D));
    host->addButton(action_, discIcon());
    connect(action_, &QAction::triggered, this, &TuxDskPlugin::showWindow);
    // In the menu of the tiles selected in Find Graphics. The entry is
    // TuxAPE's too: it is there for as long as the plugin is loaded.
    if (QAction* tiles = host->addTilesAction(tr("Copy to &TuxDSK's Disc...")))
        connect(tiles, &QAction::triggered, this, &TuxDskPlugin::takeTiles);
    qApp->installEventFilter(this);
    return true;
}

void TuxDskPlugin::stop()
{
    qApp->removeEventFilter(this);
    // TuxAPE is unloading the library: the window goes as it is, without
    // asking about discs that were not saved.
    delete window_;
    // The entry and the button are TuxAPE's: it takes them back.
    action_ = nullptr;
    host_ = nullptr;
}

bool TuxDskPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && host_)
        host_->addButton(action_, discIcon());
    return false;
}

void TuxDskPlugin::takeTiles()
{
    // The question is asked over the window the menu was in.
    QWidget* asker = QApplication::activeWindow();
    if (!window_)
        window_ = new ManagerWindow(host_, host_->window());
    // The disc that has them now is shown, if it was not; the user stays
    // where the tiles are.
    if (window_->takeTiles(host_->selectedTiles(), asker ? asker : host_->window()) && !window_->isVisible())
        window_->show();
}

void TuxDskPlugin::showWindow()
{
    if (!window_)
        window_ = new ManagerWindow(host_, host_->window());
    window_->show();
    window_->raise();
    window_->activateWindow();
}
