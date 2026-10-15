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
#include "plugin.h"
#include "prompts.h"

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
        panes_[index]->setObjectName(index == 0 ? "DiscA" : "DiscB");
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
    const char* names[3] = {"rImportAsk", "rImportAscii", "rImportBinary"};
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
    exportWith_->setObjectName("rExportHeader");
    exportWithout_ = new QRadioButton(tr("With&out header"));
    exportWithout_->setObjectName("rExportPlain");
    (preferences_.exportHeader() ? exportWith_ : exportWithout_)->setChecked(true);
    exportLayout->addWidget(exportWith_);
    exportLayout->addWidget(exportWithout_);
    connect(exportWith_, &QRadioButton::toggled, this, &ManagerWindow::readOptions);

    clean_ = new QCheckBox(tr("Auto clean-up headers in AMSDOS format"));
    clean_->setObjectName("xClean");
    clean_->setToolTip(tr("The bytes AMSDOS does not use in a header are made 0, in files added and in files exported"));
    clean_->setChecked(preferences_.cleanHeaders());
    moreTracks_ = new QCheckBox(tr("Allow more than 42 tracks"));
    moreTracks_->setObjectName("xMoreTracks");
    moreTracks_->setToolTip(tr("Files may go on past the 42nd track, to block 255: a real drive may not read that far"));
    moreTracks_->setChecked(preferences_.moreTracks());
    two_ = new QCheckBox(tr("Use &two windows"));
    two_->setObjectName("xTwo");
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
    const std::vector<tuxdsk::DiscFile> chosen = source->selectedFiles();
    int copied = 0;
    bool replaceAll = false;
    for (size_t at = 0; at < chosen.size(); ++at) {
        const tuxdsk::DiscFile& file = chosen[at];
        const QString name = QString::fromStdString(file.name);
        const std::vector<uint8_t> records = source->disc()->read(file);
        tuxdsk::AddResult result = target->putFile(file, records, replaceAll);
        // ManageDsk does not put a file twice on a disc: it asks.
        if (result == tuxdsk::AddResult::Exists) {
            const prompts::Answer answer =
                prompts::ask(this, {tr("Copy"), tr("File %1 already exists. Replace?").arg(name), at + 1 < chosen.size(), QString()});
            if (answer == prompts::Answer::Cancel)
                break;
            if (answer == prompts::Answer::No)
                continue;
            replaceAll = answer == prompts::Answer::YesToAll;
            result = target->putFile(file, records, true);
        }
        if (result == tuxdsk::AddResult::NoSpace) {
            prompts::problem(this, tr("No more space on the floppy!"));
            break;
        }
        if (result == tuxdsk::AddResult::NoEntry) {
            prompts::problem(this, tr("No more entry in the directory!"));
            break;
        }
        if (result != tuxdsk::AddResult::Done) {
            prompts::problem(this, tr("%1 cannot be copied to this disc.").arg(name));
            break;
        }
        ++copied;
    }
    setStatus(tr("%1 file(s) copied to %2.").arg(copied).arg(target->name()));
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
        const std::optional<std::string> difference =
            tuxdsk::firstDifference(panes_[0]->disc()->image(), panes_[1]->disc()->image());
        found = difference ? tr("The DSKs are different: %1").arg(QString::fromStdString(*difference))
                           : tr("The DSKs are identical");
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
    DiscPane* disc = panes_[0];
    const auto onDisc = [disc](const QString& name) {
        return disc->hasDisc() && disc->disc()->find(name.toStdString()).has_value();
    };
    // The first name of the kind the disc does not have: SPRITE.SPR,
    // SPRITE1.SPR, SPRITE2.SPR...
    QString name = QStringLiteral("SPRITE.SPR");
    for (int number = 1; onDisc(name) && number < 100; ++number)
        name = QStringLiteral("SPRITE%1.SPR").arg(number);

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

    std::vector<uint8_t> content;
    if (header)
        content = tuxdsk::makeAmsdosHeader(name.toStdString(), tuxdsk::kAmsdosBinary, static_cast<uint16_t>(tiles.address),
                                           0, static_cast<size_t>(tiles.bytes.size()));
    content.insert(content.end(), tiles.bytes.begin(), tiles.bytes.end());
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
