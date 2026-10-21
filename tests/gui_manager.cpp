// The manager's window, driven as a user drives it, less the windows that
// ask for a file's name: a disc made, files added in ManageDsk's three
// import modes, renamed, erased and brought back, changed, exported with
// and without their headers; the second disc, files copied across, the
// discs compared and swapped; a disc saved and read, as a .DSK file and as
// an .HFE one; the files of a disc read from its file put on another; discs
// and files
// dropped on the list; a disc put in TuxAPE's drives and a file run there;
// sprites selected in TuxAPE's Find Graphics window put on the disc as a
// file; and the choices kept from one day to the next. Runs without a
// display (QT_QPA_PLATFORM=offscreen).
//
//   gui_manager [prefix]   also saves pictures of the window, in the light
//                          and in the dark look, as <prefix>tuxdsk_manager.png,
//                          <prefix>tuxdsk_manager_dark.png and
//                          <prefix>tuxdsk_two_discs.png

#include <QAction>
#include <QCheckBox>
#include <QDropEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMimeData>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QUrl>

#include "dialogs.h"
#include "disc/amsdos.h"
#include "disc/compare.h"
#include "disc/hfe.h"
#include "discpane.h"
#include "gui_looks.h"
#include "english.h"
#include "gui_test.h"
#include "managerwindow.h"
#include "plugin.h"
#include "preferences.h"

using golden::Bytes;
using gui::plan;
using prompts::Answer;

namespace {

class Host : public tuxape::PluginHost {
public:
    explicit Host(const QString& folder)
        : folder_(folder)
    {
    }

    QMainWindow frame;
    QStringList inserted;  // "0:/path/disc.dsk"
    QStringList typed;

