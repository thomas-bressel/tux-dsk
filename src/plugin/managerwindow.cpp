#include "managerwindow.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "controls.h"
#include "dialogs.h"
#include "disc/amsdos.h"
#include "disc/compare.h"
#include "view/screen.h"
#include "plugin.h"
#include "prompts.h"

namespace tuxdsk {

namespace {

// Where two discs first differ, in a sentence.
QString differenceText(const tuxdsk::Difference& difference)
{
    const QString sector = QStringLiteral("&%1").arg(difference.sector, 2, 16, QLatin1Char('0')).toUpper();
    const bool sided = difference.side >= 0;
    switch (difference.kind) {
    case tuxdsk::Difference::Tracks:
        return ManagerWindow::tr("The DSKs are different: %1 tracks and %2").arg(difference.one).arg(difference.other);
    case tuxdsk::Difference::Sides:
        return ManagerWindow::tr("The DSKs are different: %1 sides and %2").arg(difference.one).arg(difference.other);
    case tuxdsk::Difference::SectorCount:
        return sided ? ManagerWindow::tr("The DSKs are different: track %1, side %2: %3 sectors and %4")
                           .arg(difference.track)
                           .arg(difference.side)
                           .arg(difference.one)
                           .arg(difference.other)
                     : ManagerWindow::tr("The DSKs are different: track %1: %2 sectors and %3")
                           .arg(difference.track)
                           .arg(difference.one)
                           .arg(difference.other);
    case tuxdsk::Difference::Numbering:
        return sided ? ManagerWindow::tr("The DSKs are different: track %1, side %2: the sectors are not numbered alike")
                           .arg(difference.track)
                           .arg(difference.side)
                     : ManagerWindow::tr("The DSKs are different: track %1: the sectors are not numbered alike")
                           .arg(difference.track);
    case tuxdsk::Difference::Bytes:
        return sided ? ManagerWindow::tr("The DSKs are different: track %1, side %2, sector %3")
                           .arg(difference.track)
                           .arg(difference.side)
                           .arg(sector)
                     : ManagerWindow::tr("The DSKs are different: track %1, sector %2").arg(difference.track).arg(sector);
    }
    return {};
}

}  // namespace

ManagerWindow::ManagerWindow(tuxape::PluginHost* host, QWidget* parent)
    : QDialog(parent)
    , host_(host)
    , preferences_(host ? host->dataFolder() : QString())
{
    setObjectName("TuxDskWindow");
    setWindowTitle(tr("TuxDSK – Disc Manager"));

    status_ = new QLabel;
    status_->setObjectName("lStatus");
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // However long what it says, the window stays the width it is.
    status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    QWidget* options = createOptions();
    for (int index = 0; index < 2; ++index) {
        panes_[index] = new DiscPane(host_, &preferences_, &options_);
        panes_[index]->setObjectName(index == 0 ? "pnDiscA" : "pnDiscB");
        connect(panes_[index], &DiscPane::status, this, &ManagerWindow::setStatus);
        connect(panes_[index], &DiscPane::changed, this, &ManagerWindow::updateButtons);
    }

    copyRight_ = pageButton(tr("Copy =>"), "bCopyRight");
    copyLeft_ = pageButton(tr("Copy <="), "bCopyLeft");
    swap_ = pageButton(tr("Swap"), "bSwap");
    compare_ = pageButton(tr("Compare"), "bCompare");
    compare_->setToolTip(tr("Compares the two files chosen, one on each disc, or else the two discs"));
    connect(copyRight_, &QPushButton::clicked, this, [this] { copySelected(0); });
    connect(copyLeft_, &QPushButton::clicked, this, [this] { copySelected(1); });
    connect(swap_, &QPushButton::clicked, this, &ManagerWindow::swapDiscs);
    connect(compare_, &QPushButton::clicked, this, [this] { prompts::inform(this, compare()); });
    between_ = new QWidget;
    auto* middle = new QVBoxLayout(between_);
    middle->setContentsMargins(0, 0, 0, 0);
    middle->addStretch(1);
    for (QPushButton* button : {copyRight_, copyLeft_, swap_, compare_})
        middle->addWidget(button);
    middle->addStretch(2);

    QPushButton* about = pageButton(tr("About..."), "bAbout");
    connect(about, &QPushButton::clicked, this, [this] {
        AboutDialog dialog(this);
        dialog.exec();
    });
    QPushButton* close = pageButton(tr("Close"), "bClose");
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    auto* discs = new QHBoxLayout;
    discs->addWidget(panes_[0], 1);
    discs->addWidget(between_);
    discs->addWidget(panes_[1], 1);
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(status_, 1);
    bottom->addWidget(about);
    bottom->addWidget(close);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(options);
    layout->addLayout(discs, 1);
    layout->addLayout(bottom);

    readOptions();
    setTwoDiscs(preferences_.twoDiscs());
    updateButtons();
}

QWidget* ManagerWindow::createOptions()
{
    auto* importBox = new QGroupBox(tr("Import:"));
    auto* importLayout = new QHBoxLayout(importBox);
    const QString labels[3] = {tr("&User choice"), tr("Force ASC&II"), tr("Force &BINARY")};
    const char* names[3] = {"rbImportAsk", "rbImportAscii", "rbImportBinary"};
    auto* importGroup = new QButtonGroup(this);
    for (int mode = 0; mode < 3; ++mode) {
        import_[mode] = new QRadioButton(labels[mode]);
        import_[mode]->setObjectName(names[mode]);
        importGroup->addButton(import_[mode], mode);
        importLayout->addWidget(import_[mode]);
        connect(import_[mode], &QRadioButton::toggled, this, &ManagerWindow::readOptions);
    }
    import_[qBound(0, preferences_.importMode(), 2)]->setChecked(true);

    auto* exportBox = new QGroupBox(tr("Export:"));
    auto* exportLayout = new QHBoxLayout(exportBox);
    exportWith_ = new QRadioButton(tr("&With header"));
    exportWith_->setObjectName("rbExportHeader");
    exportWithout_ = new QRadioButton(tr("With&out header"));
    exportWithout_->setObjectName("rbExportPlain");
    (preferences_.exportHeader() ? exportWith_ : exportWithout_)->setChecked(true);
    exportLayout->addWidget(exportWith_);
    exportLayout->addWidget(exportWithout_);
    connect(exportWith_, &QRadioButton::toggled, this, &ManagerWindow::readOptions);

    clean_ = new QCheckBox(tr("Auto clean-up headers in AMSDOS format"));
    clean_->setObjectName("ckClean");
    clean_->setToolTip(tr("The bytes AMSDOS does not use in a header are made 0, in files added and in files exported"));
    clean_->setChecked(preferences_.cleanHeaders());
    moreTracks_ = new QCheckBox(tr("Allow more than 42 tracks"));
    moreTracks_->setObjectName("ckMoreTracks");
    moreTracks_->setToolTip(tr("Files may go on past the 42nd track, to block 255: a real drive may not read that far"));
    moreTracks_->setChecked(preferences_.moreTracks());
    two_ = new QCheckBox(tr("Use &two windows"));
    two_->setObjectName("ckTwo");
    two_->setToolTip(tr("A second disc beside the first, to copy files across and to compare"));
    connect(clean_, &QCheckBox::toggled, this, &ManagerWindow::readOptions);
    connect(moreTracks_, &QCheckBox::toggled, this, &ManagerWindow::readOptions);
    connect(two_, &QCheckBox::toggled, this, &ManagerWindow::setTwoDiscs);

    auto* checks = new QVBoxLayout;
    checks->setSpacing(2);
    checks->addWidget(clean_);
    checks->addWidget(moreTracks_);
    auto* piece = new QWidget;
    auto* layout = new QHBoxLayout(piece);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(importBox);
    layout->addWidget(exportBox);
    layout->addLayout(checks);
    layout->addStretch(1);
    layout->addWidget(two_);
    return piece;
}

// The choices as the controls have them: kept for the discs to work by,
// and for the next time.
void ManagerWindow::readOptions()
{
    if (!moreTracks_)
        return;
    for (int mode = 0; mode < 3; ++mode)
        if (import_[mode]->isChecked())
            options_.importMode = mode;
    options_.exportHeader = exportWith_->isChecked();
    options_.cleanHeaders = clean_->isChecked();
    options_.moreTracks = moreTracks_->isChecked();
    preferences_.setImportMode(options_.importMode);
    preferences_.setExportHeader(options_.exportHeader);
    preferences_.setCleanHeaders(options_.cleanHeaders);
    preferences_.setMoreTracks(options_.moreTracks);
}

bool ManagerWindow::twoDiscs() const
{
    return two_->isChecked();
}

void ManagerWindow::setTwoDiscs(bool on)
{
    two_->setChecked(on);
    between_->setVisible(on);
    panes_[1]->setVisible(on);
    preferences_.setTwoDiscs(on);
    // Two discs take twice the room.
    if (!isVisible())
        resize(on ? 1480 : 860, 660);
    updateButtons();
}

void ManagerWindow::updateButtons()
{
    if (!compare_)
        return;
    const bool both = panes_[0]->hasDisc() && panes_[1]->hasDisc();
    copyRight_->setEnabled(both && !panes_[0]->selectedFiles().empty());
    copyLeft_->setEnabled(both && !panes_[1]->selectedFiles().empty());
    swap_->setEnabled(panes_[0]->hasDisc() || panes_[1]->hasDisc());
    compare_->setEnabled(both);
}

int ManagerWindow::copySelected(int from)
{
    DiscPane* source = panes_[from];
    DiscPane* target = panes_[1 - from];
    if (!source->hasDisc() || !target->hasDisc()) {
        prompts::problem(this, tr("No DSK loaded."));
        return 0;
    }
    const int copied = target->putFiles(*source->disc(), source->selectedFiles());
    setStatus(tr("%n file(s) copied to %1.", nullptr, copied).arg(target->name()));
    return copied;
}

void ManagerWindow::swapDiscs()
{
    panes_[0]->exchange(panes_[1]);
    setStatus(tr("The two discs have changed places."));
}

QString ManagerWindow::compare()
{
    if (!panes_[0]->hasDisc() || !panes_[1]->hasDisc())
        return tr("No DSK loaded.");
    const std::vector<tuxdsk::DiscFile> left = panes_[0]->selectedFiles();
    const std::vector<tuxdsk::DiscFile> right = panes_[1]->selectedFiles();
    QString found;
    if (left.size() == 1 && right.size() == 1) {
        const bool same = panes_[0]->disc()->read(left[0]) == panes_[1]->disc()->read(right[0]);
        found = (same ? tr("Files %1 and %2 identical") : tr("Files %1 and %2 different"))
                    .arg(QString::fromStdString(left[0].name), QString::fromStdString(right[0].name));
    } else {
        const std::optional<tuxdsk::Difference> difference =
            tuxdsk::firstDifference(panes_[0]->disc()->image(), panes_[1]->disc()->image());
        found = difference ? differenceText(*difference) : tr("The DSKs are identical");
    }
    setStatus(found);
    return found;
}

bool ManagerWindow::takeTiles(const tuxape::PluginTiles& tiles, QWidget* asker)
{
    QWidget* over = asker ? asker : this;
    if (tiles.bytes.isEmpty() || tiles.count <= 0) {
        prompts::problem(over, tr("No tile is selected in the Find Graphics window."));
        return false;
    }
    // A Plus's sprites go as they are, a .SPR. Tiles of the screen's modes
    // go as a window of OCP Art Studio, a .WIN: the tiles side by side, as
    // Find Graphics shows them in a row, line after line, then five bytes
    // that give the window's size, as TuxASC writes them.
    const bool sprites = tiles.mode == 3;
    const std::span<const uint8_t> selected(reinterpret_cast<const uint8_t*>(tiles.bytes.constData()),
                                            static_cast<size_t>(tiles.bytes.size()));
    const std::optional<std::vector<uint8_t>> window =
        sprites ? std::nullopt : tuxdsk::tilesWindow(selected, tiles.count, tiles.width, tiles.height);
    if (!sprites && !window) {
        prompts::problem(over, tr("A window of OCP Art Studio is at most 255 lines high and 8191 bytes wide: "
                                  "select fewer tiles, or smaller ones."));
        return false;
    }
    DiscPane* disc = panes_[0];
    const auto onDisc = [disc](const QString& name) {
        return disc->hasDisc() && disc->disc()->find(name.toStdString()).has_value();
    };
    // The first name of the kind the disc does not have: SPRITE.SPR,
    // SPRITE1.SPR, SPRITE2.SPR... or SCREEN.WIN, SCREEN1.WIN...
    const QString stem = sprites ? QStringLiteral("SPRITE") : QStringLiteral("SCREEN");
    const QString suffix = sprites ? QStringLiteral(".SPR") : QStringLiteral(".WIN");
    QString name = stem + suffix;
    for (int number = 1; onDisc(name) && number < 100; ++number)
        name = stem + QString::number(number) + suffix;

    bool header = preferences_.tilesHeader();
    bool replace = false;
    for (;;) {
        TilesDialog dialog(tiles.count, tiles.width, tiles.height, tiles.address, tiles.mode == 3, name, header, over);
        if (dialog.exec() != QDialog::Accepted)
            return false;
        name = dialog.name();
        header = dialog.withHeader();
        if (!onDisc(name))
            break;
        const prompts::Answer answer =
            prompts::ask(over, {dialog.windowTitle(), tr("File %1 already exists. Replace?").arg(name), false, QString()});
        if (answer == prompts::Answer::Cancel)
            return false;
        // No: another name is asked for.
        if (answer == prompts::Answer::Yes) {
            replace = true;
            break;
        }
    }
    preferences_.setTilesHeader(header);

    const std::vector<uint8_t> bytes = sprites ? std::vector<uint8_t>(selected.begin(), selected.end()) : *window;
    std::vector<uint8_t> content;
    if (header)
        content = tuxdsk::makeAmsdosHeader(name.toStdString(), tuxdsk::kAmsdosBinary,
                                           static_cast<uint16_t>(sprites ? tiles.address : tuxdsk::kWindowAddress), 0,
                                           bytes.size());
    content.insert(content.end(), bytes.begin(), bytes.end());
    // No disc yet: the tiles are given one.
    const bool made = !disc->hasDisc();
    if (made)
        disc->newDisc();
    if (!disc->addBytes(name, content, replace))
        return false;
    setStatus((made ? tr("A disc was made for %1: Save DSK writes it to a file.") : tr("%1 is on the disc: Save DSK writes it to its file."))
                  .arg(name));
    return true;
}

bool ManagerWindow::takeFile(const QString& name, const std::vector<uint8_t>& content, bool replace, QWidget* asker)
{
    QWidget* over = asker ? asker : this;
    DiscPane* disc = panes_[0];
    const bool there = disc->hasDisc() && disc->disc()->find(name.toStdString()).has_value();
    if (there && !replace) {
        const prompts::Answer answer =
            prompts::ask(over, {windowTitle(), tr("File %1 already exists. Replace?").arg(name), false, QString()});
        if (answer != prompts::Answer::Yes)
            return false;
    }
    // No disc yet: the file is given one.
    const bool made = !disc->hasDisc();
    if (made)
        disc->newDisc();
    if (!disc->addBytes(name, content, there))
        return false;
    setStatus((made ? tr("A disc was made for %1: Save DSK writes it to a file.") : tr("%1 is on the disc: Save DSK writes it to its file."))
                  .arg(name));
    return true;
}

void ManagerWindow::setStatus(const QString& text)
{
    status_->setText(text);
}

void ManagerWindow::closeEvent(QCloseEvent* event)
{
    // A disc that was changed and not saved is asked about.
    for (DiscPane* pane : panes_) {
        if (!pane->mayLeave()) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void ManagerWindow::reject()
{
    close();
}

}  // namespace tuxdsk
