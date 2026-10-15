// The plugin as TuxAPE finds it: the library loaded, started with a host
// that only notes what it is asked, its entry chosen, its window shown, its
// entry of Find Graphics' menu chosen and the sprites selected there put on
// a disc, and the plugin stopped. Runs without a display
// (QT_QPA_PLATFORM=offscreen).
//
//   plugin_window [prefix]   also saves a picture of the window as
//                            <prefix>tuxdsk_plugin.png

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QPluginLoader>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

#include "check.h"
#include "plugin.h"

namespace {

// A TuxAPE of the test's own: a window, and a note of what was added and
// asked.
class Host : public tuxape::PluginHost {
public:
    explicit Host(const QString& folder)
        : folder_(folder)
    {
    }

    QMainWindow frame;
    QList<QAction*> actions;
    int buttons = 0;
    int pictures = 0;
    QStringList inserted;  // "0:/path/disc.dsk"
    QStringList typed;
    QList<QAction*> tilesActions;  // its entries of Find Graphics' menu
    tuxape::PluginTiles tiles;     // the tiles selected there

    QWidget* window() override { return &frame; }
    QAction* addAction(const QString& text, const QKeySequence& shortcut) override
    {
        auto* action = new QAction(text, &frame);
        action->setShortcut(shortcut);
        actions << action;
        return action;
    }
    QAction* addTilesAction(const QString& text) override
    {
        tilesActions << new QAction(text, &frame);
        return tilesActions.last();
    }
    tuxape::PluginTiles selectedTiles() override { return tiles; }
    void addButton(QAction* action, const QIcon& icon) override
    {
        if (actions.contains(action) && !icon.isNull())
            ++pictures;
        buttons = 1;
    }
    bool insertDisc(int drive, const QString& path) override
    {
        inserted << QString::number(drive) + ':' + path;
        return QFile::exists(path);
    }
    bool loadSnapshot(const QString&) override { return false; }
    void autoType(const QString& text) override { typed << text; }
    void reset(bool) override {}
    QByteArray readMemory(int, int size) override { return QByteArray(size, '\0'); }
    tuxape::PluginScreen screen() override { return {}; }
    void openInAssembler(const QString&) override {}
    bool assembleAndRun(const QString&) override { return false; }
    QString dataFolder() override { return folder_; }

private:
    QString folder_;
};

QDialog* managerWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
        if (widget->objectName() == "TuxDskWindow")
            return qobject_cast<QDialog*>(widget);
    return nullptr;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir scratch;

    QPluginLoader loader(QStringLiteral(TUXDSK_PLUGIN));
    QObject* instance = loader.instance();
    CHECK(instance != nullptr);
    if (!instance) {
        std::printf("  %s\n", qPrintable(loader.errorString()));
        return checkSummary("plugin_window");
    }
    auto* plugin = qobject_cast<tuxape::Plugin*>(instance);
    CHECK(plugin != nullptr);
    if (!plugin)
        return checkSummary("plugin_window");

    {
        Host host(scratch.path());
        QString error;
        CHECK(plugin->start(&host, &error) && error.isEmpty());
        // One entry, with its key and its button; no window until it is chosen.
        CHECK_EQ(host.actions.size(), 1);
        CHECK(host.actions.size() == 1 && host.actions[0]->text() == "TuxDSK – &Disc Manager...");
        CHECK(host.actions.size() == 1 && host.actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_D));
        CHECK(host.buttons == 1 && host.pictures == 1);
        CHECK(managerWindow() == nullptr);
        // A change of look has the button's picture drawn again.
        QEvent look(QEvent::ApplicationPaletteChange);
        QApplication::sendEvent(&app, &look);
        CHECK_EQ(host.pictures, 2);

        host.actions[0]->trigger();
        QDialog* window = managerWindow();
        CHECK(window && window->isVisible() && window->parentWidget() == &host.frame);
        CHECK(window && window->windowTitle() == "TuxDSK – Disc Manager");
        // Chosen again, it is the same window.
        host.actions[0]->trigger();
        CHECK(managerWindow() == window);

        // No disc yet: the drives' buttons wait for one.
        QWidget* disc = window ? window->findChild<QWidget*>("DiscA") : nullptr;
        auto* insertA = disc ? disc->findChild<QPushButton*>("bInsertA") : nullptr;
        auto* title = disc ? disc->findChild<QLabel*>("Title") : nullptr;
        CHECK(insertA && !insertA->isEnabled());
        CHECK(title && title->text().startsWith("No DSK loaded"));
        CHECK(window && window->findChild<QWidget*>("DiscB") && !window->findChild<QWidget*>("DiscB")->isVisible());
        if (window && !prefix.isEmpty()) {
            QTest::qWait(50);
            window->grab().save(prefix + "tuxdsk_plugin.png");
        }

        // Its choices are kept in the folder TuxAPE gives it.
        auto* clean = window ? window->findChild<QPushButton*>("bAbout") : nullptr;
        CHECK(clean != nullptr);
        CHECK(QFile::exists(QDir(scratch.path()).filePath("TuxDSK.ini")));

        // Its entry of Find Graphics' menu: chosen, it asks what the file of
        // the sprites selected is to be called, makes a disc for it, there
        // being none, and puts the file on it.
        CHECK_EQ(host.tilesActions.size(), 1);
        CHECK(host.tilesActions.size() == 1 && host.tilesActions[0]->text() == "Copy to &TuxDSK's Disc...");
        if (host.tilesActions.size() == 1 && window) {
            host.tiles.count = 3;
            host.tiles.width = host.tiles.height = 16;
            host.tiles.address = 0x4000;
            host.tiles.mode = 3;
            host.tiles.bytes = QByteArray(3 * 256, '\x05');
            window->hide();
            QTimer::singleShot(0, [] {
                auto* asking = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                CHECK(asking && asking->objectName() == "TilesDialog");
                if (asking)
                    asking->accept();
            });
            host.tilesActions[0]->trigger();
            // The disc is shown with its file, and is still to be saved.
            CHECK(window->isVisible());
            auto* files = disc ? disc->findChild<QTreeWidget*>("Files") : nullptr;
            CHECK(files && files->topLevelItemCount() == 1);
            if (files && files->topLevelItemCount() == 1) {
                const QTreeWidgetItem* file = files->topLevelItem(0);
                CHECK(file->text(0) == "SPRITE.SPR" && file->text(4) == "BINARY" && file->text(5) == "1 Kb");
                CHECK(file->text(6) == "&4000" && file->isSelected());
            }
            CHECK(title && title->text() == "New disc* - Inside DSK file (used Kb/free Kb: 1/186) - DATA format, 42 tracks");
            CHECK(insertA && insertA->isEnabled());
            if (!prefix.isEmpty()) {
                QTest::qWait(50);
                window->grab().save(prefix + "tuxdsk_sprites.png");
            }
            // With nothing selected there is nothing to ask: a box says so,
            // and is closed here.
            host.tiles = {};
            QTimer::singleShot(0, [] {
                if (QWidget* box = QApplication::activeModalWidget())
                    box->close();
            });
            host.tilesActions[0]->trigger();
            CHECK(files && files->topLevelItemCount() == 1);
        }

        // Stopped, it leaves nothing of its own behind.
        plugin->stop();
        CHECK(managerWindow() == nullptr);
    }
    CHECK(loader.unload());
    return checkSummary("plugin_window");
}
