#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QString>

// The computer's files, for the windows.

std::optional<std::vector<uint8_t>> readHostFile(const QString& path);
bool writeHostFile(const QString& path, const std::vector<uint8_t>& bytes);
