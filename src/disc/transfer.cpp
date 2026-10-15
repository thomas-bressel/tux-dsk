#include "disc/transfer.h"

#include <algorithm>

namespace tuxdsk {

std::vector<uint8_t> importedFile(const std::string& name, std::span<const uint8_t> file, const ImportOptions& options)
{
    std::vector<uint8_t> content;
    switch (options.kind) {
    case ImportKind::AsIs:
        content.assign(file.begin(), file.end());
        break;
    case ImportKind::Ascii: {
        const std::span<const uint8_t> text = withoutAmsdosHeader(file);
        content.assign(text.begin(), text.end());
        break;
    }
    case ImportKind::Binary:
        if (hasAmsdosHeader(file)) {
            content.assign(file.begin(), file.end());
            if (options.setAddresses)
                setAmsdosHeader(content, content[18], static_cast<uint16_t>(options.loadAddress),
                                static_cast<uint16_t>(options.entryAddress));
        } else {
            content = makeAmsdosHeader(name, options.type, static_cast<uint16_t>(options.loadAddress),
                                       static_cast<uint16_t>(options.entryAddress), file.size());
            content.insert(content.end(), file.begin(), file.end());
        }
        break;
    }
    if (options.cleanHeader)
        cleanAmsdosHeader(content);
    return content;
}

std::vector<uint8_t> exportedFile(std::span<const uint8_t> records, bool withHeader, bool cleanHeader)
{
    const std::optional<AmsdosHeader> header = amsdosHeader(records);
    if (!header)
        return {records.begin(), records.end()};
    // What the header says the file holds, if the records have that much.
    const size_t there = records.size() - kAmsdosHeaderSize;
    const size_t length = header->length > 0 && header->length <= there ? header->length : there;
    const size_t from = withHeader ? 0 : kAmsdosHeaderSize;
    std::vector<uint8_t> file(records.begin() + static_cast<std::ptrdiff_t>(from),
                              records.begin() + static_cast<std::ptrdiff_t>(kAmsdosHeaderSize + length));
    if (withHeader && cleanHeader)
        cleanAmsdosHeader(file);
    return file;
}

}  // namespace tuxdsk
