#include "viewerdialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "controls.h"
#include "disc/amsdos.h"
#include "hostfiles.h"
#include "preferences.h"
#include "prompts.h"
#include "screenwidget.h"
#include "view/basic.h"
#include "view/dams.h"
#include "view/disasm.h"
#include "view/text.h"

namespace tuxdsk {

namespace {

enum Saved { TextFile, PictureFile, WindowFile };

QString extensionOf(const tuxdsk::DiscFile& file)
{
    const QString name = QString::fromStdString(file.name);
    const qsizetype dot = name.lastIndexOf('.');
    return dot < 0 ? QString() : name.mid(dot + 1);
}

// Whether bytes read as a text: what a CPC prints, to the end-of-file mark.
bool looksLikeText(const std::vector<uint8_t>& content)
{
    size_t length = 0;
    for (const uint8_t byte : content) {
        if (byte == 0x1A)
            break;
        if (byte != '\r' && byte != '\n' && byte != '\t' && (byte < ' ' || byte > 0x7E))
            return false;
        ++length;
    }
    return length > 0;
}

QWidget* row(std::initializer_list<QWidget*> widgets)
{
    auto* piece = new QWidget;
    auto* layout = new QHBoxLayout(piece);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QWidget* widget : widgets)
        layout->addWidget(widget);
    layout->addStretch(1);
    return piece;
}

}  // namespace

ViewerDialog::ViewerDialog(const tuxdsk::DiscFile& file, std::vector<uint8_t> records,
                           std::optional<tuxdsk::ScreenLook> palette, Preferences* preferences, QWidget* parent)
    : QDialog(parent)
    , file_(file)
    , preferences_(preferences)
{
    setObjectName("ViewerDialog");
    setWindowTitle(tr("TuxDSK – Show File: %1").arg(QString::fromStdString(file.name)));

    // The file without its header, and no longer than its header says.
    const std::optional<tuxdsk::AmsdosHeader> header = tuxdsk::amsdosHeader(records);
    if (header) {
        const size_t there = records.size() - tuxdsk::kAmsdosHeaderSize;
        const size_t length = header->length > 0 && header->length <= there ? header->length : there;
        content_.assign(records.begin() + tuxdsk::kAmsdosHeaderSize,
                        records.begin() + static_cast<std::ptrdiff_t>(tuxdsk::kAmsdosHeaderSize + length));
    } else {
        content_ = std::move(records);
    }
    const QString extension = extensionOf(file);
    const std::optional<tuxdsk::OcpWindow> window =
        extension == QLatin1String("WIN") ? tuxdsk::readWindow(content_) : std::nullopt;
    memory_ = window ? window->data : tuxdsk::screenMemory(content_);

    mode_ = new QComboBox;
    mode_->setObjectName("cbMode");
    mode_->addItems({tr("Hexadecimal mode"), tr("Listing mode"), tr("Dams mode"), tr("Disassembly mode"),
                     tr("Video memory mode")});

    text_ = new QPlainTextEdit;
    text_->setObjectName("teText");
    text_->setReadOnly(true);
    text_->setFont(fixedFont());
    text_->setLineWrapMode(QPlainTextEdit::NoWrap);
    screen_ = new ScreenWidget;
    screen_->setObjectName("swScreen");
    views_ = new QStackedWidget;
    views_->addWidget(text_);
    views_->addWidget(screen_);

    options_ = new QStackedWidget;
    options_->addWidget(new QWidget);  // Hexadecimal
    options_->addWidget(createListingOptions());
    options_->addWidget(new QWidget);  // Dams
    options_->addWidget(createDisassemblyOptions());
    options_->addWidget(createScreenOptions());
    options_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    // The look the file comes with: its palette's, or the one a screen of
    // ConvImgCpc carries.
    const std::optional<tuxdsk::ScreenLook> look = palette ? palette : tuxdsk::lookInScreen(memory_);
    if (look) {
        screenMode_->setCurrentIndex(std::clamp(look->mode, 0, 2));
        screen_->setInks(look->inks);
    }
    if (window) {
        flat_->setChecked(true);
        columns_->setValue(window->lineBytes);
        lines_->setValue(window->height);
    }
    start_->setValue(file.loadAddress);

    saveText_ = pageButton(tr("Save as &Text..."), "bSaveText");
    savePicture_ = pageButton(tr("Save &Picture..."), "bSavePicture");
    saveWindow_ = pageButton(tr("Save &Window..."), "bSaveWindow");
    saveWindow_->setToolTip(tr("The flat picture as an OCP Art Studio window (.WIN)"));
    QPushButton* close = pageButton(tr("Close"), "bClose");
    connect(saveText_, &QPushButton::clicked, this, [this] { chooseAndSave(TextFile); });
    connect(savePicture_, &QPushButton::clicked, this, [this] { chooseAndSave(PictureFile); });
    connect(saveWindow_, &QPushButton::clicked, this, [this] { chooseAndSave(WindowFile); });
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    auto* top = new QHBoxLayout;
    top->addWidget(mode_);
    top->addWidget(options_, 1);
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(saveText_);
    bottom->addWidget(savePicture_);
    bottom->addWidget(saveWindow_);
    bottom->addStretch(1);
    bottom->addWidget(close);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(views_, 1);
    layout->addLayout(bottom);
    resize(860, 640);

    connect(mode_, &QComboBox::currentIndexChanged, this, &ViewerDialog::show_);
    // The mode the file is shown in first.
    Mode first = Hexadecimal;
    if (file.type == tuxdsk::FileType::Basic || file.type == tuxdsk::FileType::ProtectedBasic)
        first = Listing;
    else if (extension == QLatin1String("SCR") || window)
        first = Screen;
    else if (extension == QLatin1String("DAM"))
        first = Dams;
    else if (!file.hasHeader() && looksLikeText(content_))
        first = Listing;
    mode_->setCurrentIndex(first);
    built_ = true;
    show_();
}

