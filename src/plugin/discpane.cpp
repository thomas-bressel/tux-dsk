#include "discpane.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "controls.h"
#include "dialogs.h"
#include "disc/amsdos.h"
#include "disc/hfe.h"
#include "disc/names.h"
#include "hostfiles.h"
#include "plugin.h"
#include "preferences.h"
#include "prompts.h"
#include "sectordialog.h"
#include "viewerdialog.h"

namespace tuxdsk {

namespace {

using prompts::Answer;

enum Column { Name, User, ReadOnly, Hidden, Type, Size, Start, Entry, Columns };

const int kFileRole = Qt::UserRole;

// The two kinds of file a disc is kept in: a .DSK file, for emulators, and
// an HxC Floppy Emulator's .HFE file, for a real CPC.
QString dskFilter()
{
    return DiscPane::tr("DSK Files (*.dsk *.DSK)");
}

QString hfeFilter()
{
    return DiscPane::tr("HFE Files, for a Gotek or an HxC drive on a real CPC (*.hfe *.HFE)");
}

// Why a file was not read as a disc, in a sentence.
QString notADisc(const QString& path, ImageError error)
{
    const QString file = QDir::toNativeSeparators(path);
    switch (error) {
    case ImageError::None:
        break;
    case ImageError::TooShort:
        return DiscPane::tr("%1 is too short to be a disc image.").arg(file);
    case ImageError::NotDsk:
        return DiscPane::tr("%1 is not a .DSK disc image.").arg(file);
    case ImageError::NoTrack:
        return DiscPane::tr("The disc image %1 has no track.").arg(file);
    case ImageError::TooManyTracks:
        return DiscPane::tr("The disc image %1 has more tracks than its header can list.").arg(file);
    case ImageError::NotHfe:
        return DiscPane::tr("%1 is not an .HFE disc image.").arg(file);
    case ImageError::HfeKind:
        return DiscPane::tr("The .HFE image %1 is of a kind that is not read: only the first, in MFM, is.").arg(file);
    case ImageError::HfeNoTrack:
        return DiscPane::tr("The .HFE image %1 has no track.").arg(file);
    }
    return DiscPane::tr("%1 is not a disc image.").arg(file);
}

// What a disc is read from: either kind.
QString discFilters()
{
    return DiscPane::tr("Disc Images (*.dsk *.DSK *.hfe *.HFE)") + QStringLiteral(";;") + dskFilter() + QStringLiteral(";;")
           + hfeFilter() + QStringLiteral(";;") + DiscPane::tr("All Files (*)");
}

bool namesHfe(const QString& path)
{
    return QFileInfo(path).suffix().compare(QLatin1String("hfe"), Qt::CaseInsensitive) == 0;
}

// The colour of erased files: ManageDsk's red, one that reads on the look
// in use, light or dark.
QColor erasedColour(const QPalette& palette)
{
    return palette.color(QPalette::Base).lightness() < 128 ? QColor("#FF8A80") : QColor("#B00020");
}

uint8_t typeByte(tuxdsk::FileType type)
{
    switch (type) {
    case tuxdsk::FileType::Basic: return tuxdsk::kAmsdosBasic;
    case tuxdsk::FileType::ProtectedBasic: return tuxdsk::kAmsdosProtectedBasic;
    case tuxdsk::FileType::ProtectedBinary: return tuxdsk::kAmsdosProtectedBinary;
    default: return tuxdsk::kAmsdosBinary;
    }
}

}  // namespace

DiscPane::DiscPane(tuxape::PluginHost* host, Preferences* preferences, const ManagerOptions* options, QWidget* parent)
    : QWidget(parent)
    , host_(host)
    , preferences_(preferences)
    , options_(options)
{
    setAcceptDrops(true);

    title_ = new QLabel;
    title_->setObjectName("lTitle");
    title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // However long what it says, the window stays the width it is.
    title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    list_ = new QTreeWidget;
    list_->setObjectName("lvFiles");
    list_->setRootIsDecorated(false);
    list_->setUniformRowHeights(true);
    list_->setAlternatingRowColors(true);
    list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list_->setFont(fixedFont());
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list_, &QTreeWidget::itemSelectionChanged, this, [this] {
        updateButtons();
        emit changed();
    });
    // A double click on a file is ManageDsk's: what is known of it.
    connect(list_, &QTreeWidget::itemDoubleClicked, this, &DiscPane::showInformation);
    connect(list_, &QTreeWidget::customContextMenuRequested, this, &DiscPane::showMenu);
    auto* deleteKey = new QShortcut(QKeySequence::Delete, list_);
    deleteKey->setContext(Qt::WidgetShortcut);
    connect(deleteKey, &QShortcut::activated, this, &DiscPane::deleteSelected);

