#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The computer's files, for the command-line program.

namespace tuxdsk::cli {

std::optional<std::vector<uint8_t>> readFile(const std::string& path);
bool writeFile(const std::string& path, const std::vector<uint8_t>& bytes);

// Whether a name is one a mask stands for: '*' is any characters and '?'
// any one, and capitals do not count, as on the Windows ManageDsk runs on.
bool matches(const std::string& mask, const std::string& name);

// The files a mask stands for ("*.BIN", "src/*.bas"), by name. A mask
// without '*' or '?' is the file it names, if it is there.
std::vector<std::string> filesOf(const std::string& mask);

}  // namespace tuxdsk::cli