    QWidget* window() override { return &frame; }
    QAction* addAction(const QString& text, const QKeySequence&) override { return new QAction(text, &frame); }
    void addButton(QAction*, const QIcon&) override {}
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

template <class Widget>
Widget* child(QWidget* parent, const char* name)
{
    auto* found = parent->findChild<Widget*>(name);
    if (!found) {
        std::printf("no control called %s\n", name);
        ++g_failures;
    }
    return found;
}

QString title(DiscPane* pane)
{
    return child<QLabel>(pane, "lTitle")->text();
}

// A line of the list, by the file's name; null if it has none.
QTreeWidgetItem* line(DiscPane* pane, const QString& name)
{
    for (int row = 0; row < pane->list()->topLevelItemCount(); ++row)
        if (pane->list()->topLevelItem(row)->text(0) == name)
            return pane->list()->topLevelItem(row);
    return nullptr;
}

std::optional<tuxdsk::DiscFile> file(DiscPane* pane, const char* name, int user = 0)
{
    return pane->disc()->find(name, user);
}

void drop(DiscPane* pane, const QStringList& paths)
{
    QMimeData data;
    QList<QUrl> urls;
    for (const QString& path : paths)
        urls << QUrl::fromLocalFile(path);
    data.setUrls(urls);
    // What is dragged comes in, is accepted, and is let go.
    QDragEnterEvent enter(QPoint(20, 60), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pane, &enter);
    CHECK(enter.isAccepted());
    QDropEvent event(QPointF(20, 60), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(pane, &event);
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const English english;
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    gui::answerBoxes();
    QTemporaryDir scratch;
    const QDir folder(scratch.path());
    const Bytes code = golden::input("code.bin");
    const Bytes headed = golden::input("withhdr.bin");

    {
        ManagerWindow window(nullptr);
        window.show();
        DiscPane* first = window.pane(0);
        DiscPane* second = window.pane(1);

        // ---- No disc yet ----
        CHECK(!first->hasDisc() && title(first).startsWith("No DSK loaded"));
        for (const char* name : {"bAdd", "bExport", "bRename", "bDelete", "bInfo", "bView", "bSectors", "bSave", "bSaveAs",
                                 "bInsertA", "bInsertB", "bRun"})
            CHECK(!child<QPushButton>(first, name)->isEnabled());
        CHECK(child<QPushButton>(first, "bNew")->isEnabled() && child<QPushButton>(first, "bOpen")->isEnabled());
        CHECK(!second->isVisible() && !window.twoDiscs());
        first->addFiles({gui::inputFile("code.bin")});
        CHECK(gui::g_told.size() == 1 && gui::g_told.last() == "No DSK loaded.");

        // ---- A disc made: the window that asks what it is to be ----
        gui::whenAsked([](QDialog* dialog) {
            CHECK(dialog->objectName() == "NewDiscDialog");
            CHECK_EQ(child<QSpinBox>(dialog, "sbTracks")->value(), 42);
            CHECK_EQ(child<QSpinBox>(dialog, "sbTracks")->maximum(), 42);
            child<QSpinBox>(dialog, "sbTracks")->setValue(40);
            dialog->accept();
        });
        first->chooseNew();
        CHECK(first->hasDisc() && !first->modified() && first->path().isEmpty());
        CHECK(first->disc()->image().tracks() == 40);
        CHECK(title(first) == "New disc - Inside DSK file (used Kb/free Kb: 0/178) - DATA format, 40 tracks");
        CHECK(child<QPushButton>(first, "bAdd")->isEnabled() && child<QPushButton>(first, "bSave")->isEnabled());
        CHECK(!child<QPushButton>(first, "bExport")->isEnabled());
        // Without a TuxAPE there is no drive to put it in.
        CHECK(!child<QPushButton>(first, "bInsertA")->isEnabled());
        first->newDisc();
        CHECK(first->disc()->image().tracks() == 42 && title(first).contains("0/187"));

        // ---- Files added: forced ASCII, forced binary ----
        child<QRadioButton>(&window, "rbImportAscii")->click();
        CHECK_EQ(window.options().importMode, ManagerOptions::ForceAscii);
        first->addFiles({gui::inputFile("hello.bas"), gui::inputFile("withhdr.bin")});
        CHECK(first->modified() && title(first).startsWith("New disc* - "));
        CHECK_EQ(first->list()->topLevelItemCount(), 2);
        // As ASCII a file loses the header it came with.
        CHECK(file(first, "WITHHDR.BIN") && file(first, "WITHHDR.BIN")->type == tuxdsk::FileType::Ascii);
        CHECK(file(first, "WITHHDR.BIN") && file(first, "WITHHDR.BIN")->records == 3);
        child<QRadioButton>(&window, "rbImportBinary")->click();
        first->addFiles({gui::inputFile("code.bin")});
        // As a binary one that has none is given one.
        auto added = file(first, "CODE.BIN");
        CHECK(added && added->type == tuxdsk::FileType::Binary && added->length == 300 && added->records == 4);
        QTreeWidgetItem* item = line(first, "CODE.BIN");
        CHECK(item && item->text(1) == "0" && item->text(2).isEmpty() && item->text(4) == "BINARY");
        CHECK(item && item->text(5) == "1 Kb" && item->text(6) == "&0000" && item->text(7) == "&0000");
        CHECK(line(first, "HELLO.BAS") && line(first, "HELLO.BAS")->text(4) == "ASCII");
        CHECK(line(first, "HELLO.BAS") && line(first, "HELLO.BAS")->text(6).isEmpty());
        CHECK(title(first).contains("used Kb/free Kb: 3/184"));

        // ---- The user's choice: ManageDsk's import window, for each file ----
        child<QRadioButton>(&window, "rbImportAsk")->click();
        gui::whenAsked([](QDialog* dialog) {
            CHECK(dialog->objectName() == "ImportDialog" && dialog->windowTitle() == QString::fromUtf8("TuxDSK – Import File k17.dat"));
            CHECK(child<QLabel>(dialog, "lHeader")->text() == "AMSDOS header not found.");
            // A file without a header is taken for an ASCII one.
            CHECK(child<QRadioButton>(dialog, "rbAscii")->isChecked());
            CHECK(!child<QSpinBox>(dialog, "sbStart")->isEnabled());
            child<QRadioButton>(dialog, "rbBinary")->click();
            child<QSpinBox>(dialog, "sbStart")->setValue(0x4000);
            child<QSpinBox>(dialog, "sbEntry")->setValue(0x4010);
            dialog->accept();
        });
        first->addFiles({gui::inputFile("k17.dat")});
        added = file(first, "K17.DAT");
        CHECK(added && added->type == tuxdsk::FileType::Binary && added->loadAddress == 0x4000);
        CHECK(added && added->entryAddress == 0x4010 && added->length == 17000 && added->entries.size() == 2);
        // Cancel stops there: the files that follow are not asked about.
        gui::whenAsked([](QDialog* dialog) {
            CHECK(child<QLabel>(dialog, "lHeader")->text() == "AMSDOS header found.");
            CHECK(child<QRadioButton>(dialog, "rbBinary")->isChecked());
            CHECK_EQ(child<QSpinBox>(dialog, "sbStart")->value(), 0x8000);
            dialog->reject();
        });
        first->addFiles({gui::inputFile("withhdr.bin"), gui::inputFile("noext")});
        CHECK_EQ(first->list()->topLevelItemCount(), 4);

        // ---- A file the disc has: ManageDsk asks ----
        child<QRadioButton>(&window, "rbImportBinary")->click();
        gui::g_asked.clear();
        plan(Answer::No);
        first->addFiles({gui::inputFile("withhdr.bin")});
        CHECK(gui::g_asked.size() == 1 && gui::g_asked.last() == "File WITHHDR.BIN already exists. Replace?");
        CHECK(file(first, "WITHHDR.BIN")->type == tuxdsk::FileType::Ascii);
        plan(Answer::Yes);
        first->addFiles({gui::inputFile("withhdr.bin")});
        added = file(first, "WITHHDR.BIN");
        CHECK(added && added->type == tuxdsk::FileType::Binary && added->loadAddress == 0x8000 && added->records == 4);
        CHECK_EQ(first->list()->topLevelItemCount(), 4);
        // The header as it came, rubbish and all: the clean-up is off.
        CHECK(first->disc()->read(*added)[0x1C] == 0xAA);
        child<QCheckBox>(&window, "ckClean")->setChecked(true);
        plan(Answer::Yes);
        first->addFiles({gui::inputFile("withhdr.bin")});
        added = file(first, "WITHHDR.BIN");
        CHECK(added && first->disc()->read(*added)[0x1C] == 0 && added->loadAddress == 0x8000);
        child<QCheckBox>(&window, "ckClean")->setChecked(false);
        CHECK(gui::g_answers.empty());

        // ---- Chosen in the list, the buttons work on it ----
        first->select({"CODE.BIN"});
        CHECK_EQ(first->selectedFiles().size(), 1);
        for (const char* name : {"bExport", "bRename", "bDelete", "bInfo", "bView"})
            CHECK(child<QPushButton>(first, name)->isEnabled());

        // ---- Exported: with its header, without, into a folder ----
        CHECK(first->exportFile(*file(first, "CODE.BIN"), folder.filePath("code_with.bin")));
        Bytes exported = gui::fileBytes(folder.filePath("code_with.bin"));
        CHECK_EQ(exported.size(), 428);
        CHECK(tuxdsk::hasAmsdosHeader(exported) && std::equal(code.begin(), code.end(), exported.begin() + 128));
        child<QRadioButton>(&window, "rbExportPlain")->click();
        CHECK(!window.options().exportHeader);
        CHECK(first->exportFile(*file(first, "CODE.BIN"), folder.filePath("code_plain.bin")));
        CHECK(gui::fileBytes(folder.filePath("code_plain.bin")) == code);
        child<QRadioButton>(&window, "rbExportHeader")->click();
        QDir().mkpath(folder.filePath("out"));
        first->select({"CODE.BIN", "HELLO.BAS", "K17.DAT"});
        CHECK_EQ(first->exportFiles(first->selectedFiles(), folder.filePath("out")), 3);
        CHECK_EQ(gui::fileBytes(folder.filePath("out/K17.DAT")).size(), 17128);
        // A file with no header is all its records.
        CHECK_EQ(gui::fileBytes(folder.filePath("out/HELLO.BAS")).size(), 128);
        // Files that are there are asked about: no for one, yes for the rest.
        gui::g_asked.clear();
        plan(Answer::No);
        plan(Answer::YesToAll);
        CHECK_EQ(first->exportFiles(first->selectedFiles(), folder.filePath("out")), 2);
        CHECK(gui::g_asked.size() == 2 && gui::g_asked.first() == "File HELLO.BAS already exists. Replace?");

        // ---- Renamed: ManageDsk's window, a name and a user ----
        first->select({"HELLO.BAS"});
        gui::whenAsked([](QDialog* dialog) {
            CHECK(dialog->objectName() == "RenameDialog");
            CHECK(child<QLineEdit>(dialog, "edName")->text() == "HELLO.BAS");
            child<QLineEdit>(dialog, "edName")->setText("salut.bas");
            child<QSpinBox>(dialog, "sbUser")->setValue(5);
            dialog->accept();
        });
        first->renameSelected();
        CHECK(!file(first, "HELLO.BAS") && file(first, "SALUT.BAS", 5));
        CHECK(line(first, "SALUT.BAS") && line(first, "SALUT.BAS")->text(1) == "5");
        // The name of another file is refused, and said.
        gui::g_told.clear();
        CHECK(!first->renameFile(*file(first, "SALUT.BAS", 5), "code.bin", 0));
        CHECK(gui::g_told.size() == 1 && gui::g_told.last() == "File code.bin already exists.");
        CHECK(first->renameFile(*file(first, "SALUT.BAS", 5), "HELLO.BAS", 0));

        // ---- Erased, in red, and brought back; then erased for good ----
        first->select({"HELLO.BAS"});
        gui::g_asked.clear();
        plan(Answer::Yes);
        first->deleteSelected();
        CHECK(gui::g_asked.size() == 1 && gui::g_asked.last() == "Do you really want to delete this file?\nHELLO.BAS");
        CHECK(!file(first, "HELLO.BAS"));
        item = line(first, "HELLO.BAS");
        CHECK(item && item->text(1) == "229");
        CHECK(item && line(first, "CODE.BIN") && item->foreground(0).color() != line(first, "CODE.BIN")->foreground(0).color());
        CHECK(item && item->foreground(0).color().red() > item->foreground(0).color().blue());
        CHECK_EQ(first->list()->topLevelItemCount(), 4);
        // Another user than 229 brings it back.
        CHECK(first->renameFile(*file(first, "HELLO.BAS", 229), "HELLO.BAS", 0));
        CHECK(file(first, "HELLO.BAS") && line(first, "HELLO.BAS")->text(1) == "0");
        Bytes hello = golden::input("hello.bas");
        hello.resize(128, 0);
        CHECK(first->disc()->read(*file(first, "HELLO.BAS")) == hello);
        // "Permanently delete" leaves nothing; No leaves the file.
        first->select({"HELLO.BAS", "K17.DAT"});
        CHECK_EQ(first->selectedFiles().size(), 2);
        plan(Answer::Yes, true);
        plan(Answer::No);
        first->deleteSelected();
        CHECK(!line(first, "HELLO.BAS") && file(first, "K17.DAT"));
        CHECK_EQ(first->list()->topLevelItemCount(), 3);

        // ---- Information about a file: what can be changed there ----
        first->select({"CODE.BIN"});
        gui::whenAsked([](QDialog* dialog) {
            CHECK(dialog->objectName() == "FileInfoDialog");
            auto* info = qobject_cast<FileInfoDialog*>(dialog);
            CHECK(info && info->type() == tuxdsk::FileType::Binary && info->loadAddress() == 0 && !info->readOnly());
            child<QSpinBox>(dialog, "sbStart")->setValue(0xA000);
            child<QSpinBox>(dialog, "sbEntry")->setValue(0xA003);
            child<QCheckBox>(dialog, "ckReadOnly")->setChecked(true);
            child<QCheckBox>(dialog, "ckHidden")->setChecked(true);
            dialog->accept();
        });
        first->showInformation();
        added = file(first, "CODE.BIN");
        CHECK(added && added->loadAddress == 0xA000 && added->entryAddress == 0xA003 && added->readOnly && added->hidden);
        CHECK(added && added->length == 300 && added->records == 4);
        item = line(first, "CODE.BIN");
        CHECK(item && item->text(2) == "Yes" && item->text(3) == "Yes" && item->text(6) == "&A000");
        // A binary made ASCII loses its header; made BASIC again it has one.
        CHECK(first->changeFile(*added, tuxdsk::FileType::Ascii, 0, 0, false, false));
        added = file(first, "CODE.BIN");
        CHECK(added && added->type == tuxdsk::FileType::Ascii && added->records == 3 && !added->readOnly);
        Bytes padded = code;
        padded.resize(384, 0);
        CHECK(added && first->disc()->read(*added) == padded);
        CHECK(first->changeFile(*added, tuxdsk::FileType::Basic, 0x170, 0, false, false));
        added = file(first, "CODE.BIN");
        CHECK(added && added->type == tuxdsk::FileType::Basic && added->loadAddress == 0x170 && added->length == 384);
        CHECK(line(first, "CODE.BIN")->text(4) == "BASIC");

        // ---- Figures in hexadecimal, and the directory as it is ----
        child<QCheckBox>(first, "ckHexadecimal")->setChecked(true);
        CHECK(line(first, "CODE.BIN")->text(5) == "&0180");
        child<QCheckBox>(first, "ckDirectory")->setChecked(true);
        CHECK_EQ(first->list()->topLevelItemCount(), 64);
        CHECK_EQ(first->list()->columnCount(), 6);
        CHECK(first->selectedFiles().empty() && !child<QPushButton>(first, "bDelete")->isEnabled());
        child<QCheckBox>(first, "ckHexadecimal")->setChecked(false);
        bool found = false;
        for (int row = 0; row < 64; ++row) {
            const QTreeWidgetItem* entry = first->list()->topLevelItem(row);
            if (entry->text(1) == "K17.DAT" && entry->text(3) == "1") {
                found = true;
                CHECK(entry->text(2) == "0" && entry->text(4) == "6" && entry->text(5).split(' ').size() == 1);
            }
        }
        CHECK(found);
        child<QCheckBox>(first, "ckDirectory")->setChecked(false);
        CHECK_EQ(first->list()->topLevelItemCount(), 3);
        if (!prefix.isEmpty()) {
            first->select({"K17.DAT"});
            CHECK(saveInBothLooks(&window, prefix + "tuxdsk_manager"));
        }

        // ---- Saved, and read again ----
        gui::g_told.clear();
        CHECK(!first->saveDiscAs(folder.filePath("no/such/folder/a.dsk")));
        CHECK(gui::g_told.size() == 1 && first->modified());
        CHECK(first->saveDiscAs(folder.filePath("first.dsk")));
        CHECK(!first->modified() && first->name() == "first.dsk" && title(first).startsWith("first.dsk - "));
        const Bytes saved = gui::fileBytes(folder.filePath("first.dsk"));
        CHECK(saved == first->disc()->image().bytes());

        // ---- Saved as an .HFE file too, for a Gotek or an HxC drive ----
        CHECK(DiscPane::savedPath("/tmp/disc", false) == "/tmp/disc.dsk" && DiscPane::savedPath("/tmp/disc", true) == "/tmp/disc.hfe");
        CHECK(DiscPane::savedPath("/tmp/disc.DSK", true) == "/tmp/disc.DSK" && DiscPane::savedPath("/tmp/disc.hfe", false) == "/tmp/disc.hfe");
        CHECK(DiscPane::savedPath("/tmp/my.disc", true) == "/tmp/my.disc.hfe");
        CHECK(first->saveDiscAs(folder.filePath("gotek.hfe")));
        CHECK(!first->modified() && first->name() == "gotek.hfe" && title(first).startsWith("gotek.hfe - "));
        CHECK(child<QLabel>(&window, "lStatus")->text() == "gotek.hfe saved as an HFE file, for a Gotek or an HxC drive on a real CPC");
        const Bytes hfe = gui::fileBytes(folder.filePath("gotek.hfe"));
        CHECK(hfe.size() == 512 * (2 + 42 * 49) && std::equal(hfe.begin(), hfe.begin() + 8, "HXCPICFE"));
        CHECK(hfe == tuxdsk::hfeFile(first->disc()->image()));
        // Changed and saved again, it goes to the same file, of the same kind.
        CHECK(first->renameFile(*file(first, "K17.DAT"), "TEMP.DAT", 0));
        CHECK(first->modified() && first->saveDisc() && !first->modified());
        Bytes again = gui::fileBytes(folder.filePath("gotek.hfe"));
        CHECK(again.size() == hfe.size() && again != hfe && std::equal(again.begin(), again.begin() + 8, "HXCPICFE"));
        CHECK(first->renameFile(*file(first, "TEMP.DAT"), "K17.DAT", 0));
        CHECK(first->saveDisc() && gui::fileBytes(folder.filePath("gotek.hfe")) == hfe);
        // Read from it, by another list, it is the disc that was saved: the
        // same files, the same sectors.
        {
            Preferences none;
            ManagerOptions options;
            DiscPane reader(nullptr, &none, &options);
            CHECK(reader.openDisc(folder.filePath("gotek.hfe")));
            CHECK(reader.name() == "gotek.hfe" && !reader.modified() && reader.list()->topLevelItemCount() == 3);
            CHECK(title(&reader) == title(first));
            CHECK(!tuxdsk::firstDifference(reader.disc()->image(), first->disc()->image()).has_value());
            CHECK(file(&reader, "K17.DAT")
                  && reader.disc()->read(*file(&reader, "K17.DAT")) == first->disc()->read(*file(first, "K17.DAT")));
            // And what is not an .HFE file of the kind that is read is said.
            Bytes other = hfe;
            other[8] = 3;
            {
                QFile odd(folder.filePath("other.hfe"));
                CHECK(odd.open(QIODevice::WriteOnly));
                odd.write(reinterpret_cast<const char*>(other.data()), static_cast<qint64>(other.size()));
            }
            gui::g_told.clear();
            CHECK(!reader.openDisc(folder.filePath("other.hfe")) && reader.name() == "gotek.hfe");
            CHECK(gui::g_told.size() == 1 && gui::g_told.last().contains("of a kind that is not read"));
        }
        CHECK(first->saveDiscAs(folder.filePath("first.dsk")) && gui::fileBytes(folder.filePath("first.dsk")) == saved);

        // ---- The second disc ----
        child<QCheckBox>(&window, "ckTwo")->setChecked(true);
        CHECK(window.twoDiscs() && second->isVisible());
        CHECK(!child<QPushButton>(&window, "bCopyRight")->isEnabled() && !child<QPushButton>(&window, "bCompare")->isEnabled());
        CHECK(second->openDisc(folder.filePath("first.dsk")));
        CHECK(second->name() == "first.dsk" && !second->modified() && second->list()->topLevelItemCount() == 3);
        CHECK(child<QPushButton>(&window, "bCompare")->isEnabled());
        CHECK(window.compare() == "The DSKs are identical");
        second->newDisc(40);
        CHECK(window.compare() == "The DSKs are different: 42 tracks and 40");

        // ---- Files copied across, with their user and attributes ----
        first->setFocus();
        first->changeFile(*file(first, "K17.DAT"), tuxdsk::FileType::Binary, 0x4000, 0x4010, true, false);
        first->renameFile(*file(first, "K17.DAT"), "K17.DAT", 3);
        first->select({"K17.DAT", "CODE.BIN"});
        CHECK(child<QPushButton>(&window, "bCopyRight")->isEnabled() && !child<QPushButton>(&window, "bCopyLeft")->isEnabled());
        child<QPushButton>(&window, "bCopyRight")->click();
        CHECK_EQ(second->list()->topLevelItemCount(), 2);
        auto copy = file(second, "K17.DAT", 3);
        CHECK(copy && copy->readOnly && !copy->hidden && copy->loadAddress == 0x4000 && copy->entries.size() == 2);
        CHECK(copy && second->disc()->read(*copy) == first->disc()->read(*file(first, "K17.DAT", 3)));
        CHECK(second->modified());
        // Copied again, ManageDsk asks: no, then yes for all.
        gui::g_asked.clear();
        plan(Answer::No);
        plan(Answer::Yes);
        CHECK_EQ(window.copySelected(0), 1);
        CHECK(gui::g_asked.size() == 2 && second->list()->topLevelItemCount() == 2);
        // Two files chosen, one on each disc, are what is compared.
        first->select({"CODE.BIN"});
        second->select({"CODE.BIN"});
        CHECK(window.compare() == "Files CODE.BIN and CODE.BIN identical");
        second->select({"K17.DAT"});
        CHECK(window.compare() == "Files CODE.BIN and K17.DAT different");
        if (!prefix.isEmpty()) {
            window.resize(1480, 660);
            QTest::qWait(50);
            window.grab().save(prefix + "tuxdsk_two_discs.png");
        }
        // The other way.
        second->changeFile(*file(second, "CODE.BIN"), tuxdsk::FileType::Binary, 0x1234, 0, false, false);
        second->renameFile(*file(second, "CODE.BIN"), "OTHER.BIN", 0);
        second->select({"OTHER.BIN"});
        child<QPushButton>(&window, "bCopyLeft")->click();
        CHECK(file(first, "OTHER.BIN") && file(first, "OTHER.BIN")->loadAddress == 0x1234);
        // A disc with no room for it.
        second->newDisc(1);
        gui::g_told.clear();
        first->select({"K17.DAT"});
        CHECK_EQ(window.copySelected(0), 1);  // a disc of one track grows, as ManageDsk's does
        CHECK(second->disc()->image().tracks() > 1);

        // ---- Swapped ----
        const QString firstName = first->name();
        child<QPushButton>(&window, "bSwap")->click();
        CHECK(second->name() == firstName && first->name() == "New disc" && first->modified());
        CHECK(file(second, "OTHER.BIN") && file(first, "K17.DAT", 3));
        window.swapDiscs();
        CHECK(first->name() == firstName);

        // ---- A disc and files dropped on the list ----
        second->newDisc();
        drop(second, {gui::inputFile("code.bin"), gui::inputFile("UPPER.BIN")});  // forced binary
        CHECK(second->list()->topLevelItemCount() == 2 && file(second, "UPPER.BIN")->type == tuxdsk::FileType::Binary);
        // A disc dropped on one that was not saved is asked about first.
        gui::g_asked.clear();
        plan(Answer::No);
        drop(second, {folder.filePath("first.dsk")});
        CHECK(gui::g_asked.size() == 1 && gui::g_asked.last() == "Floppy New disc not saved. Confirm?");
        CHECK(second->name() == "New disc");
        plan(Answer::Yes);
        drop(second, {folder.filePath("first.dsk")});
        CHECK(second->name() == "first.dsk" && !second->modified() && second->list()->topLevelItemCount() == 3);

        // ---- What is not a disc, and what AMSDOS's files cannot go on ----
        gui::g_told.clear();
        CHECK(!second->openDisc(gui::inputFile("code.bin")));
        CHECK(!second->openDisc(folder.filePath("nothere.dsk")));
        CHECK(gui::g_told.size() == 2 && second->name() == "first.dsk");
        {
            QFile odd(folder.filePath("odd.dsk"));
            CHECK(odd.open(QIODevice::WriteOnly));
            const Bytes bytes = tuxdsk::DiscImage::formatted(40, 0x11).bytes();
            odd.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
        }
        gui::g_told.clear();
        CHECK(second->openDisc(folder.filePath("odd.dsk")));
        CHECK(gui::g_told.size() == 1 && gui::g_told.last().startsWith("File is not recognized as standard DSK"));
        CHECK(title(second) == "odd.dsk - not a DATA, VENDOR or PCW disc, 40 tracks");
        CHECK(!child<QPushButton>(second, "bAdd")->isEnabled() && child<QPushButton>(second, "bSectors")->isEnabled());
        // A disc whose sectors follow one another: ManageDsk offers to put
        // them in a CPC's order.
        {
            tuxdsk::DiscImage image = tuxdsk::DiscImage::formatted(40);
            for (int track = 0; track < 40; ++track)
                std::sort(image.track(track).sectors.begin(), image.track(track).sectors.end(),
                          [](const tuxdsk::Sector& a, const tuxdsk::Sector& b) { return a.id < b.id; });
            QFile plain(folder.filePath("plain.dsk"));
            CHECK(plain.open(QIODevice::WriteOnly));
            const Bytes bytes = image.bytes();
            plain.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
        }
        gui::g_asked.clear();
        plan(Answer::No);
        CHECK(second->openDisc(folder.filePath("plain.dsk")));
        CHECK(gui::g_asked.size() == 1 && gui::g_asked.last() == "Sectors not interlaced.\nForce interlacing?");
        CHECK(!second->modified() && !second->disc()->image().interleaved());
        plan(Answer::Yes);
        CHECK(second->openDisc(folder.filePath("plain.dsk")));
        CHECK(second->modified() && second->disc()->image().interleaved());

        // ---- More than 42 tracks ----
        second->newDisc();
        child<QRadioButton>(&window, "rbImportAscii")->click();
        {
            QFile big(folder.filePath("big.dat"));
            CHECK(big.open(QIODevice::WriteOnly));
            big.write(QByteArray(100 * 1024, 'x'));
            big.close();
            QFile::copy(folder.filePath("big.dat"), folder.filePath("big2.dat"));
        }
        gui::g_told.clear();
        second->addFiles({folder.filePath("big.dat"), folder.filePath("big2.dat"), gui::inputFile("noext")});
        // The second does not fit before block 189, and the third is not tried.
        CHECK(gui::g_told.size() == 1 && gui::g_told.last() == "No more space on the floppy!");
        CHECK(second->list()->topLevelItemCount() == 1);
        child<QCheckBox>(&window, "ckMoreTracks")->setChecked(true);
        second->addFiles({folder.filePath("big2.dat")});
        CHECK(second->list()->topLevelItemCount() == 2 && second->disc()->image().tracks() == 45);
        child<QCheckBox>(&window, "ckMoreTracks")->setChecked(false);

        // ---- Closing with a disc that was not saved ----
        CHECK(second->modified());
        first->saveDisc();
        plan(Answer::No);
        CHECK(!window.close() && window.isVisible());
        plan(Answer::Yes);
        CHECK(window.close() && !window.isVisible());
        CHECK(gui::g_answers.empty());
    }

    // ---- With a TuxAPE: its drives, and what the window keeps ----
    {
        Host host(folder.filePath("data"));
        QDir().mkpath(folder.filePath("data"));
        ManagerWindow window(&host, &host.frame);
        window.show();
        DiscPane* disc = window.pane(0);
        CHECK(!window.twoDiscs() && window.options().importMode == ManagerOptions::Ask);
        child<QRadioButton>(&window, "rbImportBinary")->click();
        child<QRadioButton>(&window, "rbExportPlain")->click();
        child<QCheckBox>(&window, "ckTwo")->setChecked(true);
        CHECK(disc->openDisc(folder.filePath("first.dsk")));
        CHECK(child<QPushButton>(disc, "bInsertA")->isEnabled() && !child<QPushButton>(disc, "bRun")->isEnabled());

        // A disc as its file has it goes in as it is.
        child<QPushButton>(disc, "bInsertB")->click();
        CHECK(host.inserted.size() == 1 && host.inserted.last() == "1:" + folder.filePath("first.dsk"));
        // One that was changed is saved first, if the user will have it.
        disc->addFiles({gui::inputFile("UPPER.BIN")});
        CHECK(disc->modified());
        plan(Answer::No);
        CHECK(!disc->insertInDrive(0) && host.inserted.size() == 1 && disc->modified());
        plan(Answer::Yes);
        disc->select({"UPPER.BIN"});
        CHECK(child<QPushButton>(disc, "bRun")->isEnabled());
        child<QPushButton>(disc, "bRun")->click();
        CHECK(!disc->modified() && host.inserted.size() == 2 && host.inserted.last() == "0:" + folder.filePath("first.dsk"));
        CHECK(host.typed.size() == 1 && host.typed.last() == "run\"UPPER.BIN~RETURN~");
        CHECK(tuxdsk::CpcDisc(*tuxdsk::DiscImage::parse(gui::fileBytes(folder.filePath("first.dsk")))).find("UPPER.BIN"));
        CHECK(window.close());
    }
    {
        // The next day: the choices are those of the day before.
        const QSettings kept(folder.filePath("data/TuxDSK.ini"), QSettings::IniFormat);
        CHECK(kept.value("Manager/Import").toInt() == 2 && !kept.value("Manager/ExportHeader").toBool());
        CHECK(kept.value("Folders/Discs").toString() == folder.path());
        Host host(folder.filePath("data"));
        ManagerWindow window(&host, &host.frame);
        CHECK(window.options().importMode == ManagerOptions::ForceBinary && !window.options().exportHeader);
        CHECK(window.twoDiscs() && child<QRadioButton>(&window, "rbExportPlain")->isChecked());
    }

    // ---- The files of a disc read from its file, put on this one ----
    {
        ManagerWindow window(nullptr);
        DiscPane* disc = window.pane(0);
        CHECK(!child<QPushButton>(disc, "bImportDisc")->isEnabled());
        gui::g_told.clear();
        CHECK_EQ(disc->importDisc(folder.filePath("first.dsk")), 0);
        CHECK(gui::g_told.size() == 1 && gui::g_told.last() == "No DSK loaded.");
        disc->newDisc();
        CHECK(child<QPushButton>(disc, "bImportDisc")->isEnabled());

        // The other disc's files are listed, all ticked: two are kept.
        gui::whenAsked([](QDialog* dialog) {
            CHECK(dialog->objectName() == "DiscFilesDialog" && dialog->windowTitle() == QString::fromUtf8("TuxDSK – Import from gotek.hfe"));
            auto* files = child<QTreeWidget>(dialog, "lvFiles");
            CHECK_EQ(files->topLevelItemCount(), 3);
            for (int row = 0; row < files->topLevelItemCount(); ++row)
                CHECK(files->topLevelItem(row)->checkState(0) == Qt::Checked);
            CHECK(child<QLabel>(dialog, "lChosen")->text().startsWith("3 files chosen, "));
            child<QPushButton>(dialog, "bNone")->click();
            CHECK(child<QLabel>(dialog, "lChosen")->text() == "0 files chosen, 0 Kb");
            CHECK(!child<QPushButton>(dialog, "bValidate")->isEnabled());
            for (int row = 0; row < files->topLevelItemCount(); ++row)
                if (files->topLevelItem(row)->text(0) != "WITHHDR.BIN")
                    files->topLevelItem(row)->setCheckState(0, Qt::Checked);
            CHECK(child<QLabel>(dialog, "lChosen")->text().startsWith("2 files chosen, "));
            CHECK(child<QPushButton>(dialog, "bValidate")->isEnabled());
            dialog->accept();
        });
        CHECK_EQ(disc->importDisc(folder.filePath("gotek.hfe")), 2);
        CHECK(disc->modified() && disc->list()->topLevelItemCount() == 2 && !file(disc, "WITHHDR.BIN"));
        CHECK(child<QLabel>(&window, "lStatus")->text() == "2 files imported from gotek.hfe.");
        // As they are on the other disc, header and all.
        const tuxdsk::CpcDisc source(*tuxdsk::parseDisc(gui::fileBytes(folder.filePath("gotek.hfe"))));
        for (const char* name : {"CODE.BIN", "K17.DAT"}) {
            const auto there = source.find(name);
            const auto here = file(disc, name);
            CHECK(there && here && here->type == there->type && here->loadAddress == there->loadAddress);
            CHECK(there && here && disc->disc()->read(*here) == source.read(*there));
        }

        // Again, all of them: the files the disc has are asked about, one
        // kept and one replaced.
        gui::g_asked.clear();
        plan(Answer::No);
        plan(Answer::Yes);
        gui::whenAsked([](QDialog* dialog) { dialog->accept(); });
        CHECK_EQ(disc->importDisc(folder.filePath("gotek.hfe")), 2);
        CHECK(gui::g_asked.size() == 2 && gui::g_asked.first().endsWith(" already exists. Replace?"));
        CHECK(disc->list()->topLevelItemCount() == 3 && file(disc, "WITHHDR.BIN"));
        // From the .DSK file, whose K17.DAT is now a file of user 3: it goes
        // beside the one of user 0, and "Yes for all" replaces the others.
        gui::g_asked.clear();
        plan(Answer::YesToAll);
        gui::whenAsked([](QDialog* dialog) { dialog->accept(); });
        CHECK_EQ(disc->importDisc(folder.filePath("first.dsk")), 5);
        CHECK_EQ(gui::g_asked.size(), 1);
        CHECK(disc->list()->topLevelItemCount() == 6 && file(disc, "K17.DAT") && file(disc, "K17.DAT", 3));
        CHECK(file(disc, "UPPER.BIN") && file(disc, "OTHER.BIN"));
        // Cancelled, nothing changes.
        gui::whenAsked([](QDialog* dialog) { dialog->reject(); });
        CHECK_EQ(disc->importDisc(folder.filePath("gotek.hfe")), 0);

        // What has no file to give is said.
        {
            QFile empty(folder.filePath("empty.dsk"));
            CHECK(empty.open(QIODevice::WriteOnly));
            const Bytes bytes = tuxdsk::CpcDisc().image().bytes();
            empty.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
        }
        gui::g_told.clear();
        CHECK_EQ(disc->importDisc(gui::inputFile("code.bin")), 0);
        CHECK_EQ(disc->importDisc(folder.filePath("odd.dsk")), 0);
        CHECK_EQ(disc->importDisc(folder.filePath("empty.dsk")), 0);
        CHECK(gui::g_told.size() == 3
              && gui::g_told[1] == "odd.dsk is not recognized as standard DSK (DATA or VENDOR): it has no files to import.");
        CHECK(gui::g_told.size() == 3 && gui::g_told[2] == "There is no file on empty.dsk.");
        CHECK(disc->list()->topLevelItemCount() == 6 && gui::g_answers.empty());
    }

    // ---- Sprites from TuxAPE's Find Graphics window ----
    {
        ManagerWindow window(nullptr);
        DiscPane* disc = window.pane(0);
        tuxape::PluginTiles tiles;
        // None selected: it is said, and nothing is made.
        gui::g_told.clear();
        CHECK(!window.takeTiles(tiles) && gui::g_told.size() == 1 && !disc->hasDisc());
        // Two of a Plus's sprites, sixteen bytes by sixteen lines, from &4000.
        tiles.count = 2;
        tiles.width = tiles.height = 16;
        tiles.address = 0x4000;
        tiles.mode = 3;
        for (int at = 0; at < 512; ++at)
            tiles.bytes += static_cast<char>(at & 15);
        const Bytes sprites(tiles.bytes.begin(), tiles.bytes.end());

        // The name is asked for, SPRITE.SPR to begin with. Cancelled, no
        // disc is made for nothing.
        gui::whenAsked([](QDialog* dialog) {
            CHECK(dialog->objectName() == "TilesDialog" && dialog->windowTitle() == QString::fromUtf8("TuxDSK – Copy to the Disc"));
            CHECK(child<QLabel>(dialog, "lTiles")->text() == "2 sprites of 16 by 16, from &4000: 512 bytes");
            CHECK(child<QLineEdit>(dialog, "edName")->text() == "SPRITE.SPR");
            CHECK(child<QCheckBox>(dialog, "ckHeader")->isChecked());
            CHECK(child<QCheckBox>(dialog, "ckHeader")->text() == "With an AMSDOS &header: a binary that loads at &&4000");
            dialog->reject();
        });
        CHECK(!window.takeTiles(tiles) && !disc->hasDisc());

        // Validated with no disc: one is made, and the file is on it, a
        // binary that loads where the sprites were.
        gui::whenAsked([](QDialog* dialog) { dialog->accept(); });
        CHECK(window.takeTiles(tiles));
        CHECK(disc->hasDisc() && disc->modified() && disc->name() == "New disc");
        CHECK(disc->disc()->image().tracks() == 42 && disc->list()->topLevelItemCount() == 1);
        auto sprite = file(disc, "SPRITE.SPR");
        CHECK(sprite && sprite->type == tuxdsk::FileType::Binary && sprite->loadAddress == 0x4000);
        CHECK(sprite && sprite->entryAddress == 0 && sprite->length == 512 && sprite->records == 5);
        if (sprite) {
            const Bytes content = disc->disc()->read(*sprite);
            CHECK(Bytes(content.begin() + 128, content.end()) == sprites);
            const auto header = tuxdsk::amsdosHeader(content);
            CHECK(header && header->name == "SPRITE.SPR");
        }
        // It is the file chosen in the list, and the line at the bottom says
        // the disc is still to be saved.
        CHECK(disc->selectedFiles().size() == 1 && disc->selectedFiles().front().name == "SPRITE.SPR");
        CHECK(child<QLabel>(&window, "lStatus")->text() == "A disc was made for SPRITE.SPR: Save DSK writes it to a file.");

        // The next: a name the disc does not have is offered; another is
        // typed, and the bytes go alone, without a header.
        gui::whenAsked([](QDialog* dialog) {
            CHECK(child<QLineEdit>(dialog, "edName")->text() == "SPRITE1.SPR");
            child<QLineEdit>(dialog, "edName")->setText("hero.spr");
            child<QCheckBox>(dialog, "ckHeader")->setChecked(false);
            dialog->accept();
        });
        CHECK(window.takeTiles(tiles));
        auto hero = file(disc, "HERO.SPR");
        CHECK(hero && !hero->hasHeader() && hero->records == 4 && disc->disc()->read(*hero) == sprites);
        CHECK(disc->list()->topLevelItemCount() == 2 && disc->disc()->image().tracks() == 42);
        CHECK(child<QLabel>(&window, "lStatus")->text() == "HERO.SPR is on the disc: Save DSK writes it to its file.");

        // A name the disc has: ManageDsk's question. No asks for another
        // name; Yes puts the new file in place of the old.
        tiles.count = 1;
        tiles.bytes.truncate(256);
        gui::g_asked.clear();
        plan(Answer::No, false, [] {
            gui::whenAsked([](QDialog* dialog) {
                CHECK(child<QLineEdit>(dialog, "edName")->text() == "HERO.SPR");
                dialog->accept();
            });
        });
        plan(Answer::Yes);
        gui::whenAsked([](QDialog* dialog) {
            // The choice of the last time: no header.
            CHECK(!child<QCheckBox>(dialog, "ckHeader")->isChecked());
            CHECK(child<QLabel>(dialog, "lTiles")->text() == "1 sprite of 16 by 16, from &4000: 256 bytes");
            child<QLineEdit>(dialog, "edName")->setText("HERO.SPR");
            dialog->accept();
        });
        CHECK(window.takeTiles(tiles));
        CHECK(gui::g_asked.size() == 2 && gui::g_asked.last() == "File HERO.SPR already exists. Replace?");
        hero = file(disc, "HERO.SPR");
        CHECK(hero && hero->records == 2 && disc->list()->topLevelItemCount() == 2);
        // Cancel leaves the disc as it is.
        plan(Answer::Cancel);
        gui::whenAsked([](QDialog* dialog) {
            child<QLineEdit>(dialog, "edName")->setText("SPRITE.SPR");
            dialog->accept();
        });
        CHECK(!window.takeTiles(tiles) && file(disc, "SPRITE.SPR")->length == 512);

        // Tiles of the screen's modes go as a window of OCP Art Studio,
        // SCREEN.WIN: the tiles side by side, line after line, then the
        // window's width in bits, its height in lines, and 3. With a
        // header, it loads at &4000, as OCP Art Studio's windows do.
        tiles.mode = 1;
        tiles.count = 2;
        tiles.width = 2;
        tiles.height = 8;
        tiles.address = 0xC000;
        tiles.bytes.clear();
        for (int tile = 0; tile < 2; ++tile)
            for (int line = 0; line < 8; ++line)
                for (int column = 0; column < 2; ++column)
                    tiles.bytes += static_cast<char>(tile << 7 | line << 4 | column);
        gui::whenAsked([](QDialog* dialog) {
            CHECK(child<QLabel>(dialog, "lTiles")->text()
                  == "2 tiles of 2 bytes by 8 lines, from &C000, side by side: an OCP Art Studio window of 4 bytes by 8 lines (.WIN)");
            CHECK(child<QLineEdit>(dialog, "edName")->text() == "SCREEN.WIN");
            child<QCheckBox>(dialog, "ckHeader")->setChecked(true);
            CHECK(child<QCheckBox>(dialog, "ckHeader")->text() == "With an AMSDOS &header: a binary that loads at &&4000");
            dialog->accept();
        });
        CHECK(window.takeTiles(tiles));
        auto screen = file(disc, "SCREEN.WIN");
        CHECK(screen && screen->loadAddress == 0x4000 && screen->length == 4 * 8 + 5);
        if (screen) {
            const Bytes content = disc->disc()->read(*screen);
            Bytes wanted;
            for (int line = 0; line < 8; ++line)
                for (int tile = 0; tile < 2; ++tile)
                    for (int column = 0; column < 2; ++column)
                        wanted.push_back(static_cast<uint8_t>(tile << 7 | line << 4 | column));
            for (const uint8_t byte : {0, 32, 0, 8, 3})
                wanted.push_back(byte);
            // The disc gives whole records: the header says where the file ends.
            CHECK(content.size() >= 128 + wanted.size()
                  && Bytes(content.begin() + 128, content.begin() + 128 + static_cast<ptrdiff_t>(wanted.size())) == wanted);
            const auto header = tuxdsk::amsdosHeader(content);
            CHECK(header && header->length == wanted.size());
        }
        // "Current" in Find Graphics is the mode it found: a window too.
        tiles.count = 1;
        tiles.mode = 0;
        tiles.bytes.truncate(16);
        gui::whenAsked([](QDialog* dialog) {
            CHECK(child<QLineEdit>(dialog, "edName")->text() == "SCREEN1.WIN");
            dialog->accept();
        });
        CHECK(window.takeTiles(tiles) && file(disc, "SCREEN1.WIN") && file(disc, "SCREEN1.WIN")->length == 16 + 5);
        // Too high for a window: said, and nothing is made.
        tiles.height = 300;
        gui::g_told.clear();
        CHECK(!window.takeTiles(tiles) && gui::g_told.size() == 1 && !file(disc, "SCREEN2.WIN"));
        tiles.mode = 3;

        // A disc with no room: said, and the file is not there.
        disc->addBytes("FILL.DAT", Bytes(180 * 1024, 0));
        tiles.bytes = QByteArray(8 * 1024, 'x');
        tiles.count = 32;
        tiles.width = tiles.height = 16;
        gui::g_told.clear();
        gui::whenAsked([](QDialog* dialog) { dialog->accept(); });
        CHECK(!window.takeTiles(tiles));
        CHECK(gui::g_told.size() == 1 && gui::g_told.last() == "No more space on the floppy!" && !file(disc, "SPRITE1.SPR"));
        CHECK(gui::g_answers.empty());
    }

    return checkSummary("gui_manager");
}
