#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "check.h"
#include "golden.h"

// What the tests of the command-line program share: a folder of the test's
// own, with the files the golden discs were made of, and the program run in
// it as a user runs it.

namespace cli {

namespace fs = std::filesystem;

// A folder that goes when the test is done.
class Scratch {
public:
    explicit Scratch(const std::string& name)
        : path_(fs::temp_directory_path() / ("tuxdsk_" + name + "_" + std::to_string(std::rand())))
    {
        fs::remove_all(path_);
        fs::create_directories(path_);
    }
    ~Scratch()
    {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    std::string file(const std::string& name) const { return (path_ / name).string(); }
    const fs::path& path() const { return path_; }

    void write(const std::string& name, const golden::Bytes& bytes) const
    {
        std::ofstream out(path_ / name, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    golden::Bytes read(const std::string& name) const { return golden::read(file(name)); }

    // The files of tests/golden/inputs, and the twelve that are made by rule.
    void addInputs(bool withFill = false) const
    {
        for (const auto& entry : fs::directory_iterator(std::string(TUXDSK_GOLDEN_DIR) + "/inputs"))
            fs::copy_file(entry.path(), path_ / entry.path().filename());
        for (int index = 0; withFill && index < 12; ++index) {
            char name[16];
            std::snprintf(name, sizeof name, "fill%02d.dat", index);
            write(name, golden::fill(index));
        }
    }

private:
    fs::path path_;
};

}  // namespace cli
