#include "dialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/version.h"
#include "controls.h"
#include "disc/amsdos.h"

namespace tuxdsk {

namespace {

// What the windows of this file share says, translated under its own name.
struct Texts {
    Q_DECLARE_TR_FUNCTIONS(tuxdsk::dialogs)
};

QString bytesText(qint64 bytes)
{
    return Texts::tr("%Ln byte(s)", nullptr, static_cast<int>(bytes));
}

// ManageDsk's two buttons: Validate and Cancel.
QDialogButtonBox* validateOrCancel(QDialog* dialog)
{
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(Texts::tr("Validate"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName("bValidate");
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

// Where a file's name on a disc is typed: eight characters, a dot and
// three, of those a directory can hold.
QLineEdit* nameEdit(const QString& name)
{
    auto* edit = new QLineEdit(name);
    edit->setObjectName("edName");
    edit->setFont(fixedFont());
    edit->setMaxLength(12);
    static const QRegularExpression shape(QStringLiteral("[^\\s<>.,;:=?*\\[\\]]{1,8}(\\.[^\\s<>.,;:=?*\\[\\]]{0,3})?"));
    edit->setValidator(new QRegularExpressionValidator(shape, edit));
    return edit;
}

QLabel* fixedLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setFont(fixedFont());
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

}  // namespace

// ---- ImportDialog --------------------------------------------------------------

ImportDialog::ImportDialog(const QString& name, const QString& cpcName, std::span<const uint8_t> file, QWidget* parent)
    : QDialog(parent)
{
    setObjectName("ImportDialog");
    setWindowTitle(tr("TuxDSK – Import File %1").arg(name));
    const std::optional<tuxdsk::AmsdosHeader> header = tuxdsk::amsdosHeader(file);

    auto* found = new QLabel(header ? tr("AMSDOS header found.") : tr("AMSDOS header not found."));
    found->setObjectName("lHeader");
    ascii_ = new QRadioButton(tr("&ASCII file"));
    ascii_->setObjectName("rbAscii");
    binary_ = new QRadioButton(tr("&BINARY file"));
    binary_->setObjectName("rbBinary");
    // A file is taken for what it looks like, as ManageDsk takes it.
    (header ? binary_ : ascii_)->setChecked(true);
    load_ = new AddressBox;
    load_->setObjectName("sbStart");
    load_->setValue(header ? header->loadAddress : 0);
    entry_ = new AddressBox;
    entry_->setObjectName("sbEntry");
    entry_->setValue(header ? header->entryAddress : 0);
    const auto follow = [this] {
        load_->setEnabled(binary_->isChecked());
        entry_->setEnabled(binary_->isChecked());
    };
    connect(binary_, &QRadioButton::toggled, this, follow);
    follow();

    auto* form = new QFormLayout;
    form->addRow(tr("Name on the disc:"), fixedLabel(cpcName));
    form->addRow(tr("Size:"), fixedLabel(bytesText(file.size())));
    form->addRow(found);
    form->addRow(tr("Choose file type:"), ascii_);
    form->addRow(QString(), binary_);
    form->addRow(tr("&Start address:"), load_);
    form->addRow(tr("&Entry point:"), entry_);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(validateOrCancel(this));
}

tuxdsk::ImportOptions ImportDialog::options() const
{
    tuxdsk::ImportOptions options;
    options.kind = ascii_->isChecked() ? tuxdsk::ImportKind::Ascii : tuxdsk::ImportKind::Binary;
    options.setAddresses = true;
    options.loadAddress = load_->value();
    options.entryAddress = entry_->value();
    return options;
}

// ---- FileInfoDialog ------------------------------------------------------------

FileInfoDialog::FileInfoDialog(const tuxdsk::DiscFile& file, QWidget* parent)
    : QDialog(parent)
{
    setObjectName("FileInfoDialog");
    setWindowTitle(tr("TuxDSK – Information about a File"));

    type_ = new QComboBox;
    type_->setObjectName("cbType");
    for (const tuxdsk::FileType type : {tuxdsk::FileType::Basic, tuxdsk::FileType::ProtectedBasic, tuxdsk::FileType::Binary,
                                        tuxdsk::FileType::ProtectedBinary, tuxdsk::FileType::Ascii})
        type_->addItem(QString::fromLatin1(tuxdsk::typeName(type)), static_cast<int>(type));
    // A type the header says and AMSDOS does not know stays what it is
    // unless another is chosen.
    if (file.type == tuxdsk::FileType::Unknown)
        type_->addItem(QString::fromLatin1(tuxdsk::typeName(file.type)), static_cast<int>(file.type));
    type_->setCurrentIndex(type_->findData(static_cast<int>(file.type)));
    load_ = new AddressBox;
    load_->setObjectName("sbStart");
    load_->setValue(file.loadAddress);
    entry_ = new AddressBox;
    entry_->setObjectName("sbEntry");
    entry_->setValue(file.entryAddress);
    readOnly_ = new QCheckBox(tr("&Read only"));
    readOnly_->setObjectName("ckReadOnly");
    readOnly_->setChecked(file.readOnly);
    hidden_ = new QCheckBox(tr("&Hidden"));
    hidden_->setObjectName("ckHidden");
    hidden_->setChecked(file.hidden);
    // A file without a header has no address to load at.
    const auto follow = [this] {
        const bool header = type() != tuxdsk::FileType::Ascii;
        load_->setEnabled(header);
        entry_->setEnabled(header);
    };
    connect(type_, &QComboBox::currentIndexChanged, this, follow);
    follow();

    auto* form = new QFormLayout;
    form->addRow(tr("Name of the file:"), fixedLabel(QString::fromStdString(file.name)));
    form->addRow(tr("User:"), fixedLabel(QString::number(file.user)));
    form->addRow(tr("&Type:"), type_);
    form->addRow(tr("&Start address:"), load_);
    const QString length = QStringLiteral("&%1").arg(file.length, 4, 16, QLatin1Char('0')).toUpper();
    form->addRow(tr("Size:"), fixedLabel(tr("%1 (%2), %n record(s)", nullptr, file.records).arg(length, bytesText(file.length))));
    form->addRow(tr("&Entry point:"), entry_);
    form->addRow(QString(), readOnly_);
    form->addRow(QString(), hidden_);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(validateOrCancel(this));
}

tuxdsk::FileType FileInfoDialog::type() const
{
    return static_cast<tuxdsk::FileType>(type_->currentData().toInt());
}

int FileInfoDialog::loadAddress() const
{
    return load_->value();
}

int FileInfoDialog::entryAddress() const
{
    return entry_->value();
}

bool FileInfoDialog::readOnly() const
{
    return readOnly_->isChecked();
}

bool FileInfoDialog::hidden() const
{
    return hidden_->isChecked();
}

// ---- RenameDialog --------------------------------------------------------------

RenameDialog::RenameDialog(const tuxdsk::DiscFile& file, QWidget* parent)
    : QDialog(parent)
{
    setObjectName("RenameDialog");
    setWindowTitle(tr("TuxDSK – Rename %1").arg(QString::fromStdString(file.name)));
    name_ = nameEdit(QString::fromStdString(file.name));
    user_ = new QSpinBox;
    user_->setObjectName("sbUser");
    user_->setRange(0, 255);
    user_->setValue(file.user);
    auto* hint = new QLabel(tr("User 229 is that of erased files."));

    auto* form = new QFormLayout;
    form->addRow(tr("&Name of the file:"), name_);
    form->addRow(tr("&User:"), user_);
    form->addRow(QString(), hint);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(validateOrCancel(this));
    name_->selectAll();
}

QString RenameDialog::name() const
{
    return name_->text().toUpper();
}

int RenameDialog::user() const
{
    return user_->value();
}

// ---- DiscFilesDialog -----------------------------------------------------------

DiscFilesDialog::DiscFilesDialog(const QString& discName, const std::vector<tuxdsk::DiscFile>& files, QWidget* parent)
    : QDialog(parent)
    , files_(files)
{
    setObjectName("DiscFilesDialog");
    setWindowTitle(tr("TuxDSK – Import from %1").arg(discName));
    auto* what = new QLabel(tr("The files of %1 to put on this disc, with their user and attributes:").arg(discName));
    what->setWordWrap(true);

    list_ = new QTreeWidget;
    list_->setObjectName("lvFiles");
    list_->setRootIsDecorated(false);
    list_->setUniformRowHeights(true);
    list_->setAlternatingRowColors(true);
    list_->setFont(fixedFont());
    list_->setHeaderLabels({tr("File Name"), tr("User"), tr("Type"), tr("Size")});
    for (const tuxdsk::DiscFile& file : files_) {
        auto* item = new QTreeWidgetItem(list_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Checked);
        item->setText(0, QString::fromStdString(file.name));
        item->setText(1, QString::number(file.user));
        item->setText(2, QString::fromLatin1(tuxdsk::typeName(file.type)));
        item->setText(3, tr("%1 Kb").arg(file.kilobytes()));
        for (const int column : {1, 3})
            item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
    }
    for (int column = 0; column < list_->columnCount(); ++column)
        list_->resizeColumnToContents(column);
    // The sizes stay beside the names, which take the room left.
    list_->header()->setStretchLastSection(false);
    list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(list_, &QTreeWidget::itemChanged, this, &DiscFilesDialog::follow);

    count_ = new QLabel;
    count_->setObjectName("lChosen");
    QPushButton* all = pageButton(tr("&All"), "bAll");
    QPushButton* none = pageButton(tr("&None"), "bNone");
    connect(all, &QPushButton::clicked, this, [this] { tickAll(true); });
    connect(none, &QPushButton::clicked, this, [this] { tickAll(false); });
    auto* ticks = new QHBoxLayout;
    ticks->addWidget(all);
    ticks->addWidget(none);
    ticks->addWidget(count_, 1);

    QDialogButtonBox* buttons = validateOrCancel(this);
    validate_ = buttons->button(QDialogButtonBox::Ok);
    validate_->setText(tr("Import"));
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(what);
    layout->addWidget(list_, 1);
    layout->addLayout(ticks);
    layout->addWidget(buttons);
    resize(480, 420);
    follow();
}

std::vector<tuxdsk::DiscFile> DiscFilesDialog::chosen() const
{
    std::vector<tuxdsk::DiscFile> chosen;
    for (int row = 0; row < list_->topLevelItemCount(); ++row)
        if (list_->topLevelItem(row)->checkState(0) == Qt::Checked)
            chosen.push_back(files_[static_cast<size_t>(row)]);
    return chosen;
}

void DiscFilesDialog::tickAll(bool ticked)
{
    for (int row = 0; row < list_->topLevelItemCount(); ++row)
        list_->topLevelItem(row)->setCheckState(0, ticked ? Qt::Checked : Qt::Unchecked);
}

// What is ticked, and how much room it takes; nothing ticked, nothing to do.
void DiscFilesDialog::follow()
{
    const std::vector<tuxdsk::DiscFile> ticked = chosen();
    int kilobytes = 0;
    for (const tuxdsk::DiscFile& file : ticked)
        kilobytes += file.kilobytes();
    count_->setText(tr("%n file(s) chosen, %1 Kb", nullptr, static_cast<int>(ticked.size())).arg(kilobytes));
    validate_->setEnabled(!ticked.empty());
}

// ---- NewDiscDialog -------------------------------------------------------------

NewDiscDialog::NewDiscDialog(bool moreTracks, QWidget* parent)
    : QDialog(parent)
{
    setObjectName("NewDiscDialog");
    setWindowTitle(tr("TuxDSK – Create DSK"));
    tracks_ = new QSpinBox;
    tracks_->setObjectName("sbTracks");
    tracks_->setRange(1, moreTracks ? 80 : 42);
    // What ManageDsk gives a disc it creates.
    tracks_->setValue(42);
    format_ = new QComboBox;
    format_->setObjectName("cbFormat");
    format_->addItem(tr("DATA"), static_cast<int>(tuxdsk::DiscFormat::Data));
    format_->addItem(tr("VENDOR (two tracks kept for CP/M)"), static_cast<int>(tuxdsk::DiscFormat::Vendor));

    auto* form = new QFormLayout;
    form->addRow(tr("Number of &tracks:"), tracks_);
    form->addRow(tr("&Format:"), format_);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    QDialogButtonBox* buttons = validateOrCancel(this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Format"));
    layout->addWidget(buttons);
}

int NewDiscDialog::tracks() const
{
    return tracks_->value();
}

tuxdsk::DiscFormat NewDiscDialog::format() const
{
    return static_cast<tuxdsk::DiscFormat>(format_->currentData().toInt());
}

// ---- TilesDialog ---------------------------------------------------------------

TilesDialog::TilesDialog(int count, int width, int height, int address, bool sprites, const QString& name, bool header,
                         QWidget* parent)
    : QDialog(parent)
{
    setObjectName("TilesDialog");
    setWindowTitle(tr("TuxDSK – Copy to the Disc"));
    const auto hex = [](int value) { return QStringLiteral("&%1").arg(value, 4, 16, QLatin1Char('0')).toUpper(); };
    const QString from = hex(address);
    // Tiles of the screen's modes go as a window of OCP Art Studio, side
    // by side, which loads at &4000.
    const QString loads = sprites ? from : hex(0x4000);
    auto* what = new QLabel(sprites ? tr("%n sprite(s) of %1 by %2, from %3: %4 bytes", nullptr, count)
                                          .arg(width)
                                          .arg(height)
                                          .arg(from)
                                          .arg(count * width * height)
                                    : tr("%n tile(s) of %1 bytes by %2 lines, from %3, side by side: "
                                         "an OCP Art Studio window of %4 bytes by %2 lines (.WIN)",
                                         nullptr, count)
                                          .arg(width)
                                          .arg(height)
                                          .arg(from)
                                          .arg(count * width));
    what->setObjectName("lTiles");
    name_ = nameEdit(name);
    // The address's own '&' is not the sign of a key.
    header_ = new QCheckBox(tr("With an AMSDOS &header: a binary that loads at %1").arg(QLatin1Char('&') + loads));
    header_->setObjectName("ckHeader");
    header_->setToolTip(sprites ? tr("With it, LOAD puts the bytes back where they were; without it, the file is the bytes alone")
                                : tr("With it, the window is as OCP Art Studio saves one; without it, the file is its "
                                     "lines and its size alone"));
    header_->setChecked(header);

    auto* form = new QFormLayout;
    form->addRow(what);
    form->addRow(tr("&Name of the file:"), name_);
    form->addRow(QString(), header_);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(validateOrCancel(this));
    name_->selectAll();
    name_->setFocus();
}

QString TilesDialog::name() const
{
    return name_->text().toUpper();
}

bool TilesDialog::withHeader() const
{
    return header_->isChecked();
}

// ---- AboutDialog ---------------------------------------------------------------

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName("AboutDialog");
    setWindowTitle(tr("TuxDSK – About"));
    auto* text = new QTextBrowser;
    text->setObjectName("tbAbout");
    text->setOpenExternalLinks(true);
    text->setFrameShape(QFrame::NoFrame);
    QString html = QString("<h2>TuxDSK %1</h2>").arg(QString::fromLatin1(tuxdsk::versionString()));
    html += tr("<p>The files of Amstrad CPC disc images (.DSK): added, exported, renamed, deleted, and shown as "
               "listings, screens or code.</p>"
               "<p>TuxDSK is ManageDsk %1 written again in C++, as a plugin for TuxAPE. ManageDsk is the work of "
               "Ludovic Deplanque (Demoniak); what TuxDSK does, and the discs it writes, are ManageDsk's.</p>"
               "<p>The Z80 instructions are named as ManageDsk names them, by the tables iDSK publishes; the Dams "
               "listing follows the DamsConverter of Thierry Jouin (Ramlaid). TuxDSK is free software under the "
               "MIT License, as iDSK is.</p>")
                .arg(QString::fromLatin1(tuxdsk::manageDskVersion()));
    html += tr("<h3>Greetings, as ManageDsk gives them</h3>"
               "<p>Thanks to Ramlaid, to Targhan, Longshot and Kukulcan for their many bug reports, to Simon Owen, "
               "to Lex Sparrow, and to all users.</p>"
               "<h3>Where ManageDsk reads and writes floppies</h3>"
               "<p>ManageDsk's drive A: is a real one. TuxDSK's are TuxAPE's: <b>Insert in A:</b> and <b>Insert in "
               "B:</b> put the disc in the emulated CPC's drives, and <b>Run</b> starts the file chosen there.</p>"
               "<h3>For a real CPC</h3>"
               "<p><b>Save As</b> offers two kinds of file: a .DSK file, for emulators, and an .HFE file, which a "
               "Gotek or an HxC drive put in place of a real CPC's drive reads. The .HFE file of a disc is the one "
               "the HxC Floppy Emulator's own software makes of it.</p>"
               "<h3>From TuxAPE's Find Graphics</h3>"
               "<p>The sprites or tiles selected there go to a file of the first disc by the right button's menu: "
               "<b>Copy to TuxDSK's Disc</b>. A disc is made if there is none.</p>"
               "<h3>The command line</h3>"
               "<p>The program <tt>tuxdsk</tt>, in the plugin's <tt>bin</tt> folder, takes ManageDsk's command "
               "line: <tt>tuxdsk -C -A*.BIN -Stoto.dsk</tt>.</p>"
               "<p>Light or dark, the windows follow TuxAPE's look: Settings, Dark Theme.</p>");
    text->setHtml(html);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(text, 1);
    layout->addWidget(buttons);
    resize(560, 520);
}

}  // namespace tuxdsk
