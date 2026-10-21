#include "screenwidget.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace tuxdsk {

namespace {

QColor colourOf(int number)
{
    const tuxdsk::CpcColour colour = tuxdsk::cpcColour(number);
    return QColor(colour.red, colour.green, colour.blue);
}

// A square of a colour, with the outline that tells it from the window.
QIcon swatch(int number, const QColor& outline)
{
    QPixmap pixmap(18, 18);
    pixmap.fill(colourOf(number));
    QPainter painter(&pixmap);
    painter.setPen(outline);
    painter.drawRect(0, 0, 17, 17);
    return QIcon(pixmap);
}

}  // namespace

ScreenWidget::ScreenWidget(QWidget* parent)
    : QWidget(parent)
{
    view_ = new QLabel;
    view_->setObjectName("lPicture");
    view_->setAlignment(Qt::AlignCenter);
    scroll_ = new QScrollArea;
    scroll_->setWidget(view_);
    scroll_->setWidgetResizable(true);
    scroll_->setAlignment(Qt::AlignCenter);

    auto* inks = new QHBoxLayout;
    inks->setSpacing(2);
    inks->addWidget(new QLabel(tr("Inks:")));
    for (int pen = 0; pen < 16; ++pen) {
        auto* button = new QToolButton;
        button->setObjectName(QStringLiteral("bInk%1").arg(pen));
        button->setAutoRaise(true);
        button->setIconSize(QSize(18, 18));
        connect(button, &QToolButton::clicked, this, [this, pen] {
            const int chosen = chooseColour(inks_[static_cast<size_t>(pen)], this);
            if (chosen < 0)
                return;
            inks_[static_cast<size_t>(pen)] = static_cast<uint8_t>(chosen);
            draw();
            emit inksChanged();
        });
        swatches_[pen] = button;
        inks->addWidget(button);
    }
    inks->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll_, 1);
    layout->addLayout(inks);
    drawSwatches();
}

void ScreenWidget::setPicture(const tuxdsk::ScreenPicture& picture, int mode)
{
    picture_ = picture;
    mode_ = mode;
    draw();
}

void ScreenWidget::setInks(const tuxdsk::Inks& inks)
{
    inks_ = inks;
    draw();
}

void ScreenWidget::setZoom(int zoom)
{
    zoom_ = qBound(1, zoom, 8);
    draw();
}

QImage ScreenWidget::image() const
{
    if (picture_.width <= 0 || picture_.height <= 0)
        return {};
    QImage image(picture_.width, picture_.height, QImage::Format_Indexed8);
    for (int pen = 0; pen < 16; ++pen)
        image.setColor(pen, colourOf(inks_[static_cast<size_t>(pen)]).rgb());
    for (int line = 0; line < picture_.height; ++line)
        std::copy_n(&picture_.pens[static_cast<size_t>(line) * static_cast<size_t>(picture_.width)], picture_.width,
                    image.scanLine(line));
    return image;
}

void ScreenWidget::draw()
{
    drawSwatches();
    const QImage picture = image();
    if (picture.isNull()) {
        view_->setPixmap(QPixmap());
        return;
    }
    // A pixel of mode 2 is half as wide as a line is high.
    view_->setPixmap(QPixmap::fromImage(picture.scaled(picture.width() * zoom_, picture.height() * 2 * zoom_)));
}

void ScreenWidget::drawSwatches()
{
    // A mode has the pens its pixels have bits for.
    const int pens = mode_ == 0 ? 16 : mode_ == 2 ? 2 : 4;
    const QColor outline = palette().color(QPalette::WindowText);
    for (int pen = 0; pen < 16; ++pen) {
        const int ink = inks_[static_cast<size_t>(pen)];
        swatches_[pen]->setIcon(swatch(ink, outline));
        swatches_[pen]->setToolTip(tr("Ink %1: colour %2").arg(pen).arg(ink));
        swatches_[pen]->setVisible(pen < pens);
    }
}

int ScreenWidget::chooseColour(int current, QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setObjectName("ColourDialog");
    dialog.setWindowTitle(tr("TuxDSK – Color Selection"));
    int chosen = -1;
    auto* grid = new QGridLayout;
    grid->setSpacing(4);
    const QColor outline = dialog.palette().color(QPalette::WindowText);
    for (int colour = 0; colour < 27; ++colour) {
        auto* button = new QToolButton;
        button->setObjectName(QStringLiteral("bColour%1").arg(colour));
        button->setIcon(swatch(colour, outline));
        button->setIconSize(QSize(18, 18));
        button->setText(QString::number(colour));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setCheckable(true);
        button->setChecked(colour == current);
        QObject::connect(button, &QToolButton::clicked, &dialog, [&dialog, &chosen, colour] {
            chosen = colour;
            dialog.accept();
        });
        grid->addWidget(button, colour / 9, colour % 9);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addLayout(grid);
    layout->addWidget(buttons);
    return dialog.exec() == QDialog::Accepted ? chosen : -1;
}

}  // namespace tuxdsk
