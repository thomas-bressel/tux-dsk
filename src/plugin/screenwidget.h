#pragma once

#include <QImage>
#include <QWidget>

#include "view/screen.h"

class QLabel;
class QScrollArea;
class QToolButton;

// A picture of the CPC as the viewer shows it: the pens of a ScreenPicture
// in the colours of sixteen inks, each line drawn twice as the monitor
// draws it, and the inks under it, each a button that changes its colour:
// ManageDsk's "colour selection".
class ScreenWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScreenWidget(QWidget* parent = nullptr);

    void setPicture(const tuxdsk::ScreenPicture& picture, int mode);
    void setInks(const tuxdsk::Inks& inks);
    const tuxdsk::Inks& inks() const { return inks_; }
    // How many times its size the picture is drawn, 1 to 8.
    void setZoom(int zoom);

    // The picture as it is shown, a pixel for each of the CPC's in mode 2
    // and a line for each line: what "Save Picture" writes.
    QImage image() const;

    // One of the CPC's 27 colours, chosen in a window of them all; -1 if
    // none was.
    static int chooseColour(int current, QWidget* parent);

signals:
    void inksChanged();

private:
    tuxdsk::ScreenPicture picture_;
    tuxdsk::Inks inks_ = tuxdsk::defaultInks();
    int mode_ = 1;
    int zoom_ = 1;
    QLabel* view_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QToolButton* swatches_[16] = {};

    void draw();
    void drawSwatches();
};
