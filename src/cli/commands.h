#pragma once

#include <string>
#include <vector>

// ManageDsk's command line: commands that follow one another, each a '-',
// a letter (a capital or not) and, with no space, what it works on.
//
//   -C                                   an empty disc
//   -L<disc>                             a disc read from a .DSK or .HFE file
//   -A<mask>                             files put on the disc as they are
//   -I<file>/<name>/<type>/<start>/<entry>
//                                        a file put on the disc under a
//                                        name, with the header of its type
//   -S<disc>                             the disc saved as a .DSK file, or as
//                                        an .HFE one if its name ends so
//
//   tuxdsk -C -A*.BIN -Stoto.dsk
//   tuxdsk -LOld.dsk -A*.BAS -A*.BIN -SNew.dsk

namespace tuxdsk::cli {

constexpr int kStatusDone = 0;
constexpr int kStatusFailed = 1;
constexpr int kStatusBadLine = 2;

struct Command {
    char letter = 0;  // 'C', 'L', 'A', 'I' or 'S'
    std::string argument;
};

struct ParsedLine {
    std::vector<Command> commands;
    bool usageAsked = false;
    std::string error;  // why the line cannot be done; empty if it can

    bool ok() const { return error.empty(); }
};

ParsedLine parseCommandLine(const std::vector<std::string>& arguments);
std::string usageText();

// What -I is given, taken apart.
struct ImportRequest {
    enum class Type { Binary, Basic, Ascii };

    std::string file;  // the computer's
    std::string name;  // on the disc
    Type type = Type::Binary;
    int start = 0;
    int entry = 0;
    std::string error;

    bool ok() const { return error.empty(); }
};

ImportRequest parseImport(const std::string& argument);

struct Outcome {
    int status = kStatusDone;
    std::vector<std::string> said;  // what was done, a line each
    std::string error;              // what stopped it
};

// Does the commands, in order, and stops at the first that cannot be done.
Outcome run(const std::vector<Command>& commands);

}  // namespace tuxdsk::cli