    add_ = pageButton(tr("&Add File(s)..."), "bAdd");
    importDisc_ = pageButton(tr("Im&port from DSK..."), "bImportDisc");
    importDisc_->setToolTip(tr("Puts on this disc files of another one, read from its .DSK or .HFE file"));
    export_ = pageButton(tr("&Export File(s)..."), "bExport");
    rename_ = pageButton(tr("&Rename File(s)..."), "bRename");
    delete_ = pageButton(tr("&Delete File(s)..."), "bDelete");
    info_ = pageButton(tr("&Information..."), "bInfo");
    view_ = pageButton(tr("Show &File"), "bView");
    sectors_ = pageButton(tr("Show Sec&tors"), "bSectors");
    showDirectory_ = new QCheckBox(tr("Show Director&y"));
    showDirectory_->setObjectName("ckDirectory");
    showDirectory_->setToolTip(tr("The 64 entries of the directory as they are, instead of the files"));
    hexadecimal_ = new QCheckBox(tr("&Hexadecimal display"));
    hexadecimal_->setObjectName("ckHexadecimal");
    QPushButton* create = pageButton(tr("&Create DSK..."), "bNew");
    QPushButton* open = pageButton(tr("Read DS&K..."), "bOpen");
    save_ = pageButton(tr("&Save DSK"), "bSave");
    saveAs_ = pageButton(tr("Save As..."), "bSaveAs");
    insertA_ = pageButton(tr("Insert in A:"), "bInsertA");
    insertB_ = pageButton(tr("Insert in B:"), "bInsertB");
    run_ = pageButton(tr("Run"), "bRun");
    insertA_->setToolTip(tr("Puts the disc in drive A: of TuxAPE's CPC"));
    insertB_->setToolTip(tr("Puts the disc in drive B: of TuxAPE's CPC"));
    run_->setToolTip(tr("Puts the disc in drive A: of TuxAPE's CPC and runs the file chosen"));

    connect(add_, &QPushButton::clicked, this, &DiscPane::chooseAdd);
    connect(importDisc_, &QPushButton::clicked, this, &DiscPane::chooseImportDisc);
    connect(export_, &QPushButton::clicked, this, &DiscPane::chooseExport);
    connect(rename_, &QPushButton::clicked, this, &DiscPane::renameSelected);
    connect(delete_, &QPushButton::clicked, this, &DiscPane::deleteSelected);
    connect(info_, &QPushButton::clicked, this, &DiscPane::showInformation);
    connect(view_, &QPushButton::clicked, this, &DiscPane::viewSelected);
    connect(sectors_, &QPushButton::clicked, this, &DiscPane::showSectors);
    connect(showDirectory_, &QCheckBox::toggled, this, &DiscPane::refresh);
    connect(hexadecimal_, &QCheckBox::toggled, this, &DiscPane::refresh);
    connect(create, &QPushButton::clicked, this, &DiscPane::chooseNew);
    connect(open, &QPushButton::clicked, this, &DiscPane::chooseOpen);
    connect(save_, &QPushButton::clicked, this, &DiscPane::saveDisc);
    connect(saveAs_, &QPushButton::clicked, this, &DiscPane::chooseSaveAs);
    connect(insertA_, &QPushButton::clicked, this, [this] { insertInDrive(0); });
    connect(insertB_, &QPushButton::clicked, this, [this] { insertInDrive(1); });
    connect(run_, &QPushButton::clicked, this, &DiscPane::runSelected);

    auto* files = new QGridLayout;
    files->addWidget(add_, 0, 0);
    files->addWidget(export_, 0, 1);
    files->addWidget(rename_, 0, 2);
    files->addWidget(delete_, 0, 3);
    files->addWidget(info_, 1, 0);
    files->addWidget(view_, 1, 1);
    files->addWidget(sectors_, 1, 2);
    files->addWidget(importDisc_, 1, 3);
    auto* shown = new QHBoxLayout;
    shown->addWidget(showDirectory_);
    shown->addWidget(hexadecimal_);
    shown->addStretch(1);
    auto* disc = new QGridLayout;
    disc->addWidget(create, 0, 0);
    disc->addWidget(open, 0, 1);
    disc->addWidget(save_, 0, 2);
    disc->addWidget(saveAs_, 0, 3);
    disc->addWidget(insertA_, 1, 0);
    disc->addWidget(insertB_, 1, 1);
    disc->addWidget(run_, 1, 2);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(title_);
    layout->addWidget(list_, 1);
    layout->addLayout(files);
    layout->addLayout(shown);
    layout->addLayout(disc);
    refresh();
}

QString DiscPane::name() const
{
    if (!disc_)
        return QString();
    return path_.isEmpty() ? tr("New disc") : QFileInfo(path_).fileName();
}

// ---- The disc --------------------------------------------------------------------

void DiscPane::setDisc(tuxdsk::CpcDisc disc, const QString& path, bool modified)
{
    disc_ = std::move(disc);
    path_ = path;
    modified_ = modified;
    refresh();
    emit changed();
}

// The disc was changed: it is to be saved, and shown again.
void DiscPane::touched(const QString& said)
{
    modified_ = true;
    refresh();
    emit changed();
    emit status(said);
}

void DiscPane::newDisc(int tracks, tuxdsk::DiscFormat format)
{
    setDisc(tuxdsk::CpcDisc::formatted(tracks, format), QString(), false);
    emit status(tr("An empty disc of %1 tracks, in the %2 format.").arg(tracks).arg(QLatin1String(tuxdsk::formatName(format))));
}

