#include "hostfiles.h"

#include <QFile>
#include <QSaveFile>

namespace tuxdsk {

std::optional<std::vector<uint8_t>> readHostFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray all = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return std::nullopt;
    return std::vector<uint8_t>(all.begin(), all.end());
}

bool writeHostFile(const QString& path, const std::vector<uint8_t>& bytes)
{
    // Written beside its place, then moved there: a disc that cannot be
    // written whole leaves the one that was there.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    return file.commit();
}

}  // namespace tuxdsk
