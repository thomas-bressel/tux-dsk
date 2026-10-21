// The windows that show: a file in the viewer's five modes, the mode chosen
// for it, a screen with its palette and its inks changed, a window saved;
// the sectors of a disc; and the windows that ask, by themselves. Runs
// without a display (QT_QPA_PLATFORM=offscreen).
//
//   gui_viewers [prefix]   also saves pictures of the viewer and of the
//                          sectors, as <prefix>tuxdsk_listing.png,
//                          <prefix>tuxdsk_screen.png (and _dark) and
//                          <prefix>tuxdsk_sectors.png

#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QToolButton>

#include "controls.h"
#include "dialogs.h"
#include "disc/amsdos.h"
#include "discpane.h"
#include "gui_looks.h"
#include "gui_test.h"
#include "preferences.h"
#include "screenwidget.h"
#include "sectordialog.h"
#include "view/basic.h"
#include "view/screen.h"
#include "viewerdialog.h"

using golden::Bytes;

namespace {

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

// 10 PRINT "HELLO":GOTO 10, as the CPC keeps it.
Bytes program()
{
    const Bytes line = {0xBF, ' ', '"', 'H', 'E', 'L', 'L', 'O', '"', 0x01, 0xA0, ' ', 0x1E, 10, 0, 0};
    Bytes bytes = {static_cast<uint8_t>(line.size() + 4), 0, 10, 0};
    bytes.insert(bytes.end(), line.begin(), line.end());
    bytes.insert(bytes.end(), {0, 0});
    return bytes;
}

Bytes withHeader(const char* name, uint8_t type, int load, const Bytes& data)
{
    Bytes bytes = tuxdsk::makeAmsdosHeader(name, type, static_cast<uint16_t>(load), 0, data.size());
    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}

// A screen: its first line all of pen 1 in mode 1, the rest pen 0, and the
// last byte of its last line pen 3.
Bytes screen()
{
    Bytes memory(0x4000, 0);
    std::fill(memory.begin(), memory.begin() + 80, 0xF0);
    memory[0x3800 + 24 * 80 + 79] = 0xFF;
    return memory;
}

// An OCP palette: mode 0, ink 0 bright red, ink 1 bright green, ink 5 bright blue.
Bytes palette()
{
    Bytes bytes(3 + 17 * 12, 0x54);
    bytes[0] = 0;
    std::fill_n(bytes.begin() + 3, 12, 0x4C);
    std::fill_n(bytes.begin() + 3 + 12, 12, 0x52);
    std::fill_n(bytes.begin() + 3 + 5 * 12, 12, 0x55);
    return bytes;
}

QRgb rgb(int colour)
{
    const tuxdsk::CpcColour c = tuxdsk::cpcColour(colour);
    return qRgb(c.red, c.green, c.blue);
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString prefix = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    gui::answerBoxes();
    QTemporaryDir scratch;
    const QDir folder(scratch.path());
    Preferences preferences;
    ManagerOptions options;
    options.importMode = ManagerOptions::ForceAscii;  // files go on the disc as they are given

    // A disc with a file of each kind the viewer knows.
    DiscPane pane(nullptr, &preferences, &options);
    pane.newDisc();
    const auto put = [&](const char* name, const Bytes& bytes) {
        CHECK(pane.putFile({name, 0, false, false, false, 0, {}, tuxdsk::FileType::Ascii, 0, 0, 0}, bytes, false)
              == tuxdsk::AddResult::Done);
    };
    put("PROG.BAS", withHeader("PROG.BAS", tuxdsk::kAmsdosBasic, 0x170, program()));
    put("SECRET.BAS", withHeader("SECRET.BAS", tuxdsk::kAmsdosProtectedBasic, 0x170, tuxdsk::unprotectedBasic(program())));
    put("README.TXT", Bytes{'H', 'i', ' ', 't', 'h', 'e', 'r', 'e', '\r', '\n', 'B', 'y', 'e', '\r', '\n', 0x1A, 0x1A});
    put("CODE.BIN", withHeader("CODE.BIN", tuxdsk::kAmsdosBinary, 0x8000, Bytes{0x21, 0x00, 0xC0, 0x18, 0xFE, 0xC9}));
    put("RAW.DAT", Bytes(200, 0x81));
    put("SOURCE.DAM", withHeader("SOURCE.DAM", tuxdsk::kAmsdosBinary, 0, Bytes{'G', 'O', 0x80, 'A', ',', '1', 0x0D, 0}));
    put("PIC.SCR", withHeader("PIC.SCR", tuxdsk::kAmsdosBinary, 0xC000, screen()));
    put("PIC.PAL", withHeader("PIC.PAL", tuxdsk::kAmsdosBinary, 0x8809, palette()));
    put("LONE.SCR", withHeader("LONE.SCR", tuxdsk::kAmsdosBinary, 0xC000, screen()));
    put("SPRITE.WIN", withHeader("SPRITE.WIN", tuxdsk::kAmsdosBinary, 0x4000,
                                 tuxdsk::windowFile(Bytes{0xF0, 0x0F, 0xFF, 0x00, 0x0F, 0xF0}, 2, 3)));
    const auto view = [&pane](const char* name) {
        ViewerDialog* viewer = pane.viewFile(*pane.disc()->find(name));
        CHECK(viewer && viewer->isVisible());
        return viewer;
    };

    // ---- A BASIC program is listed ----
    {
        ViewerDialog* viewer = view("PROG.BAS");
        CHECK(viewer->windowTitle() == QString::fromUtf8("TuxDSK – Show File: PROG.BAS"));
        CHECK(viewer->mode() == ViewerDialog::Listing);
        CHECK(viewer->text() == "10 PRINT \"HELLO\":GOTO 10\n");
        CHECK(child<QPushButton>(viewer, "bSaveText")->isEnabled() && !child<QPushButton>(viewer, "bSavePicture")->isEnabled());
        // Its bytes, without its header.
        viewer->setMode(ViewerDialog::Hexadecimal);
        CHECK(viewer->text().startsWith("#0000: 14 00 0A 00 BF 20 22 48 45 4C 4C 4F 22 01 A0 20 | ....."));
        CHECK(viewer->text().count('\n') == 2);
        // The text shown is what "Save as Text" writes.
        viewer->setMode(ViewerDialog::Listing);
        CHECK(viewer->saveText(folder.filePath("prog.txt")));
        CHECK(gui::fileBytes(folder.filePath("prog.txt")) == Bytes({'1', '0', ' ', 'P', 'R', 'I', 'N', 'T', ' ', '"', 'H', 'E',
                                                                   'L', 'L', 'O', '"', ':', 'G', 'O', 'T', 'O', ' ', '1', '0',
                                                                   '\n'}));
        if (!prefix.isEmpty()) {
            QTest::qWait(50);
            viewer->grab().save(prefix + "tuxdsk_listing.png");
        }
        viewer->close();
        // One that was protected is listed all the same.
        viewer = view("SECRET.BAS");
        CHECK(viewer->mode() == ViewerDialog::Listing && viewer->text() == "10 PRINT \"HELLO\":GOTO 10\n");
        viewer->close();
    }

    // ---- A text is listed too, to its end-of-file mark ----
    {
        ViewerDialog* viewer = view("README.TXT");
        CHECK(viewer->mode() == ViewerDialog::Listing && viewer->text() == "Hi there\nBye\n");
        CHECK(!preferences.wrapListings());
        child<QCheckBox>(viewer, "ckWrap")->setChecked(true);
        CHECK(preferences.wrapListings() && viewer->text() == "Hi there\nBye\n");
        child<QCheckBox>(viewer, "ckWrap")->setChecked(false);
        viewer->close();
    }

    // ---- Anything else is bytes, and can be read as instructions ----
    {
        ViewerDialog* viewer = view("RAW.DAT");
        CHECK(viewer->mode() == ViewerDialog::Hexadecimal);
        // All its records: a file without a header does not say where it ends.
        CHECK(viewer->text().count('\n') == 16 && viewer->text().startsWith("#0000: 81 81"));
        viewer->close();

        viewer = view("CODE.BIN");
        CHECK(viewer->mode() == ViewerDialog::Hexadecimal);
        CHECK(viewer->text() == "#0000: 21 00 C0 18 FE C9                               | !.....\n");
        // From the address it loads at, which can be changed.
        viewer->setMode(ViewerDialog::Disassembly);
        CHECK_EQ(child<QSpinBox>(viewer, "sbStart")->value(), 0x8000);
        CHECK(viewer->text() == "8000 21 00 C0       LD HL,C000\n8003 18 FE          JR 8003\n8005 C9             RET\n");
        child<QSpinBox>(viewer, "sbStart")->setValue(0x100);
        CHECK(viewer->text().startsWith("0100 21 00 C0       LD HL,C000\n0103 18 FE          JR 0103\n"));
        viewer->close();

        viewer = view("SOURCE.DAM");
        CHECK(viewer->mode() == ViewerDialog::Dams && viewer->text() == "GO\tLD\tA,1\n");
        viewer->close();
    }

    // ---- A screen, with the palette that goes with it ----
    {
        ViewerDialog* viewer = view("LONE.SCR");
        CHECK(viewer->mode() == ViewerDialog::Screen && viewer->text().isEmpty());
        CHECK(!child<QPushButton>(viewer, "bSaveText")->isEnabled() && child<QPushButton>(viewer, "bSavePicture")->isEnabled());
        CHECK(!child<QPushButton>(viewer, "bSaveWindow")->isEnabled());
        // Without a palette: mode 1 and the inks a CPC starts with.
        CHECK_EQ(child<QComboBox>(viewer, "cbScreenMode")->currentIndex(), 1);
        QImage picture = viewer->picture();
        CHECK(picture.width() == 640 && picture.height() == 200);
        CHECK(picture.pixel(0, 0) == rgb(24) && picture.pixel(639, 0) == rgb(24));  // pen 1: bright yellow
        CHECK(picture.pixel(0, 1) == rgb(1) && picture.pixel(639, 199) == rgb(6));  // pen 0 blue, pen 3 bright red
        // In mode 2 the same bytes are other pixels.
        child<QComboBox>(viewer, "cbScreenMode")->setCurrentIndex(2);
        picture = viewer->picture();
        CHECK(picture.pixel(0, 0) == rgb(24) && picture.pixel(4, 0) == rgb(1) && picture.pixel(8, 0) == rgb(24));
        // An ink changed in ManageDsk's window of the 27 colours.
        auto* screenWidget = child<ScreenWidget>(viewer, "swScreen");
        CHECK(child<QToolButton>(viewer, "bInk1")->isVisible() && !child<QToolButton>(viewer, "bInk2")->isVisible());
        gui::whenAsked([](QDialog* dialog) {
            CHECK(dialog->objectName() == "ColourDialog" && dialog->findChildren<QToolButton*>().size() == 27);
            CHECK(child<QToolButton>(dialog, "bColour24")->isChecked());
            child<QToolButton>(dialog, "bColour18")->click();
        });
        child<QToolButton>(viewer, "bInk1")->click();
        CHECK_EQ(screenWidget->inks()[1], 18);
        CHECK(viewer->picture().pixel(0, 0) == rgb(18));
        // The picture saved is as the monitor shows it: each line twice.
        CHECK(viewer->savePicture(folder.filePath("lone.png")));
        const QImage saved(folder.filePath("lone.png"));
        CHECK(saved.width() == 640 && saved.height() == 400 && saved.pixel(0, 1) == rgb(18) && saved.pixel(0, 2) == rgb(1));
        viewer->close();

        // With its .PAL: the palette's mode and inks.
        viewer = view("PIC.SCR");
        CHECK_EQ(child<QComboBox>(viewer, "cbScreenMode")->currentIndex(), 0);
        picture = viewer->picture();
        // &F0 in mode 0 is two pixels of pen 5: bright blue here.
        CHECK(picture.pixel(0, 0) == rgb(2) && picture.pixel(0, 1) == rgb(6));
        CHECK(child<QToolButton>(viewer, "bInk15")->isVisible());
        // Fewer columns and lines, as ManageDsk lets one ask.
        child<QSpinBox>(viewer, "sbColumns")->setValue(40);
        child<QSpinBox>(viewer, "sbLines")->setValue(100);
        CHECK(viewer->picture().width() == 320 && viewer->picture().height() == 100);
        CHECK_EQ(child<QSpinBox>(viewer, "sbColumns")->maximum(), 80);
        if (!prefix.isEmpty()) {
            child<QSpinBox>(viewer, "sbColumns")->setValue(80);
            child<QSpinBox>(viewer, "sbLines")->setValue(200);
            CHECK(saveInBothLooks(viewer, prefix + "tuxdsk_screen"));
        }
        viewer->close();
    }

    // ---- A window: flat, as wide and as high as it says ----
    {
        ViewerDialog* viewer = view("SPRITE.WIN");
        CHECK(viewer->mode() == ViewerDialog::Screen);
        CHECK(child<QCheckBox>(viewer, "ckFlat")->isChecked() && child<QPushButton>(viewer, "bSaveWindow")->isEnabled());
        CHECK(child<QSpinBox>(viewer, "sbColumns")->value() == 2 && child<QSpinBox>(viewer, "sbLines")->value() == 3);
        CHECK_EQ(child<QSpinBox>(viewer, "sbColumns")->maximum(), 128);
        QImage picture = viewer->picture();
        CHECK(picture.width() == 16 && picture.height() == 3);
        // Mode 1: &F0 is four pixels of pen 1, &0F of pen 2, &FF of pen 3.
        CHECK(picture.pixel(0, 0) == rgb(24) && picture.pixel(8, 0) == rgb(20) && picture.pixel(0, 1) == rgb(6));
        CHECK(picture.pixel(8, 1) == rgb(1) && picture.pixel(0, 2) == rgb(20));
        // From further on in the bytes, and narrower.
        child<QSpinBox>(viewer, "sbOffset")->setValue(2);
        child<QSpinBox>(viewer, "sbColumns")->setValue(1);
        picture = viewer->picture();
        CHECK(picture.width() == 8 && picture.pixel(0, 0) == rgb(6) && picture.pixel(0, 1) == rgb(1));
        // What is shown, saved as a window again.
        child<QSpinBox>(viewer, "sbLines")->setValue(4);
        CHECK(viewer->saveWindow(folder.filePath("part.win")));
        const Bytes written = gui::fileBytes(folder.filePath("part.win"));
        const auto header = tuxdsk::amsdosHeader(written);
        CHECK(header && header->name == "PART.WIN" && header->loadAddress == 0x4000 && header->length == 9);
        CHECK(Bytes(written.begin() + 128, written.end()) == Bytes({0xFF, 0x00, 0x0F, 0xF0, 0, 8, 0, 4, 3}));
        viewer->close();
    }

    // ---- The sectors ----
    {
        SectorDialog* sectors = pane.showSectors();
        CHECK(sectors && sectors->isVisible() && sectors->windowTitle() == QString::fromUtf8("TuxDSK – Showing Sectors: New disc"));
        CHECK(sectors->track() == 0 && sectors->side() == 0 && sectors->place() == 0);
        CHECK_EQ(child<QComboBox>(sectors, "cbSector")->count(), 9);
        CHECK(child<QComboBox>(sectors, "cbSector")->itemText(1) == "&C6");
        CHECK(sectors->details() == "Sector 1 of 9: number &C1, 512 bytes");
        // The directory's first sector: the first file's entry.
        CHECK(sectors->text().startsWith("#0000: 00 50 52 4F 47 20 20 20 20 42 41 53 00 00 00 02 | .PROG....BAS...."));
        CHECK(sectors->text().count('\n') == 32);
        child<QCheckBox>(sectors, "ckDetailed")->setChecked(true);
        CHECK(sectors->details().endsWith("Track 0, side 0, size 2, ST1 &00, ST2 &00 - the track's gap &4E, its filler &E5"));
        // The next sector under the head, and on to the next track.
        child<QPushButton>(sectors, "bNext")->click();
        CHECK(sectors->place() == 1 && sectors->details().startsWith("Sector 2 of 9: number &C6"));
        child<QPushButton>(sectors, "bPrevious")->click();
        child<QPushButton>(sectors, "bPrevious")->click();
        CHECK(sectors->track() == 0 && sectors->place() == 0);
        sectors->showSector(0, 0, 8);
        child<QPushButton>(sectors, "bNext")->click();
        CHECK(sectors->track() == 1 && sectors->place() == 0);
        child<QPushButton>(sectors, "bPrevious")->click();
        CHECK(sectors->track() == 0 && sectors->place() == 8);
        child<QSpinBox>(sectors, "sbTrack")->setValue(41);
        CHECK(sectors->text().startsWith("#0000: E5 E5") && child<QSpinBox>(sectors, "sbTrack")->maximum() == 41);
        sectors->showSector(41, 0, 8);
        child<QPushButton>(sectors, "bNext")->click();
        CHECK(sectors->track() == 41 && sectors->place() == 8);
        child<QPushButton>(sectors, "bDirectory")->click();
        CHECK(sectors->track() == 0 && sectors->place() == 0 && !child<QSpinBox>(sectors, "sbSide")->isEnabled());
        if (!prefix.isEmpty()) {
            QTest::qWait(50);
            sectors->grab().save(prefix + "tuxdsk_sectors.png");
        }
        sectors->close();

        // A VENDOR disc's directory is on its third track; a track that
        // lost its sectors says so.
        tuxdsk::CpcDisc vendor = tuxdsk::CpcDisc::formatted(40, tuxdsk::DiscFormat::Vendor);
        vendor.image().track(5).sectors.clear();
        SectorDialog dialog(vendor, "vendor.dsk");
        child<QPushButton>(&dialog, "bDirectory")->click();
        CHECK(dialog.track() == 2 && dialog.place() == 0 && dialog.details().contains("number &41"));
        dialog.showSector(5, 0, 0);
        CHECK(dialog.details() == "This track has no sector." && dialog.text().isEmpty());
        dialog.showSector(4, 0, 8);
        child<QPushButton>(&dialog, "bNext")->click();
        CHECK(dialog.track() == 6 && dialog.place() == 0);
    }

    // ---- The windows that ask, by themselves ----
    {
        const Bytes headed = golden::input("withhdr.bin");
        ImportDialog withHeaderDialog("withhdr.bin", "WITHHDR.BIN", headed);
        tuxdsk::ImportOptions chosen = withHeaderDialog.options();
        CHECK(chosen.kind == tuxdsk::ImportKind::Binary && chosen.setAddresses && chosen.loadAddress == 0x8000);
        CHECK(chosen.entryAddress == 0x8010);
        child<QRadioButton>(&withHeaderDialog, "rbAscii")->click();
        CHECK(withHeaderDialog.options().kind == tuxdsk::ImportKind::Ascii);

        tuxdsk::DiscFile file = *pane.disc()->find("CODE.BIN");
        FileInfoDialog info(file);
        CHECK(info.type() == tuxdsk::FileType::Binary && info.loadAddress() == 0x8000 && info.entryAddress() == 0);
        CHECK_EQ(child<QComboBox>(&info, "cbType")->count(), 5);
        child<QComboBox>(&info, "cbType")->setCurrentIndex(4);
        CHECK(info.type() == tuxdsk::FileType::Ascii && !child<QSpinBox>(&info, "sbStart")->isEnabled());
        // A type AMSDOS does not know is one more choice, for it to stay.
        file.type = tuxdsk::FileType::Unknown;
        FileInfoDialog odd(file);
        CHECK(child<QComboBox>(&odd, "cbType")->count() == 6 && odd.type() == tuxdsk::FileType::Unknown);

        RenameDialog rename(file);
        CHECK(rename.name() == "CODE.BIN" && rename.user() == 0);
        child<QLineEdit>(&rename, "edName")->setText("new.b");
        CHECK(rename.name() == "NEW.B");
        // What a directory cannot hold cannot be typed.
        child<QLineEdit>(&rename, "edName")->clear();
        QTest::keyClicks(child<QLineEdit>(&rename, "edName"), "a b*c.toolong");
        CHECK(rename.name() == "ABC.TOO");

        NewDiscDialog more(true);
        CHECK(more.tracks() == 42 && more.format() == tuxdsk::DiscFormat::Data);
        CHECK_EQ(child<QSpinBox>(&more, "sbTracks")->maximum(), 80);
        child<QComboBox>(&more, "cbFormat")->setCurrentIndex(1);
        CHECK(more.format() == tuxdsk::DiscFormat::Vendor);

        AboutDialog about;
        const QString text = child<QTextBrowser>(&about, "tbAbout")->toPlainText();
        CHECK(text.contains("ManageDsk 0.20g") && text.contains("Ludovic Deplanque") && text.contains("MIT License"));

        // An address typed as the CPC's users write them.
        AddressBox box;
        box.setValue(0xC000);
        CHECK(box.text() == "&C000");
        box.clear();
        QTest::keyClicks(&box, "#bb5a");
        CHECK_EQ(box.value(), 0xBB5A);
        box.clear();
        QTest::keyClicks(&box, "49152");
        CHECK_EQ(box.value(), 0xC000);
        box.clear();
        QTest::keyClicks(&box, "0x170");
        CHECK_EQ(box.value(), 0x170);
    }

    CHECK(gui::g_told.isEmpty() && gui::g_answers.empty());
    return checkSummary("gui_viewers");
}