bool DiscPane::openDisc(const QString& path)
{
    const auto file = readHostFile(path);
    if (!file) {
        prompts::problem(this, tr("Unable to read the file %1").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    // A .DSK file or an .HFE one: what the file starts with says.
    tuxdsk::ImageError reason = tuxdsk::ImageError::None;
    std::optional<tuxdsk::DiscImage> image = tuxdsk::parseDisc(*file, &reason);
    if (!image) {
        prompts::problem(this, notADisc(path, reason));
        return false;
    }
    // A disc whose sectors follow one another is slow on a real drive:
    // ManageDsk offers to put them in a CPC's order.
    bool interleaved = false;
    if (!image->interleaved()
        && prompts::ask(this, {tr("TuxDSK – Read DSK"), tr("Sectors not interlaced.\nForce interlacing?"), false, QString()})
               == Answer::Yes) {
        image->interleave();
        interleaved = true;
    }
    setDisc(tuxdsk::CpcDisc(std::move(*image)), QFileInfo(path).absoluteFilePath(), interleaved);
    if (preferences_)
        preferences_->setDiscFolder(QFileInfo(path).absolutePath());
    if (!disc_->hasDirectory())
        prompts::problem(this, tr("File is not recognized as standard DSK (DATA or VENDOR): its files cannot be shown, its "
                                  "sectors can."));
    emit status(tr("%1 read.").arg(name()));
    return true;
}

bool DiscPane::saveDiscAs(const QString& path)
{
    if (!disc_)
        return false;
    // The kind of file is the one its name says: an .HFE file is the disc
    // as a drive reads it, for a Gotek or an HxC drive; any other name is a
    // .DSK file.
    const bool hfe = namesHfe(path);
    if (!writeHostFile(path, hfe ? tuxdsk::hfeFile(disc_->image()) : disc_->image().bytes())) {
        prompts::problem(this, tr("%1 cannot be written.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    path_ = QFileInfo(path).absoluteFilePath();
    modified_ = false;
    if (preferences_)
        preferences_->setDiscFolder(QFileInfo(path).absolutePath());
    refresh();
    emit changed();
    emit status(hfe ? tr("%1 saved as an HFE file, for a Gotek or an HxC drive on a real CPC").arg(name())
                    : tr("%1 saved").arg(name()));
    return true;
}

bool DiscPane::saveDisc()
{
    if (!disc_)
        return false;
    if (path_.isEmpty()) {
        chooseSaveAs();
        return !path_.isEmpty() && !modified_;
    }
    return saveDiscAs(path_);
}

bool DiscPane::mayLeave()
{
    if (!disc_ || !modified_)
        return true;
    return prompts::ask(this, {tr("TuxDSK"), tr("Floppy %1 not saved. Confirm?").arg(name()), false, QString()})
           == Answer::Yes;
}

void DiscPane::exchange(DiscPane* other)
{
    std::swap(disc_, other->disc_);
    std::swap(path_, other->path_);
    std::swap(modified_, other->modified_);
    for (DiscPane* pane : {this, other}) {
        pane->refresh();
        emit pane->changed();
    }
}

// ---- Its files ---------------------------------------------------------------------

std::vector<tuxdsk::DiscFile> DiscPane::selectedFiles() const
{
    std::vector<tuxdsk::DiscFile> chosen;
    if (showDirectory_->isChecked())
        return chosen;
    for (int row = 0; row < list_->topLevelItemCount(); ++row) {
        const QTreeWidgetItem* item = list_->topLevelItem(row);
        const int index = item->data(0, kFileRole).toInt();
        if (item->isSelected() && index >= 0 && index < static_cast<int>(files_.size()))
            chosen.push_back(files_[static_cast<size_t>(index)]);
    }
    return chosen;
}

void DiscPane::select(const QStringList& names)
{
    list_->clearSelection();
    if (showDirectory_->isChecked())
        return;
    for (int row = 0; row < list_->topLevelItemCount(); ++row) {
        QTreeWidgetItem* item = list_->topLevelItem(row);
        const int index = item->data(0, kFileRole).toInt();
        // The files that are there: one that was erased is chosen in the
        // list itself.
        if (index >= 0 && index < static_cast<int>(files_.size()) && !files_[static_cast<size_t>(index)].erased
            && names.contains(item->text(Name), Qt::CaseInsensitive))
            item->setSelected(true);
    }
}

// Says why a file did not go on the disc; true if it did.
bool DiscPane::tell(tuxdsk::AddResult result, const QString& name)
{
    switch (result) {
    case tuxdsk::AddResult::Done:
        return true;
    case tuxdsk::AddResult::BadName:
        prompts::problem(this, tr("%1: no name of a CPC's directory can be made of this one.").arg(name));
        break;
    case tuxdsk::AddResult::Exists:
        prompts::problem(this, tr("File %1 already exists.").arg(name));
        break;
    case tuxdsk::AddResult::NoEntry:
        prompts::problem(this, tr("No more entry in the directory!"));
        break;
    case tuxdsk::AddResult::NoSpace:
        prompts::problem(this, tr("No more space on the floppy!"));
        break;
    case tuxdsk::AddResult::NoDisc:
        prompts::problem(this, tr("This disc is not in the DATA, VENDOR or PCW format: files cannot be put on it."));
        break;
    }
    return false;
}

tuxdsk::AddResult DiscPane::addFile(const QString& path, const tuxdsk::ImportOptions& options, bool replace)
{
    if (!disc_)
        return tuxdsk::AddResult::NoDisc;
    const auto file = readHostFile(path);
    if (!file) {
        prompts::problem(this, tr("Unable to read the file %1").arg(QDir::toNativeSeparators(path)));
        return tuxdsk::AddResult::BadName;
    }
    const std::string name = tuxdsk::cpcName(QFileInfo(path).fileName().toStdString());
    const std::vector<uint8_t> content = tuxdsk::importedFile(name, *file, options);
    tuxdsk::AddResult result;
    const std::optional<tuxdsk::DiscFile> there = name.empty() ? std::nullopt : disc_->find(name);
    if (there && replace) {
        result = disc_->replace(*there, content, options_->blocks());
    } else {
        tuxdsk::AddOptions where;
        where.blocks = options_->blocks();
        result = disc_->add(name, content, where);
    }
    if (result == tuxdsk::AddResult::Done)
        touched(tr("%1 added (%Ln bytes).", nullptr, static_cast<int>(content.size())).arg(QString::fromStdString(name)));
    return result;
}

bool DiscPane::addBytes(const QString& name, const std::vector<uint8_t>& content, bool replace)
{
    if (!disc_)
        return false;
    const std::string clean = tuxdsk::cpcName(name.toStdString());
    tuxdsk::AddResult result;
    const std::optional<tuxdsk::DiscFile> there = clean.empty() ? std::nullopt : disc_->find(clean);
    if (there && replace) {
        result = disc_->replace(*there, content, options_->blocks());
    } else {
        tuxdsk::AddOptions where;
        where.blocks = options_->blocks();
        result = disc_->add(clean, content, where);
    }
    if (!tell(result, name))
        return false;
    touched(tr("%1 added (%Ln bytes).", nullptr, static_cast<int>(content.size())).arg(QString::fromStdString(clean)));
    select({QString::fromStdString(clean)});
    return true;
}

void DiscPane::addFiles(const QStringList& paths)
{
    if (!disc_) {
        prompts::problem(this, tr("No DSK loaded."));
        return;
    }
    if (preferences_ && !paths.isEmpty())
        preferences_->setFileFolder(QFileInfo(paths.first()).absolutePath());
    bool replaceAll = false;
    int added = 0;
    for (qsizetype at = 0; at < paths.size(); ++at) {
        const QString& path = paths[at];
        const QString shown = QFileInfo(path).fileName();
        const auto file = readHostFile(path);
        if (!file) {
            prompts::problem(this, tr("Unable to read the file %1").arg(QDir::toNativeSeparators(path)));
            continue;
        }
        const QString cpcName = QString::fromStdString(tuxdsk::cpcName(shown.toStdString()));

        tuxdsk::ImportOptions options;
        switch (options_->importMode) {
        case ManagerOptions::ForceAscii:
            options.kind = tuxdsk::ImportKind::Ascii;
            break;
        case ManagerOptions::ForceBinary:
            options.kind = tuxdsk::ImportKind::Binary;
            break;
        default: {
            ImportDialog dialog(shown, cpcName, *file, this);
            if (dialog.exec() != QDialog::Accepted) {
                emit status(tr("%n file(s) added.", nullptr, added));
                return;
            }
            options = dialog.options();
            break;
        }
        }
        options.cleanHeader = options_->cleanHeaders;
        // A header counts a file's bytes on 16 bits.
        if (options.kind == tuxdsk::ImportKind::Binary && !tuxdsk::hasAmsdosHeader(*file) && file->size() >= 0x10000
            && prompts::ask(this, {tr("TuxDSK – Import File %1").arg(shown), tr("Size greater than 64Kb. Continue?"), false, QString()})
                   != Answer::Yes)
            continue;

        tuxdsk::AddResult result = addFile(path, options, replaceAll);
        if (result == tuxdsk::AddResult::Exists) {
            const Answer answer = prompts::ask(
                this, {tr("TuxDSK – Import File %1").arg(shown), tr("File %1 already exists. Replace?").arg(cpcName),
                       at + 1 < paths.size(), QString()});
            if (answer == Answer::Cancel)
                break;
            if (answer == Answer::No)
                continue;
            replaceAll = answer == Answer::YesToAll;
            result = addFile(path, options, true);
        }
        // A disc that is full is full for the files that follow too.
        if (!tell(result, shown) && result != tuxdsk::AddResult::BadName)
            break;
        added += result == tuxdsk::AddResult::Done;
    }
    emit status(tr("%n file(s) added.", nullptr, added));
}

tuxdsk::AddResult DiscPane::putFile(const tuxdsk::DiscFile& file, const std::vector<uint8_t>& records, bool replace)
{
    if (!disc_)
        return tuxdsk::AddResult::NoDisc;
    tuxdsk::AddOptions where;
    // A file that was erased comes back as a file of user 0.
    where.user = file.erased ? 0 : file.user;
    where.readOnly = file.readOnly;
    where.hidden = file.hidden;
    where.blocks = options_->blocks();
    tuxdsk::AddResult result;
    const std::optional<tuxdsk::DiscFile> there = disc_->find(file.name, where.user);
    if (there && replace) {
        result = disc_->replace(*there, records, where.blocks);
        if (result == tuxdsk::AddResult::Done)
            if (const auto now = disc_->find(file.name, where.user))
                disc_->setAttributes(*now, file.readOnly, file.hidden);
    } else {
        result = disc_->add(file.name, records, where);
    }
    if (result == tuxdsk::AddResult::Done)
        touched(tr("%1 copied.").arg(QString::fromStdString(file.name)));
    return result;
}

int DiscPane::putFiles(const tuxdsk::CpcDisc& from, const std::vector<tuxdsk::DiscFile>& files)
{
    if (!disc_) {
        prompts::problem(this, tr("No DSK loaded."));
        return 0;
    }
    int put = 0;
    bool replaceAll = false;
    for (size_t at = 0; at < files.size(); ++at) {
        const tuxdsk::DiscFile& file = files[at];
        const QString name = QString::fromStdString(file.name);
        const std::vector<uint8_t> records = from.read(file);
        tuxdsk::AddResult result = putFile(file, records, replaceAll);
        // ManageDsk does not put a file twice on a disc: it asks.
        if (result == tuxdsk::AddResult::Exists) {
            const Answer answer =
                prompts::ask(this, {tr("TuxDSK – Copy"), tr("File %1 already exists. Replace?").arg(name), at + 1 < files.size(), QString()});
            if (answer == Answer::Cancel)
                break;
            if (answer == Answer::No)
                continue;
            replaceAll = answer == Answer::YesToAll;
            result = putFile(file, records, true);
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
        ++put;
    }
    return put;
}

int DiscPane::importDisc(const QString& path)
{
    if (!disc_) {
        prompts::problem(this, tr("No DSK loaded."));
        return 0;
    }
    const auto file = readHostFile(path);
    if (!file) {
        prompts::problem(this, tr("Unable to read the file %1").arg(QDir::toNativeSeparators(path)));
        return 0;
    }
    tuxdsk::ImageError reason = tuxdsk::ImageError::None;
    std::optional<tuxdsk::DiscImage> image = tuxdsk::parseDisc(*file, &reason);
    if (!image) {
        prompts::problem(this, notADisc(path, reason));
        return 0;
    }
    if (preferences_)
        preferences_->setDiscFolder(QFileInfo(path).absolutePath());
    // The other disc is only read: the order of its sectors does not matter.
    const tuxdsk::CpcDisc other(std::move(*image));
    const QString otherName = QFileInfo(path).fileName();
    if (!other.hasDirectory()) {
        prompts::problem(this, tr("%1 is not recognized as standard DSK (DATA or VENDOR): it has no files to import.").arg(otherName));
        return 0;
    }
    const std::vector<tuxdsk::DiscFile> files = other.files();
    if (files.empty()) {
        prompts::problem(this, tr("There is no file on %1.").arg(otherName));
        return 0;
    }
    DiscFilesDialog dialog(otherName, files, this);
    if (dialog.exec() != QDialog::Accepted)
        return 0;
    const int imported = putFiles(other, dialog.chosen());
    emit status(tr("%n file(s) imported from %1.", nullptr, imported).arg(otherName));
    return imported;
}

bool DiscPane::exportFile(const tuxdsk::DiscFile& file, const QString& path)
{
    if (!disc_)
        return false;
    const std::vector<uint8_t> bytes =
        tuxdsk::exportedFile(disc_->read(file), options_->exportHeader, options_->cleanHeaders);
    if (!writeHostFile(path, bytes)) {
        prompts::problem(this, tr("%1 cannot be written.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    emit status(tr("%1 exported (%Ln bytes).", nullptr, static_cast<int>(bytes.size())).arg(QString::fromStdString(file.name)));
    return true;
}

int DiscPane::exportFiles(const std::vector<tuxdsk::DiscFile>& files, const QString& folder)
{
    int written = 0;
    bool replaceAll = false;
    for (size_t at = 0; at < files.size(); ++at) {
        const tuxdsk::DiscFile& file = files[at];
        // A name the computer's folders may not like is the file's all the
        // same: '?' stands for what cannot be printed, and '/' is not one
        // of a directory's characters here.
        QString name = QString::fromStdString(file.name);
        name.replace('/', '_');
        const QString path = QDir(folder).filePath(name);
        if (QFileInfo::exists(path) && !replaceAll) {
            const Answer answer = prompts::ask(this, {tr("TuxDSK – Export File(s)"), tr("File %1 already exists. Replace?").arg(name),
                                                      at + 1 < files.size(), QString()});
            if (answer == Answer::Cancel)
                break;
            if (answer == Answer::No)
                continue;
            replaceAll = answer == Answer::YesToAll;
        }
        if (!exportFile(file, path))
            break;
        ++written;
    }
    emit status(tr("%n file(s) exported to %1.", nullptr, written).arg(shownPath(folder)));
    return written;
}

bool DiscPane::renameFile(const tuxdsk::DiscFile& file, const QString& name, int user)
{
    if (!disc_)
        return false;
    const tuxdsk::AddResult result = disc_->rename(file, name.toStdString(), user);
    if (!tell(result, name))
        return false;
    const QString now = QString::fromStdString(tuxdsk::cpcName(name.toStdString()));
    if (user == tuxdsk::kErasedUser && !file.erased)
        touched(tr("%1 erased.").arg(now));
    else if (file.erased && user != tuxdsk::kErasedUser)
        touched(tr("%1 brought back, as a file of user %2.").arg(now).arg(user));
    else
        touched(tr("%1 is now %2, user %3.").arg(QString::fromStdString(file.name), now).arg(user));
    return true;
}

void DiscPane::eraseFile(const tuxdsk::DiscFile& file, bool forGood)
{
    if (!disc_)
        return;
    // A file already erased can only be erased for good.
    disc_->erase(file, forGood || file.erased);
    touched(forGood || file.erased ? tr("%1 permanently deleted.").arg(QString::fromStdString(file.name))
                                   : tr("%1 deleted.").arg(QString::fromStdString(file.name)));
}

bool DiscPane::changeFile(const tuxdsk::DiscFile& file, tuxdsk::FileType type, int loadAddress, int entryAddress,
                          bool readOnly, bool hidden)
{
    if (!disc_ || file.erased)
        return false;
    const bool wantsHeader = type != tuxdsk::FileType::Ascii;
    const bool sameHeader = type == file.type
                            && (!wantsHeader || (loadAddress == file.loadAddress && entryAddress == file.entryAddress));
    if (!sameHeader) {
        std::vector<uint8_t> content = disc_->read(file);
        if (file.hasHeader() && wantsHeader) {
            // A type AMSDOS does not know, left as it is, stays in the header.
            const uint8_t byte = type == tuxdsk::FileType::Unknown ? content[18] : typeByte(type);
            tuxdsk::setAmsdosHeader(content, byte, static_cast<uint16_t>(loadAddress), static_cast<uint16_t>(entryAddress));
        } else if (file.hasHeader()) {
            content = tuxdsk::exportedFile(content, false);
        } else {
            std::vector<uint8_t> headed =
                tuxdsk::makeAmsdosHeader(file.name, typeByte(type), static_cast<uint16_t>(loadAddress),
                                         static_cast<uint16_t>(entryAddress), content.size());
            headed.insert(headed.end(), content.begin(), content.end());
            content = std::move(headed);
        }
        if (!tell(disc_->replace(file, content, options_->blocks()), QString::fromStdString(file.name)))
            return false;
    }
    if (const auto now = disc_->find(file.name, file.user))
        if (now->readOnly != readOnly || now->hidden != hidden)
            disc_->setAttributes(*now, readOnly, hidden);
    touched(tr("%1 changed.").arg(QString::fromStdString(file.name)));
    return true;
}

ViewerDialog* DiscPane::viewFile(const tuxdsk::DiscFile& file)
{
    if (!disc_)
        return nullptr;
    // A screen or a window is seen with the palette that goes with it:
    // the .PAL of its name, as OCP Art Studio saves one.
    std::optional<tuxdsk::ScreenLook> look;
    const size_t dot = file.name.rfind('.');
    if (dot != std::string::npos && (file.name.substr(dot) == ".SCR" || file.name.substr(dot) == ".WIN"))
        if (const auto palette = disc_->find(file.name.substr(0, dot) + ".PAL", file.user))
            look = tuxdsk::lookOfPalette(tuxdsk::exportedFile(disc_->read(*palette), false));
    auto* viewer = new ViewerDialog(file, disc_->read(file), look, preferences_, this);
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->show();
    return viewer;
}

SectorDialog* DiscPane::showSectors()
{
    if (!disc_)
        return nullptr;
    auto* dialog = new SectorDialog(*disc_, name(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    return dialog;
}

bool DiscPane::insertInDrive(int drive)
{
    if (!disc_ || !host_)
        return false;
    // TuxAPE takes a disc from its file: what is in the list must be there.
    if ((path_.isEmpty() || modified_)
        && (prompts::ask(this, {tr("TuxDSK"), tr("The disc is saved before it goes in the drive. Save it now?"), false, QString()})
                != Answer::Yes
            || !saveDisc()))
        return false;
    const QChar letter = QLatin1Char(drive == 0 ? 'A' : 'B');
    if (!host_->insertDisc(drive, path_)) {
        emit status(tr("TuxAPE did not take %1 in drive %2:.").arg(name()).arg(letter));
        return false;
    }
    emit status(tr("%1 is in drive %2: of TuxAPE.").arg(name()).arg(letter));
    return true;
}

// ---- What the buttons do -----------------------------------------------------------

QString DiscPane::startFolder(bool discs) const
{
    if (discs && !path_.isEmpty())
        return QFileInfo(path_).absolutePath();
    if (!preferences_)
        return QString();
    return discs ? preferences_->discFolder() : preferences_->fileFolder();
}

void DiscPane::chooseNew()
{
    if (!mayLeave())
        return;
    NewDiscDialog dialog(options_->moreTracks, this);
    if (dialog.exec() == QDialog::Accepted)
        newDisc(dialog.tracks(), dialog.format());
}

void DiscPane::chooseOpen()
{
    if (!mayLeave())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Read DSK"), startFolder(true), discFilters());
    if (!path.isEmpty())
        openDisc(path);
}

void DiscPane::chooseSaveAs()
{
    if (!disc_)
        return;
    const QString suggested = path_.isEmpty() ? QDir(startFolder(true)).filePath(QStringLiteral("disc.dsk")) : path_;
    // The two kinds of file are offered; the one the disc is in comes first.
    const QStringList kinds = namesHfe(path_) ? QStringList{hfeFilter(), dskFilter()} : QStringList{dskFilter(), hfeFilter()};
    QString kind;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save DSK"), suggested, kinds.join(QStringLiteral(";;")), &kind);
    if (!path.isEmpty())
        saveDiscAs(savedPath(path, kind == hfeFilter()));
}

// The name a disc is saved under, from the one typed and the kind of file
// chosen: a name without a kind of its own is given that one's.
QString DiscPane::savedPath(const QString& typed, bool hfe)
{
    const QString suffix = QFileInfo(typed).suffix().toLower();
    if (suffix == QLatin1String("dsk") || suffix == QLatin1String("hfe"))
        return typed;
    return typed + (hfe ? QStringLiteral(".hfe") : QStringLiteral(".dsk"));
}

void DiscPane::chooseAdd()
{
    if (!disc_) {
        prompts::problem(this, tr("No DSK loaded."));
        return;
    }
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Add File(s)"), startFolder(false), tr("All Files (*)"));
    if (!paths.isEmpty())
        addFiles(paths);
}

void DiscPane::chooseImportDisc()
{
    if (!disc_) {
        prompts::problem(this, tr("No DSK loaded."));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Import from DSK"), startFolder(true), discFilters());
    if (!path.isEmpty())
        importDisc(path);
}

void DiscPane::chooseExport()
{
    const std::vector<tuxdsk::DiscFile> chosen = selectedFiles();
    if (chosen.empty())
        return;
    // One file can be given another name on its way out; several go to a
    // folder under their own.
    if (chosen.size() == 1) {
        const QString suggested = QDir(startFolder(false)).filePath(QString::fromStdString(chosen[0].name));
        const QString path = QFileDialog::getSaveFileName(this, tr("Export File(s)"), suggested, tr("All Files (*)"));
        if (path.isEmpty())
            return;
        if (preferences_)
            preferences_->setFileFolder(QFileInfo(path).absolutePath());
        exportFile(chosen[0], path);
        return;
    }
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Export File(s)"), startFolder(false));
    if (folder.isEmpty())
        return;
    if (preferences_)
        preferences_->setFileFolder(folder);
    exportFiles(chosen, folder);
}

void DiscPane::renameSelected()
{
    for (const tuxdsk::DiscFile& file : selectedFiles()) {
        // The file as it is now: one renamed before it may have changed it.
        const auto now = disc_->find(file.name, file.user);
        if (!now)
            continue;
        RenameDialog dialog(*now, this);
        if (dialog.exec() != QDialog::Accepted)
            break;
        renameFile(*now, dialog.name(), dialog.user());
    }
}

void DiscPane::deleteSelected()
{
    const std::vector<tuxdsk::DiscFile> chosen = selectedFiles();
    bool all = false;
    bool forGood = false;
    for (size_t at = 0; at < chosen.size(); ++at) {
        const tuxdsk::DiscFile& file = chosen[at];
        if (!all) {
            const Answer answer =
                prompts::ask(this,
                             {tr("TuxDSK – Delete File(s)"),
                              tr("Do you really want to delete this file?\n%1").arg(QString::fromStdString(file.name)),
                              at + 1 < chosen.size(), tr("Permanently delete")},
                             &forGood);
            if (answer == Answer::Cancel)
                break;
            if (answer == Answer::No)
                continue;
            all = answer == Answer::YesToAll;
        }
        if (const auto now = disc_->find(file.name, file.user))
            eraseFile(*now, forGood);
    }
}

void DiscPane::showInformation()
{
    const std::vector<tuxdsk::DiscFile> chosen = selectedFiles();
    if (chosen.empty())
        return;
    const tuxdsk::DiscFile& file = chosen.front();
    FileInfoDialog dialog(file, this);
    // What was erased is shown, and is not to be changed where it lies.
    if (file.erased)
        if (QPushButton* validate = dialog.findChild<QPushButton*>("bValidate"))
            validate->setEnabled(false);
    if (dialog.exec() == QDialog::Accepted)
        changeFile(file, dialog.type(), dialog.loadAddress(), dialog.entryAddress(), dialog.readOnly(), dialog.hidden());
}

void DiscPane::viewSelected()
{
    for (const tuxdsk::DiscFile& file : selectedFiles())
        viewFile(file);
}

void DiscPane::runSelected()
{
    const std::vector<tuxdsk::DiscFile> chosen = selectedFiles();
    if (chosen.empty() || chosen.front().erased || !insertInDrive(0))
        return;
    // In WinAPE's Auto-Type syntax, as TuxAPE takes it.
    host_->autoType(QStringLiteral("run\"%1~RETURN~").arg(QString::fromStdString(chosen.front().name)));
}

void DiscPane::showMenu(const QPoint& where)
{
    if (selectedFiles().empty())
        return;
    QMenu menu(this);
    const auto entry = [&menu, this](const QString& text, QPushButton* button, void (DiscPane::*what)()) {
        QAction* action = menu.addAction(text, this, what);
        action->setEnabled(button->isEnabled());
    };
    entry(tr("&Information..."), info_, &DiscPane::showInformation);
    entry(tr("Show &File"), view_, &DiscPane::viewSelected);
    menu.addSeparator();
    entry(tr("&Export File(s)..."), export_, &DiscPane::chooseExport);
    entry(tr("&Rename File(s)..."), rename_, &DiscPane::renameSelected);
    entry(tr("&Delete File(s)..."), delete_, &DiscPane::deleteSelected);
    if (host_) {
        menu.addSeparator();
        entry(tr("Run in TuxAPE"), run_, &DiscPane::runSelected);
    }
    menu.exec(list_->viewport()->mapToGlobal(where));
}

// ---- What is shown -----------------------------------------------------------------

QString DiscPane::number(int value, int digits) const
{
    if (hexadecimal_->isChecked())
        return QStringLiteral("&%1").arg(value, digits, 16, QLatin1Char('0')).toUpper();
    return QString::number(value);
}

void DiscPane::refresh()
{
    // What was chosen stays chosen: a file is known by its user and its name.
    const auto key = [](const tuxdsk::DiscFile& file) {
        return QString::number(file.user) + QLatin1Char(':') + QString::fromStdString(file.name);
    };
    QStringList chosen;
    for (const tuxdsk::DiscFile& file : selectedFiles())
        chosen << key(file);

    if (!disc_) {
        title_->setText(tr("No DSK loaded: Create DSK makes an empty disc, Read DSK reads one."));
    } else {
        const tuxdsk::DiscImage& image = disc_->image();
        // The disc's name, then what it is.
        const bool twoSides = image.sides() > 1;
        QString what;
        if (!disc_->hasDirectory())
            what = (twoSides ? tr("not a DATA, VENDOR or PCW disc, %1 tracks, two sides (the files are those of the first)")
                             : tr("not a DATA, VENDOR or PCW disc, %1 tracks"))
                       .arg(image.tracks());
        else
            what = (twoSides ? tr("Inside DSK file (used Kb/free Kb: %1/%2) - %3 format, %4 tracks, two sides (the files "
                                  "are those of the first)")
                             : tr("Inside DSK file (used Kb/free Kb: %1/%2) - %3 format, %4 tracks"))
                       .arg(QString::number(disc_->usedBlocks()), QString::number(disc_->freeBlocks()),
                            QLatin1String(tuxdsk::formatName(disc_->format())), QString::number(image.tracks()));
        title_->setText(name() + (modified_ ? QStringLiteral("*") : QString()) + QStringLiteral(" - ") + what);
        title_->setToolTip(path_.isEmpty() ? QString() : QDir::toNativeSeparators(path_));
    }

    if (showDirectory_->isChecked())
        listDirectory();
    else
        listFiles();
    for (int column = 0; column < list_->columnCount(); ++column)
        list_->resizeColumnToContents(column);
    for (int row = 0; row < list_->topLevelItemCount() && !files_.empty(); ++row) {
        QTreeWidgetItem* item = list_->topLevelItem(row);
        if (chosen.contains(key(files_[static_cast<size_t>(item->data(0, kFileRole).toInt())])))
            item->setSelected(true);
    }
    updateButtons();
}

void DiscPane::listFiles()
{
    list_->clear();
    list_->setColumnCount(Columns);
    list_->setHeaderLabels({tr("File Name"), tr("User"), tr("Read only"), tr("Hidden"), tr("Type"), tr("Size"), tr("Start"),
                            tr("Entry")});
    files_ = disc_ ? disc_->files(true) : std::vector<tuxdsk::DiscFile>();
    const QColor red = erasedColour(palette());
    for (size_t index = 0; index < files_.size(); ++index) {
        const tuxdsk::DiscFile& file = files_[index];
        auto* item = new QTreeWidgetItem(list_);
        item->setData(0, kFileRole, static_cast<int>(index));
        item->setText(Name, QString::fromStdString(file.name));
        item->setText(User, QString::number(file.user));
        item->setText(ReadOnly, file.readOnly ? tr("Yes") : QString());
        item->setText(Hidden, file.hidden ? tr("Yes") : QString());
        item->setText(Type, QString::fromLatin1(tuxdsk::typeName(file.type)));
        // In kilobytes, as ManageDsk and the CPC's CAT count; in bytes, as
        // the header says, when the figures are hexadecimal.
        item->setText(Size, hexadecimal_->isChecked() ? number(file.length) : tr("%1 Kb").arg(file.kilobytes()));
        if (file.hasHeader()) {
            item->setText(Start, QStringLiteral("&%1").arg(file.loadAddress, 4, 16, QLatin1Char('0')).toUpper());
            item->setText(Entry, QStringLiteral("&%1").arg(file.entryAddress, 4, 16, QLatin1Char('0')).toUpper());
        }
        for (const int column : {User, Size})
            item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
        if (file.erased) {
            for (int column = 0; column < Columns; ++column)
                item->setForeground(column, red);
            item->setToolTip(Name, tr("Erased: Rename brings it back, with another user than 229"));
        }
    }
}

void DiscPane::listDirectory()
{
    list_->clear();
    files_.clear();
    list_->setColumnCount(6);
    list_->setHeaderLabels({tr("Entry"), tr("File Name"), tr("User"), tr("Page"), tr("Records"), tr("Blocks")});
    if (!disc_ || !disc_->hasDirectory())
        return;
    const QColor red = erasedColour(palette());
    const auto entries = disc_->directory();
    for (int index = 0; index < tuxdsk::kDirectoryEntries; ++index) {
        const tuxdsk::DirEntry& entry = entries[static_cast<size_t>(index)];
        auto* item = new QTreeWidgetItem(list_);
        item->setData(0, kFileRole, -1);
        item->setText(0, number(index, 2));
        item->setText(1, QString::fromStdString(tuxdsk::shownName(entry.name)));
        item->setText(2, number(entry.user, 2));
        item->setText(3, number(entry.extent, 2));
        item->setText(4, number(entry.records, 2));
        QStringList blocks;
        for (const uint8_t block : entry.blocks)
            if (block)
                blocks << number(block, 2);
        item->setText(5, blocks.join(' '));
        for (const int column : {0, 2, 3, 4})
            item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
        if (entry.free())
            for (int column = 0; column < 6; ++column)
                item->setForeground(column, red);
    }
}

void DiscPane::updateButtons()
{
    const bool disc = disc_.has_value();
    const bool files = disc && disc_->hasDirectory();
    const std::vector<tuxdsk::DiscFile> chosen = selectedFiles();
    const bool some = !chosen.empty();
    save_->setEnabled(disc);
    saveAs_->setEnabled(disc);
    add_->setEnabled(files);
    importDisc_->setEnabled(files);
    export_->setEnabled(some);
    rename_->setEnabled(some);
    delete_->setEnabled(some);
    info_->setEnabled(some);
    view_->setEnabled(some);
    sectors_->setEnabled(disc);
    showDirectory_->setEnabled(files);
    // TuxAPE's drives, when there is a TuxAPE.
    insertA_->setEnabled(disc && host_);
    insertB_->setEnabled(disc && host_);
    run_->setEnabled(some && host_ && !chosen.front().erased);
}

// ---- Files and discs dropped on the list -------------------------------------------

void DiscPane::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void DiscPane::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls())
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile())
            paths << url.toLocalFile();
    if (paths.isEmpty())
        return;
    event->acceptProposedAction();
    // A disc image is read; anything else is added to the disc.
    if (paths.size() == 1
        && (QFileInfo(paths.first()).suffix().compare(QLatin1String("dsk"), Qt::CaseInsensitive) == 0 || namesHfe(paths.first()))) {
        if (mayLeave())
            openDisc(paths.first());
        return;
    }
    addFiles(paths);
}

void DiscPane::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    // The look has changed: the red of erased files is the new look's.
    if (event->type() == QEvent::PaletteChange && run_)
        refresh();
}

}  // namespace tuxdsk
