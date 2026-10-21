// The plugin as TuxAPE finds it: the library loaded with QPluginLoader and
// taken only if it is a tuxape::Plugin, started with a host that only notes
// what it is asked, its entry chosen, its window shown, its entry of Find
// Graphics' menu chosen and the sprites selected there put on a disc, a
// file put there as another plugin puts one, and the plugin stopped, then
// unloaded. Then once more in French, as TuxAPE starts it when its windows
// are in French, and in a language TuxDSK has no translation for. Runs
// without a display (QT_QPA_PLATFORM=offscreen).
//
//   plugin_window [prefix]   also saves a picture of the window as
//                            <prefix>tuxdsk_plugin.png

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPluginLoader>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

#include "check.h"
#include "host.h"

namespace {

QDialog* managerWindow()
{
    for (QWidget* widget : QApplication::topLevelWidgets())
        if (widget->objectName() == "TuxDskWindow")
            return qobject_cast<QDialog*>(widget);
    return nullptr;
}

// One load of the plugin, in the language the host speaks: the texts of its
// entries and its window, as that language has them. In English, the
// sprites of Find Graphics and a file of another plugin are also put on its
// disc, and the window saved as pictures if `prefix` says where.
struct Run {
    QString entry;
    QString tilesEntry;
    QString title;
    QString disc;
};

Run runOnce(const QString& language, const QString& folder, const QString& prefix)
{
    Run run;
    QPluginLoader loader(QStringLiteral(TUXDSK_PLUGIN));
    QObject* instance = loader.instance();
    CHECK(instance != nullptr);
    if (!instance) {
        std::printf("  %s\n", qPrintable(loader.errorString()));
        return run;
    }
    // The interface TuxAPE asks of a plugin.
    CHECK(loader.metaData().value("IID").toString() == QStringLiteral("org.tuxape.Plugin/1"));
    auto* plugin = qobject_cast<tuxape::Plugin*>(instance);
    CHECK(plugin != nullptr);
    if (!plugin)
        return run;

    {
        Host host(folder);
        host.speaks = language;
        QString error;
        CHECK(plugin->start(&host, &error) && error.isEmpty());
        // One entry, with its key and its button; no window until it is chosen.
        CHECK_EQ(host.actions.size(), 1);
        CHECK(host.actions.size() == 1 && host.actions[0]->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F6));
        CHECK(host.buttons == 1 && host.pictures == 1);
        CHECK(managerWindow() == nullptr);
        // A change of look has the button's picture drawn again.
        QEvent look(QEvent::ApplicationPaletteChange);
        QApplication::sendEvent(qApp, &look);
        CHECK_EQ(host.pictures, 2);
        if (host.actions.isEmpty() || host.tilesActions.size() != 1)
            return run;
        run.entry = host.actions[0]->text();
        run.tilesEntry = host.tilesActions[0]->text();

        host.actions[0]->trigger();
        QDialog* window = managerWindow();
        CHECK(window && window->isVisible() && window->parentWidget() == &host.frame);
        run.title = window ? window->windowTitle() : QString();
        // Chosen again, it is the same window.
        host.actions[0]->trigger();
        CHECK(managerWindow() == window);

        // No disc yet: the drives' buttons wait for one.
        QWidget* disc = window ? window->findChild<QWidget*>("pnDiscA") : nullptr;
        auto* insertA = disc ? disc->findChild<QPushButton*>("bInsertA") : nullptr;
        auto* title = disc ? disc->findChild<QLabel*>("lTitle") : nullptr;
        CHECK(insertA && !insertA->isEnabled());
        run.disc = title ? title->text() : QString();
        CHECK(window && window->findChild<QWidget*>("pnDiscB") && !window->findChild<QWidget*>("pnDiscB")->isVisible());
        if (window && !prefix.isEmpty()) {
            QTest::qWait(50);
            window->grab().save(prefix + "tuxdsk_plugin.png");
        }

        // Its choices are kept in the folder TuxAPE gives it.
        CHECK(QFile::exists(QDir(folder).filePath("TuxDSK.ini")));

        // Its entry of Find Graphics' menu: chosen, it asks what the file of
        // the sprites selected is to be called, makes a disc for it, there
        // being none, and puts the file on it.
        if (language == "en" && window) {
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
            auto* files = disc ? disc->findChild<QTreeWidget*>("lvFiles") : nullptr;
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

        // What it offers other plugins, called by name as they call it: a
        // file put on its first disc as it is, and the name of that disc.
        if (language == "en" && window) {
            auto* files = disc ? disc->findChild<QTreeWidget*>("lvFiles") : nullptr;
            QString named;
            CHECK(QMetaObject::invokeMethod(instance, "discName", Q_RETURN_ARG(QString, named)) && named == "New disc");
            const auto put = [instance](const char* name, const QByteArray& content, bool replace) {
                bool done = false;
                const bool called = QMetaObject::invokeMethod(instance, "putFile", Q_RETURN_ARG(bool, done),
                                                              Q_ARG(QString, QString::fromLatin1(name)),
                                                              Q_ARG(QByteArray, content), Q_ARG(bool, replace));
                return called && done;
            };
            CHECK(put("OTHER.BIN", QByteArray(300, 'z'), false));
            CHECK(files && files->topLevelItemCount() == 2);
            // Put again, it asks first: the question closed, nothing is
            // replaced. Told to replace, it does without asking.
            QTimer::singleShot(0, [] {
                if (QWidget* box = QApplication::activeModalWidget())
                    box->close();
            });
            CHECK(!put("OTHER.BIN", QByteArray(10, 'y'), false));
            CHECK(put("OTHER.BIN", QByteArray(10, 'y'), true));
            CHECK(files && files->topLevelItemCount() == 2);
        }

        // Stopped, it leaves nothing of its own behind.
        plugin->stop();
        CHECK(managerWindow() == nullptr);
    }
    // Its translation is gone with it.
    CHECK(QCoreApplication::translate("tuxdsk::ManagerWindow", "TuxDSK – Disc Manager") == QString::fromUtf8("TuxDSK – Disc Manager"));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(loader.unload());
    return run;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QTemporaryDir folder;

    // In English, the language of the sources.
    const Run english = runOnce("en", folder.path(), prefix);
    CHECK(english.entry == QString::fromUtf8("TuxDSK – &Disc Manager..."));
    CHECK(english.tilesEntry == "Copy to &TuxDSK's Disc...");
    CHECK(english.title == QString::fromUtf8("TuxDSK – Disc Manager"));
    CHECK(english.disc.startsWith("No DSK loaded"));

    // In French: TuxAPE's language, TuxDSK's own translation.
    const Run french = runOnce("fr", folder.path(), QString());
    CHECK(french.entry == QString::fromUtf8("TuxDSK – &Gestionnaire de disquettes..."));
    CHECK(french.tilesEntry == QString::fromUtf8("Copier sur la disquette de &TuxDSK..."));
    CHECK(french.title == QString::fromUtf8("TuxDSK – Gestionnaire de disquettes"));
    CHECK(french.disc.startsWith(QString::fromUtf8("Pas de DSK chargé")));

    // A language TuxDSK has no translation for: English.
    const Run other = runOnce("xx", folder.path(), QString());
    CHECK(other.entry == english.entry && other.disc == english.disc);
    return checkSummary("plugin_window");
}
