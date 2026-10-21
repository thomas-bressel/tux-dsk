#include "sectordialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "controls.h"
#include "view/text.h"

namespace tuxdsk {

namespace {

QString hex2(int value)
{
    return QStringLiteral("&%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

}  // namespace

SectorDialog::SectorDialog(const tuxdsk::CpcDisc& disc, const QString& name, QWidget* parent)
    : QDialog(parent)
    , disc_(disc)
{
    setObjectName("SectorDialog");
    setWindowTitle(tr("TuxDSK – Showing Sectors: %1").arg(name));
    const tuxdsk::DiscImage& image = disc_.image();

    track_ = new QSpinBox;
    track_->setObjectName("sbTrack");
    track_->setRange(0, std::max(image.tracks() - 1, 0));
    side_ = new QSpinBox;
    side_->setObjectName("sbSide");
    side_->setRange(0, std::max(image.sides() - 1, 0));
    side_->setEnabled(image.sides() > 1);
    sector_ = new QComboBox;
    sector_->setObjectName("cbSector");
    sector_->setFont(fixedFont());
    sector_->setMinimumContentsLength(6);
    QPushButton* previous = pageButton(tr("< &Previous"), "bPrevious");
    QPushButton* next = pageButton(tr("&Next >"), "bNext");
    QPushButton* directory = pageButton(tr("Show &Directory"), "bDirectory");
    directory->setEnabled(disc_.hasDirectory());
    detailed_ = new QCheckBox(tr("Detailed &informations"));
    detailed_->setObjectName("ckDetailed");
    details_ = new QLabel;
    details_->setObjectName("lDetails");
    details_->setFont(fixedFont());
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text_ = new QPlainTextEdit;
    text_->setObjectName("teText");
    text_->setReadOnly(true);
    text_->setFont(fixedFont());
    text_->setLineWrapMode(QPlainTextEdit::NoWrap);
    QPushButton* close = pageButton(tr("Close"), "bClose");

    connect(track_, &QSpinBox::valueChanged, this, [this] {
        listSectors();
        show_();
    });
    connect(side_, &QSpinBox::valueChanged, this, [this] {
        listSectors();
        show_();
    });
    connect(sector_, &QComboBox::currentIndexChanged, this, &SectorDialog::show_);
    connect(detailed_, &QCheckBox::toggled, this, &SectorDialog::show_);
    connect(previous, &QPushButton::clicked, this, [this] { step(-1); });
    connect(next, &QPushButton::clicked, this, [this] { step(1); });
    // The directory: the first sector, by its number, of the first track
    // that is not kept for the system.
    connect(directory, &QPushButton::clicked, this, [this] {
        const int reserved = disc_.format() == tuxdsk::DiscFormat::Vendor ? 2 : disc_.format() == tuxdsk::DiscFormat::Pcw ? 1 : 0;
        const tuxdsk::Track& track = disc_.image().track(reserved);
        for (size_t place = 0; place < track.sectors.size(); ++place)
            if (track.sectors[place].id == track.firstId())
                showSector(reserved, 0, static_cast<int>(place));
    });
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Track:")));
    top->addWidget(track_);
    top->addWidget(new QLabel(tr("Side:")));
    top->addWidget(side_);
    top->addWidget(new QLabel(tr("Sector:")));
    top->addWidget(sector_);
    top->addWidget(previous);
    top->addWidget(next);
    top->addWidget(directory);
    top->addStretch(1);
    top->addWidget(detailed_);
    auto* bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(close);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(details_);
    layout->addWidget(text_, 1);
    layout->addLayout(bottom);
    resize(760, 620);

    listSectors();
    show_();
}

void SectorDialog::listSectors()
{
    const QSignalBlocker quiet(sector_);
    sector_->clear();
    for (const tuxdsk::Sector& sector : disc_.image().track(track_->value(), side_->value()).sectors)
        sector_->addItem(hex2(sector.id));
}

void SectorDialog::showSector(int track, int side, int place)
{
    {
        const QSignalBlocker quietTrack(track_);
        const QSignalBlocker quietSide(side_);
        track_->setValue(track);
        side_->setValue(side);
    }
    listSectors();
    const QSignalBlocker quiet(sector_);
    sector_->setCurrentIndex(std::clamp(place, 0, std::max(sector_->count() - 1, 0)));
    show_();
}

int SectorDialog::track() const
{
    return track_->value();
}

int SectorDialog::side() const
{
    return side_->value();
}

int SectorDialog::place() const
{
    return sector_->currentIndex();
}

QString SectorDialog::text() const
{
    return text_->toPlainText();
}

QString SectorDialog::details() const
{
    return details_->text();
}

void SectorDialog::show_()
{
    const tuxdsk::Track& track = disc_.image().track(track_->value(), side_->value());
    if (!track.formatted()) {
        details_->setText(tr("This track is not formatted."));
        text_->clear();
        return;
    }
    const int place = sector_->currentIndex();
    if (place < 0 || place >= static_cast<int>(track.sectors.size())) {
        details_->setText(tr("This track has no sector."));
        text_->clear();
        return;
    }
    const tuxdsk::Sector& sector = track.sectors[static_cast<size_t>(place)];
    QString details = tr("Sector %1 of %2: number %3, %4 bytes")
                          .arg(place + 1)
                          .arg(track.sectors.size())
                          .arg(hex2(sector.id))
                          .arg(sector.data.size());
    if (detailed_->isChecked()) {
        details += '\n';
        details += tr("Track %1, side %2, size %3, ST1 %4, ST2 %5 - the track's gap %6, its filler %7")
                       .arg(sector.track)
                       .arg(sector.side)
                       .arg(sector.size)
                       .arg(hex2(sector.status1), hex2(sector.status2), hex2(track.gap()), hex2(track.filler()));
    }
    // What the controller found wrong when the sector was read from a real
    // disc: a copy protection, as often as not.
    if ((sector.status1 & 0x20) || (sector.status2 & 0x20))
        details += tr(" - read with a CRC error");
    if (sector.status2 & 0x40)
        details += tr(" - marked as deleted");
    details_->setText(details);
    const std::string dump = tuxdsk::hexDump(sector.data);
    text_->setPlainText(QString::fromLatin1(dump.c_str(), static_cast<qsizetype>(dump.size())));
}

// The next sector, or the one before: on the next track when the track's
// are done.
void SectorDialog::step(int by)
{
    int track = track_->value();
    int side = side_->value();
    int place = sector_->currentIndex() + by;
    const tuxdsk::DiscImage& image = disc_.image();
    const auto sectors = [&image](int t, int s) { return static_cast<int>(image.track(t, s).sectors.size()); };
    while (place < 0 || place >= sectors(track, side)) {
        if (by > 0) {
            if (side + 1 < image.sides()) {
                ++side;
            } else if (track + 1 < image.tracks()) {
                ++track;
                side = 0;
            } else {
                return;
            }
            place = 0;
            if (sectors(track, side) > 0)
                break;
        } else {
            if (side > 0) {
                --side;
            } else if (track > 0) {
                --track;
                side = image.sides() - 1;
            } else {
                return;
            }
            place = sectors(track, side) - 1;
            if (place >= 0)
                break;
        }
    }
    showSector(track, side, place);
}

}  // namespace tuxdsk
