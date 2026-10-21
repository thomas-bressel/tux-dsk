#pragma once

#include <QDialog>

#include "discpane.h"
#include "preferences.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QRadioButton;
namespace tuxape {
class PluginHost;
struct PluginTiles;
}

namespace tuxdsk {

// TuxDSK's window: ManageDsk's, with its two discs side by side.
//
// Along the top, ManageDsk's choices: what a file added is taken for, how
// a file is exported, the clean-up of headers, more than 42 tracks, and
// "use two windows", which shows the second disc. Between the two discs,
// what goes from one to the other: files copied either way, the discs
// swapped, and the discs or two files compared. Along the bottom, a line
// that says what was last done.
class ManagerWindow : public QDialog {
    Q_OBJECT

public:
    // `host` is TuxAPE, for the discs put in its drives; it may be null, as
    // it is when a test shows the window by itself.
    explicit ManagerWindow(tuxape::PluginHost* host, QWidget* parent = nullptr);

    // The first disc (0) and the second (1).
    DiscPane* pane(int index) const { return panes_[index]; }
    const ManagerOptions& options() const { return options_; }
    bool twoDiscs() const;
    void setTwoDiscs(bool on);

    // The files chosen on a disc, copied to the other; how many were.
    int copySelected(int from);
    void swapDiscs();
    // Compares the files chosen, one on each disc, or else the two discs;
    // what it finds, as it is said.
    QString compare();

    // The tiles selected in TuxAPE's Find Graphics window, put on the first
    // disc as a file whose name is asked for, "SPRITE.SPR" to begin with;
    // a disc is made if there is none. `asker` is the window the question
    // is asked over. False if nothing was put on the disc.
    bool takeTiles(const tuxape::PluginTiles& tiles, QWidget* asker = nullptr);
    // A file another plugin gives, put on the first disc as it is, a disc
    // made if there is none. Unless `replace`, a file of that name on the
    // disc is not replaced without asking. False if it was not put there.
    bool takeFile(const QString& name, const std::vector<uint8_t>& content, bool replace, QWidget* asker = nullptr);

    void setStatus(const QString& text);

protected:
    void closeEvent(QCloseEvent* event) override;
    // Escape closes the window as its own button does: asking first.
    void reject() override;

private:
    tuxape::PluginHost* host_;
    Preferences preferences_;
    ManagerOptions options_;
    DiscPane* panes_[2] = {};
    QWidget* between_ = nullptr;
    QCheckBox* two_ = nullptr;
    QRadioButton* import_[3] = {};
    QRadioButton* exportWith_ = nullptr;
    QRadioButton* exportWithout_ = nullptr;
    QCheckBox* clean_ = nullptr;
    QCheckBox* moreTracks_ = nullptr;
    QPushButton* copyRight_ = nullptr;
    QPushButton* copyLeft_ = nullptr;
    QPushButton* swap_ = nullptr;
    QPushButton* compare_ = nullptr;
    QLabel* status_ = nullptr;

    QWidget* createOptions();
    void readOptions();
    void updateButtons();
};

}  // namespace tuxdsk