QWidget* ViewerDialog::createListingOptions()
{
    wrap_ = new QCheckBox(tr("Line &wrap like CPC mode 2"));
    wrap_->setObjectName("ckWrap");
    wrap_->setChecked(preferences_ && preferences_->wrapListings());
    french_ = new QCheckBox(tr("&French accented letters"));
    french_->setObjectName("ckFrench");
    french_->setToolTip(tr("On a French CPC, @ \\ { | } are à ç é ù è"));
    french_->setChecked(preferences_ && preferences_->frenchLetters());
    connect(wrap_, &QCheckBox::toggled, this, [this](bool on) {
        if (preferences_)
            preferences_->setWrapListings(on);
        show_();
    });
    connect(french_, &QCheckBox::toggled, this, [this](bool on) {
        if (preferences_)
            preferences_->setFrenchLetters(on);
        show_();
    });
    return row({wrap_, french_});
}

QWidget* ViewerDialog::createDisassemblyOptions()
{
    start_ = new AddressBox;
    start_->setObjectName("sbStart");
    auto* label = new QLabel(tr("&Start address:"));
    label->setBuddy(start_);
    connect(start_, &QSpinBox::valueChanged, this, &ViewerDialog::show_);
    return row({label, start_});
}

QWidget* ViewerDialog::createScreenOptions()
{
    screenMode_ = new QComboBox;
    screenMode_->setObjectName("cbScreenMode");
    screenMode_->addItems({tr("Mode 0"), tr("Mode 1"), tr("Mode 2")});
    screenMode_->setCurrentIndex(1);
    flat_ = new QCheckBox(tr("&Flat"));
    flat_->setObjectName("ckFlat");
    flat_->setToolTip(tr("Flat video memory mode: one line after the other, as a sprite or a window is kept"));
    columns_ = new QSpinBox;
    columns_->setObjectName("sbColumns");
    columns_->setRange(1, 80);
    columns_->setValue(80);
    columns_->setToolTip(tr("Number of columns: bytes across"));
    lines_ = new QSpinBox;
    lines_->setObjectName("sbLines");
    lines_->setRange(1, 200);
    lines_->setValue(200);
    lines_->setToolTip(tr("Number of rows"));
    offset_ = new AddressBox(static_cast<int>(std::max<size_t>(memory_.size(), 1)) - 1);
    offset_->setObjectName("sbOffset");
    offset_->setToolTip(tr("Offset: where the picture starts in the file"));
    zoom_ = new QSpinBox;
    zoom_->setObjectName("sbZoom");
    zoom_->setRange(1, 8);
    zoom_->setPrefix(QStringLiteral("× "));

    // Flat, a picture is as wide and as high as one says; as a screen it is
    // no more than a screen's 80 bytes by 200 lines.
    const auto limits = [this] {
        const bool flat = flat_->isChecked();
        columns_->setMaximum(flat ? 128 : 80);
        lines_->setMaximum(flat ? 1024 : 200);
    };
    connect(flat_, &QCheckBox::toggled, this, [this, limits] {
        limits();
        show_();
    });
    connect(screenMode_, &QComboBox::currentIndexChanged, this, &ViewerDialog::show_);
    for (QSpinBox* box : {columns_, lines_, static_cast<QSpinBox*>(offset_)})
        connect(box, &QSpinBox::valueChanged, this, &ViewerDialog::show_);
    connect(zoom_, &QSpinBox::valueChanged, this, [this](int zoom) { screen_->setZoom(zoom); });

    return row({screenMode_, flat_, new QLabel(tr("Columns:")), columns_, new QLabel(tr("Rows:")), lines_,
                new QLabel(tr("Offset:")), offset_, zoom_});
}

ViewerDialog::Mode ViewerDialog::mode() const
{
    return static_cast<Mode>(mode_->currentIndex());
}

