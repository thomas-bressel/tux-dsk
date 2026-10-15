#include "disc/names.h"

#include <cstring>

namespace tuxdsk {

namespace {

// What a name of a CPC's directory can be made of.
bool allowed(unsigned char c)
{
    return c > ' ' && c < 0x7F && !std::strchr("<>.,;:=?*[]", c);
}

char capital(unsigned char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : static_cast<char>(c);
}

std::string cleaned(const std::string& part, size_t most)
{
    std::string kept;
    for (const unsigned char c : part)
        if (allowed(c) && kept.size() < most)
            kept += capital(c);
    return kept;
}

}  // namespace

std::string cpcName(const std::string& hostName)
{
    const size_t folder = hostName.find_last_of("/\\");
    const std::string file = folder == std::string::npos ? hostName : hostName.substr(folder + 1);
    const size_t dot = file.find('.');
    const std::string name = cleaned(file.substr(0, dot), 8);
    if (name.empty())
        return {};
    // The extension stops at the next dot: "a.b.c" is "A.B".
    std::string extension;
    if (dot != std::string::npos) {
        const std::string rest = file.substr(dot + 1);
        extension = cleaned(rest.substr(0, rest.find('.')), 3);
    }
    return extension.empty() ? name : name + '.' + extension;
}

std::array<uint8_t, 11> directoryName(const std::string& name)
{
    std::array<uint8_t, 11> padded;
    padded.fill(' ');
    const std::string clean = cpcName(name);
    const size_t dot = clean.find('.');
    const std::string base = clean.substr(0, dot);
    for (size_t at = 0; at < base.size(); ++at)
        padded[at] = static_cast<uint8_t>(base[at]);
    if (dot != std::string::npos)
        for (size_t at = dot + 1; at < clean.size(); ++at)
            padded[8 + at - dot - 1] = static_cast<uint8_t>(clean[at]);
    return padded;
}

std::string shownName(std::span<const uint8_t> entryName)
{
    const auto printable = [](uint8_t byte) {
        const char c = static_cast<char>(byte & 0x7F);
        return c >= ' ' && c < 0x7F ? c : '?';
    };
    std::string name;
    for (size_t at = 0; at < 8 && at < entryName.size(); ++at)
        name += printable(entryName[at]);
    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    std::string extension;
    for (size_t at = 8; at < 11 && at < entryName.size(); ++at)
        extension += printable(entryName[at]);
    while (!extension.empty() && extension.back() == ' ')
        extension.pop_back();
    return extension.empty() ? name : name + '.' + extension;
}

}  // namespace tuxdsk
