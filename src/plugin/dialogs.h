#pragma once

#include <cstdint>
#include <span>

#include <QDialog>

#include "disc/disc.h"
#include "disc/transfer.h"

class AddressBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;

// The windows of the manager that ask something before a thing is done.

// ManageDsk's "import file" window, shown for each file added when the
// import is the user's choice: whether the file has a header, and what it
// is to be on the disc, an ASCII file or a binary with its two addresses.
class ImportDialog : public QDialog {
    Q_OBJECT

public:
    // `name` is the file's name on the computer and `cpcName` the one it
    // will have on the disc.
    ImportDialog(const QString& name, const QString& cpcName, std::span<const uint8_t> file, QWidget* parent = nullptr);

    tuxdsk::ImportOptions options() const;

private:
    QRadioButton* ascii_ = nullptr;
    QRadioButton* binary_ = nullptr;
    AddressBox* load_ = nullptr;
    AddressBox* entry_ = nullptr;
};

// ManageDsk's "information about a file": its name, user and size, and what
// can be changed of it there: its type, where it loads and is entered, and
// its two attributes.
class FileInfoDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileInfoDialog(const tuxdsk::DiscFile& file, QWidget* parent = nullptr);

    tuxdsk::FileType type() const;
    int loadAddress() const;
    int entryAddress() const;
    bool readOnly() const;
    bool hidden() const;

private:
    QComboBox* type_ = nullptr;
    AddressBox* load_ = nullptr;
    AddressBox* entry_ = nullptr;
    QCheckBox* readOnly_ = nullptr;
    QCheckBox* hidden_ = nullptr;
};

// Another name and another user for a file. User 229 is that of erased
// files: given to a file it erases it, and taken from one it brings it back.
class RenameDialog : public QDialog {
    Q_OBJECT

public:
    explicit RenameDialog(const tuxdsk::DiscFile& file, QWidget* parent = nullptr);

    QString name() const;
    int user() const;

private:
    QLineEdit* name_ = nullptr;
    QSpinBox* user_ = nullptr;
};

// A new disc: how many tracks, and in which format.
class NewDiscDialog : public QDialog {
    Q_OBJECT

public:
    // With more than 42 tracks allowed, up to 80 can be asked for.
    explicit NewDiscDialog(bool moreTracks, QWidget* parent = nullptr);

    int tracks() const;
    tuxdsk::DiscFormat format() const;

private:
    QSpinBox* tracks_ = nullptr;
    QComboBox* format_ = nullptr;
};

// Tiles selected in TuxAPE's Find Graphics window, on their way to a file of
// the disc: what the file is to be called, and whether it is given an
// AMSDOS header, which has it load back where the tiles were.
class TilesDialog : public QDialog {
    Q_OBJECT

public:
    // `sprites` says they are a Plus's sprites, a byte to a pixel; `width`
    // is in bytes and `height` in lines.
    TilesDialog(int count, int width, int height, int address, bool sprites, const QString& name, bool header,
                QWidget* parent = nullptr);

    QString name() const;
    bool withHeader() const;

private:
    QLineEdit* name_ = nullptr;
    QCheckBox* header_ = nullptr;
};

// The window's "About".
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
};