void ViewerDialog::setMode(Mode mode)
{
    mode_->setCurrentIndex(mode);
}

tuxdsk::ScreenView ViewerDialog::screenView() const
{
    tuxdsk::ScreenView view;
    view.mode = screenMode_->currentIndex();
    view.flat = flat_->isChecked();
    view.columns = columns_->value();
    view.lines = lines_->value();
    view.offset = offset_->value();
    return view;
}

void ViewerDialog::show_()
{
    if (!built_)
        return;
    const Mode shown = mode();
    options_->setCurrentIndex(shown);
    views_->setCurrentIndex(shown == Screen ? 1 : 0);
    saveText_->setEnabled(shown != Screen);
    savePicture_->setEnabled(shown == Screen);
    saveWindow_->setEnabled(shown == Screen && flat_->isChecked());

    std::string listing;
    switch (shown) {
    case Hexadecimal:
        listing = tuxdsk::hexDump(content_);
        break;
    case Listing: {
        const bool program = file_.type == tuxdsk::FileType::Basic || file_.type == tuxdsk::FileType::ProtectedBasic;
        if (file_.type == tuxdsk::FileType::ProtectedBasic)
            listing = tuxdsk::basicListing(tuxdsk::unprotectedBasic(content_));
        else
            listing = program ? tuxdsk::basicListing(content_) : tuxdsk::asciiListing(content_);
        if (wrap_->isChecked())
            listing = tuxdsk::wrapped(listing);
        listing = tuxdsk::cpcText(listing, french_->isChecked());
        break;
    }
    case Dams:
        listing = tuxdsk::cpcText(tuxdsk::damsListing(content_));
        break;
    case Disassembly:
        listing = tuxdsk::disassembly(content_, start_->value());
        break;
    case Screen:
        screen_->setPicture(tuxdsk::screenPicture(memory_, screenView()), screenMode_->currentIndex());
        return;
    }
    text_->setPlainText(QString::fromUtf8(listing.c_str(), static_cast<qsizetype>(listing.size())));
}

QString ViewerDialog::text() const
{
    return mode() == Screen ? QString() : text_->toPlainText();
}

QImage ViewerDialog::picture() const
{
    return mode() == Screen ? screen_->image() : QImage();
}

bool ViewerDialog::saveText(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QByteArray bytes = text_->toPlainText().toUtf8();
    return file.write(bytes) == bytes.size();
}

bool ViewerDialog::savePicture(const QString& path) const
{
    // As the monitor shows it: each line twice.
    const QImage image = screen_->image();
    return !image.isNull() && image.scaled(image.width(), image.height() * 2).save(path, "PNG");
}

bool ViewerDialog::saveWindow(const QString& path) const
{
    const tuxdsk::ScreenView view = screenView();
    const size_t size = static_cast<size_t>(view.columns) * static_cast<size_t>(view.lines);
    const size_t from = std::min<size_t>(static_cast<size_t>(view.offset), memory_.size());
    const std::span<const uint8_t> shown(memory_.data() + from, std::min(size, memory_.size() - from));
    const std::vector<uint8_t> window = tuxdsk::windowFile(shown, view.columns, view.lines);
    // A binary that loads at &4000, as OCP Art Studio's own.
    std::vector<uint8_t> bytes = tuxdsk::makeAmsdosHeader(QFileInfo(path).fileName().toStdString(),
                                                          tuxdsk::kAmsdosBinary, 0x4000, 0, window.size());
    bytes.insert(bytes.end(), window.begin(), window.end());
    return writeHostFile(path, bytes);
}

void ViewerDialog::chooseAndSave(int what)
{
    QString base = QString::fromStdString(file_.name);
    base.truncate(base.lastIndexOf('.') < 0 ? base.size() : base.lastIndexOf('.'));
    const QString folder = preferences_ && !preferences_->fileFolder().isEmpty() ? preferences_->fileFolder() + '/' : QString();
    const QString suggested = folder + base + (what == TextFile ? ".TXT" : what == PictureFile ? ".png" : ".WIN");
    const QString filter = what == TextFile      ? tr("Text Files (*.TXT);;All Files (*)")
                           : what == PictureFile ? tr("PNG Pictures (*.png);;All Files (*)")
                                                 : tr("WIN Files (*.WIN);;All Files (*)");
    const QString path = QFileDialog::getSaveFileName(this, windowTitle(), suggested, filter);
    if (path.isEmpty())
        return;
    if (preferences_)
        preferences_->setFileFolder(QFileInfo(path).absolutePath());
    const bool saved = what == TextFile ? saveText(path) : what == PictureFile ? savePicture(path) : saveWindow(path);
    if (!saved)
        prompts::problem(this, tr("%1 cannot be written.").arg(QDir::toNativeSeparators(path)));
}

}  // namespace tuxdsk
