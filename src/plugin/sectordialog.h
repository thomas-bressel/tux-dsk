#pragma once

#include <QDialog>

#include "disc/disc.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;

namespace tuxdsk {

// ManageDsk's sector viewer: the sectors of a disc, one at a time, as bytes
// and as characters, with what the track and the sector say of themselves.
// It shows the disc as it was when the window was opened.
class SectorDialog : public QDialog {
    Q_OBJECT

public:
    SectorDialog(const tuxdsk::CpcDisc& disc, const QString& name, QWidget* parent = nullptr);

    // The sector shown: a track, a side, and its place on the track.
    void showSector(int track, int side, int place);
    int track() const;
    int side() const;
    int place() const;
    // What the window says: the sector's bytes, and the line above them.
    QString text() const;
    QString details() const;

private:
    tuxdsk::CpcDisc disc_;
    QSpinBox* track_ = nullptr;
    QSpinBox* side_ = nullptr;
    QComboBox* sector_ = nullptr;
    QCheckBox* detailed_ = nullptr;
    QLabel* details_ = nullptr;
    QPlainTextEdit* text_ = nullptr;

    void listSectors();
    void show_();
    void step(int by);
};

}  // namespace tuxdsk
