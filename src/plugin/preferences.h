#pragma once

#include <QHash>
#include <QString>
#include <QVariant>

namespace tuxdsk {

// What TuxDSK remembers from one day to the next: the folders discs and
// files were last taken from, and the choices of the manager's window and
// of the viewer. They are kept in TuxDSK.ini, in the folder TuxAPE gives
// the plugin for its own; with no such folder, as when a test shows a
// window by itself, they are remembered for as long as the window lives.
class Preferences {
public:
    // `folder` is PluginHost::dataFolder(), or empty.
    explicit Preferences(const QString& folder = QString());

    // The file the preferences are kept in, or empty.
    QString file() const { return file_; }

    QString discFolder() const { return text("Folders/Discs"); }
    void setDiscFolder(const QString& folder) { set("Folders/Discs", folder); }
    QString fileFolder() const { return text("Folders/Files"); }
    void setFileFolder(const QString& folder) { set("Folders/Files", folder); }

    // ManageDsk's choices: what a file added is taken for (0 asks, 1 is
    // ASCII, 2 binary), whether a file exported keeps its header, the
    // clean-up of headers, more than 42 tracks, and the second disc.
    int importMode() const { return value("Manager/Import", 0).toInt(); }
    void setImportMode(int mode) { set("Manager/Import", mode); }
    bool exportHeader() const { return value("Manager/ExportHeader", true).toBool(); }
    void setExportHeader(bool on) { set("Manager/ExportHeader", on); }
    bool cleanHeaders() const { return value("Manager/CleanHeaders", false).toBool(); }
    void setCleanHeaders(bool on) { set("Manager/CleanHeaders", on); }
    bool moreTracks() const { return value("Manager/MoreTracks", false).toBool(); }
    void setMoreTracks(bool on) { set("Manager/MoreTracks", on); }
    bool twoDiscs() const { return value("Manager/TwoDiscs", false).toBool(); }
    void setTwoDiscs(bool on) { set("Manager/TwoDiscs", on); }

    // The viewer's: lines cut as the CPC's mode 2 cuts them, and the
    // accented letters of a French CPC.
    bool wrapListings() const { return value("Viewer/Wrap", false).toBool(); }
    void setWrapListings(bool on) { set("Viewer/Wrap", on); }
    bool frenchLetters() const { return value("Viewer/French", false).toBool(); }
    void setFrenchLetters(bool on) { set("Viewer/French", on); }

    // Tiles taken from TuxAPE's Find Graphics window: whether their file is
    // given an AMSDOS header.
    bool tilesHeader() const { return value("Tiles/Header", true).toBool(); }
    void setTilesHeader(bool on) { set("Tiles/Header", on); }

private:
    QVariant value(const char* key, const QVariant& fallback) const { return kept_.value(QLatin1String(key), fallback); }
    QString text(const char* key) const { return value(key, QString()).toString(); }
    void set(const char* key, const QVariant& value);

    QString file_;
    QHash<QString, QVariant> kept_;
};

}  // namespace tuxdsk
