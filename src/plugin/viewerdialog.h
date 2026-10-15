#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QDialog>
#include <QImage>

#include "disc/disc.h"
#include "view/screen.h"

class AddressBox;
class Preferences;
class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class ScreenWidget;

// ManageDsk's file viewer: a file of a disc shown in one of its five modes.
//
//   Hexadecimal    its bytes
//   Listing        a BASIC program, protected or not, or a text
//   Dams           a source of the Dams assembler
//   Disassembly    Z80 instructions, from the address it loads at
//   Video memory   a screen, a window or a sprite, in a mode and its inks
//
// The mode is chosen for the file when it is shown, as ManageDsk chooses
// it: a BASIC program is listed, a .SCR and a .WIN are pictures, a .DAM is
// a source, a text is listed and anything else is bytes.
class ViewerDialog : public QDialog {
    Q_OBJECT

public:
    enum Mode { Hexadecimal, Listing, Dams, Disassembly, Screen };

    // `records` is all of the file as the disc has it. `palette` is the
    // look of the .PAL that goes with it, if the disc has one.
    ViewerDialog(const tuxdsk::DiscFile& file, std::vector<uint8_t> records, std::optional<tuxdsk::ScreenLook> palette,
                 Preferences* preferences, QWidget* parent = nullptr);

    Mode mode() const;
    void setMode(Mode mode);
    // What the window shows: the text of the four modes that are text, and
    // the picture of the fifth.
    QString text() const;
    QImage picture() const;

    // What the buttons write: the text shown, the picture as a PNG, and
    // the flat picture's bytes as an OCP window (.WIN). False if the file
    // cannot be written.
    bool saveText(const QString& path) const;
    bool savePicture(const QString& path) const;
    bool saveWindow(const QString& path) const;

private:
    tuxdsk::DiscFile file_;
    std::vector<uint8_t> content_;  // the file without its header, as long as its header says
    std::vector<uint8_t> memory_;   // the same as a screen: unpacked
    Preferences* preferences_;
    bool built_ = false;  // the controls are all there: what they say can be shown

    QComboBox* mode_ = nullptr;
    QStackedWidget* options_ = nullptr;
    QStackedWidget* views_ = nullptr;
    QPlainTextEdit* text_ = nullptr;
    ScreenWidget* screen_ = nullptr;
    QCheckBox* wrap_ = nullptr;
    QCheckBox* french_ = nullptr;
    AddressBox* start_ = nullptr;
    QComboBox* screenMode_ = nullptr;
    QCheckBox* flat_ = nullptr;
    QSpinBox* columns_ = nullptr;
    QSpinBox* lines_ = nullptr;
    AddressBox* offset_ = nullptr;
    QSpinBox* zoom_ = nullptr;
    QPushButton* saveText_ = nullptr;
    QPushButton* savePicture_ = nullptr;
    QPushButton* saveWindow_ = nullptr;

    QWidget* createListingOptions();
    QWidget* createDisassemblyOptions();
    QWidget* createScreenOptions();
    tuxdsk::ScreenView screenView() const;
    void show_();
    void chooseAndSave(int what);
};
