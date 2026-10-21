#include "tuxdskplugin.h"

#include <QAction>
#include <QApplication>
#include <QEvent>

#include "icons.h"
#include "managerwindow.h"

namespace tuxdsk {

bool TuxDskPlugin::start(tuxape::PluginHost* host, QString*)
{
    host_ = host;
    // TuxDSK's texts in TuxAPE's language, before its first tr(): the
    // translations built into the library. None for English, the language
    // of the sources.
    if (translator_.load(QStringLiteral(":/i18n/tuxdsk/tuxdsk_%1.qm").arg(host->language())))
        QCoreApplication::installTranslator(&translator_);
    // One of the keys TuxAPE leaves to plugins (see PLUGINS.md).
    action_ = host->addAction(tr("TuxDSK – &Disc Manager..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F6));
    host->addButton(action_, discIcon());
    connect(action_, &QAction::triggered, this, &TuxDskPlugin::showWindow);
    // In the menu of the tiles selected in Find Graphics. The entry is
    // TuxAPE's too: it is there for as long as the plugin is loaded.
    if (QAction* tiles = host->addTilesAction(tr("Copy to &TuxDSK's Disc...")))
        connect(tiles, &QAction::triggered, this, &TuxDskPlugin::takeTiles);
    // Qt tells the application when the look changes: its events are
    // watched for that one, which costs a comparison an event.
    qApp->installEventFilter(this);
    return true;
}

void TuxDskPlugin::stop()
{
    qApp->removeEventFilter(this);
    // TuxAPE unloads the library next: nothing of it may be left running,
    // nor its translator installed. The window goes as it is, without
    // asking about discs that were not saved.
    delete window_;
    QCoreApplication::removeTranslator(&translator_);
    // The entry and the button are TuxAPE's: it takes them back.
    action_ = nullptr;
    host_ = nullptr;
}

bool TuxDskPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && host_ && action_)
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

bool TuxDskPlugin::putFile(const QString& name, const QByteArray& content, bool replace)
{
    if (!host_)
        return false;
    // A question is asked over the window the plugin was asked from.
    QWidget* asker = QApplication::activeWindow();
    if (!window_)
        window_ = new ManagerWindow(host_, host_->window());
    const std::vector<uint8_t> bytes(content.begin(), content.end());
    if (!window_->takeFile(name, bytes, replace, asker ? asker : host_->window()))
        return false;
    if (!window_->isVisible())
        window_->show();
    return true;
}

QString TuxDskPlugin::discName() const
{
    return window_ && window_->pane(0)->hasDisc() ? window_->pane(0)->name() : QString();
}

void TuxDskPlugin::showWindow()
{
    if (!window_)
        window_ = new ManagerWindow(host_, host_->window());
    window_->show();
    window_->raise();
    window_->activateWindow();
}

}  // namespace tuxdsk
