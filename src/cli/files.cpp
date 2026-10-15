#include "cli/files.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace tuxdsk::cli {

namespace {

char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
}

std::string lowered(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), lower);
    return text;
}

bool matchesFrom(const std::string& mask, size_t m, const std::string& name, size_t n)
{
    for (; m < mask.size(); ++m, ++n) {
        if (mask[m] == '*') {
            for (size_t rest = n; rest <= name.size(); ++rest)
                if (matchesFrom(mask, m + 1, name, rest))
                    return true;
            return false;
        }
        if (n >= name.size() || (mask[m] != '?' && lower(mask[m]) != lower(name[n])))
            return false;
    }
    return n == name.size();
}

}  // namespace

std::optional<std::vector<uint8_t>> readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad())
        return std::nullopt;
    return bytes;
}

bool writeFile(const std::string& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.flush();
    return file.good();
}

bool matches(const std::string& mask, const std::string& name)
{
    // For Windows "*.*" is every file, with a dot in its name or not.
    return matchesFrom(mask == "*.*" ? "*" : mask, 0, name, 0);
}

std::vector<std::string> filesOf(const std::string& mask)
{
    namespace fs = std::filesystem;
    std::error_code error;
    if (mask.find_first_of("*?") == std::string::npos)
        return fs::is_regular_file(mask, error) ? std::vector<std::string>{mask} : std::vector<std::string>{};

    const size_t cut = mask.find_last_of("/\\");
    const std::string folder = cut == std::string::npos ? std::string() : mask.substr(0, cut + 1);
    const std::string pattern = cut == std::string::npos ? mask : mask.substr(cut + 1);
    std::vector<std::string> found;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder.empty() ? "." : folder, error)) {
        const std::string name = entry.path().filename().string();
        if (entry.is_regular_file(error) && matches(pattern, name))
            found.push_back(folder + name);
    }
    // In the order Windows lists them: by name, capitals or not.
    std::sort(found.begin(), found.end(),
              [](const std::string& a, const std::string& b) { return lowered(a) < lowered(b); });
    return found;
}

}  // namespace tuxdsk::cli
