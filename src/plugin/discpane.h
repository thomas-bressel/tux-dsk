#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QString>
#include <QStringList>
#include <QWidget>

#include "disc/disc.h"
#include "disc/transfer.h"

class Preferences;
class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class SectorDialog;
class ViewerDialog;
namespace tuxape {
class PluginHost;
}

// The choices of the manager's window, which its discs work by.
struct ManagerOptions {
    enum Import { Ask, ForceAscii, ForceBinary };

    int importMode = Ask;       // what a file added is taken for
    bool exportHeader = true;   // a file exported keeps its header
    bool cleanHeaders = false;  // ManageDsk's "auto clean-up" of headers, both ways
    bool moreTracks = false;    // files may go past a 42nd track

    int blocks() const { return moreTracks ? tuxdsk::kAllBlocks : tuxdsk::kWindowBlocks; }
};

// One of the manager's two discs: ManageDsk's list of the files of a disc,
// with the buttons that work on the disc and on the files chosen in the
// list. The disc is read from, and saved to, a .DSK file or an .HFE one.
//
// The list has a line a file: its name, user, attributes, type, size and
// the addresses of its header; files that were erased and can still be
// brought back are in red, with user 229. "Show Directory" lists the 64
// entries of the directory as they are instead.
//
// What a button does by asking (a file's name, a confirmation) is also
// there as a function that is told: the windows' tests use those.
class DiscPane : public QWidget {
    Q_OBJECT

public:
    DiscPane(tuxape::PluginHost* host, Preferences* preferences, const ManagerOptions* options,
             QWidget* parent = nullptr);

    bool hasDisc() const { return disc_.has_value(); }
    bool modified() const { return modified_; }
    // The disc's file, empty for one not saved yet, and what the disc is
    // called in the windows.
    QString path() const { return path_; }
    QString name() const;
    const tuxdsk::CpcDisc* disc() const { return disc_ ? &*disc_ : nullptr; }
    QTreeWidget* list() const { return list_; }

    // ---- The disc ----
    void newDisc(int tracks = 42, tuxdsk::DiscFormat format = tuxdsk::DiscFormat::Data);
    // Reads a disc from a .DSK file or from an HxC Floppy Emulator's .HFE
    // file, whichever the file is.
    bool openDisc(const QString& path);
    // Saves it as an .HFE file, for a Gotek or an HxC drive on a real CPC,
    // if the name ends in ".hfe"; as a .DSK file under any other name.
    bool saveDiscAs(const QString& path);
    // The name a disc is saved under, from the one typed in the window that
    // asks and the kind of file chosen there: a name that says neither
    // ".dsk" nor ".hfe" is given the ending of the kind chosen.
    static QString savedPath(const QString& typed, bool hfe);
    // To its file, or to one that is asked for if it has none.
    bool saveDisc();
    // Asks before a disc that was changed and not saved is let go; true if
    // it may be.
    bool mayLeave();
    // The two discs change places.
    void exchange(DiscPane* other);

    // ---- Its files ----
    // The files chosen in the list, and files chosen by their names.
    std::vector<tuxdsk::DiscFile> selectedFiles() const;
    void select(const QStringList& names);
    // A file of the computer put on the disc; in place of the one of its
    // name if `replace`.
    tuxdsk::AddResult addFile(const QString& path, const tuxdsk::ImportOptions& options, bool replace = false);
    // Several, as the window's import mode says, asking what it asks.
    void addFiles(const QStringList& paths);
    // Bytes that are not a file of the computer, put on the disc as they
    // are under a name; in place of the file of that name if `replace`.
    // Says why if they do not go on, and returns false.
    bool addBytes(const QString& name, const std::vector<uint8_t>& content, bool replace = false);
    // A file of the other disc: its records, with its user and attributes.
    tuxdsk::AddResult putFile(const tuxdsk::DiscFile& file, const std::vector<uint8_t>& records, bool replace);
    // A file written to the computer, as the window's export mode says.
    bool exportFile(const tuxdsk::DiscFile& file, const QString& path);
    // Several, each under its name, into a folder; how many were written.
    int exportFiles(const std::vector<tuxdsk::DiscFile>& files, const QString& folder);
    bool renameFile(const tuxdsk::DiscFile& file, const QString& name, int user);
    void eraseFile(const tuxdsk::DiscFile& file, bool forGood);
    // What "information about a file" changes.
    bool changeFile(const tuxdsk::DiscFile& file, tuxdsk::FileType type, int loadAddress, int entryAddress,
                    bool readOnly, bool hidden);
    ViewerDialog* viewFile(const tuxdsk::DiscFile& file);
    SectorDialog* showSectors();
    // The disc in a drive of TuxAPE (0 for A:); it is saved first if it has
    // to be.
    bool insertInDrive(int drive);

    // ---- What the buttons do ----
    void chooseNew();
    void chooseOpen();
    void chooseSaveAs();
    void chooseAdd();
    void chooseExport();
    void renameSelected();
    void deleteSelected();
    void showInformation();
    void viewSelected();
    void runSelected();

signals:
    // What was just done, for the line along the bottom of the window.
    void status(const QString& text);
    // The disc, or what is chosen in the list, is no longer what it was.
    void changed();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    tuxape::PluginHost* host_;
    Preferences* preferences_;
    const ManagerOptions* options_;
    std::optional<tuxdsk::CpcDisc> disc_;
    QString path_;
    bool modified_ = false;
    std::vector<tuxdsk::DiscFile> files_;  // those of the list, in its order

    QLabel* title_ = nullptr;
    QTreeWidget* list_ = nullptr;
    QCheckBox* showDirectory_ = nullptr;
    QCheckBox* hexadecimal_ = nullptr;
    QPushButton* save_ = nullptr;
    QPushButton* saveAs_ = nullptr;
    QPushButton* add_ = nullptr;
    QPushButton* export_ = nullptr;
    QPushButton* rename_ = nullptr;
    QPushButton* delete_ = nullptr;
    QPushButton* info_ = nullptr;
    QPushButton* view_ = nullptr;
    QPushButton* sectors_ = nullptr;
    QPushButton* insertA_ = nullptr;
    QPushButton* insertB_ = nullptr;
    QPushButton* run_ = nullptr;

    void setDisc(tuxdsk::CpcDisc disc, const QString& path, bool modified);
    void touched(const QString& said);
    void refresh();
    void listFiles();
    void listDirectory();
    void updateButtons();
    void showMenu(const QPoint& where);
    QString number(int value, int digits = 4) const;
    QString startFolder(bool discs) const;
    bool tell(tuxdsk::AddResult result, const QString& name);
};
