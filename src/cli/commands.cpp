#include "cli/commands.h"

#include <cstdlib>
#include <optional>

#include "base/version.h"
#include "cli/files.h"
#include "disc/amsdos.h"
#include "disc/disc.h"
#include "disc/hfe.h"
#include "disc/names.h"

namespace tuxdsk::cli {

namespace {

char capital(char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c;
}

std::string capitals(std::string text)
{
    for (char& c : text)
        c = capital(c);
    return text;
}

// A type of -I: ManageDsk knows them by their first letters.
std::optional<ImportRequest::Type> importType(const std::string& word)
{
    const std::string type = capitals(word);
    if (type.rfind("BIN", 0) == 0)
        return ImportRequest::Type::Binary;
    if (type.rfind("BAS", 0) == 0)
        return ImportRequest::Type::Basic;
    if (type.rfind("ASC", 0) == 0)
        return ImportRequest::Type::Ascii;
    return std::nullopt;
}

// An address: a number as ManageDsk reads it, or in hexadecimal after '&',
// '#' or "0x". Nothing if the word is not one.
std::optional<int> address(const std::string& word)
{
    if (word.empty())
        return std::nullopt;
    const char* digits = word.c_str();
    int base = 10;
    if (word[0] == '&' || word[0] == '#') {
        ++digits;
        base = 16;
    } else if (word.size() > 2 && word[0] == '0' && capital(word[1]) == 'X') {
        digits += 2;
        base = 16;
    }
    char* end = nullptr;
    const long value = std::strtol(digits, &end, base);
    if (end == digits || *end != '\0')
        return std::nullopt;
    return static_cast<int>(value & 0xFFFF);
}

// Why a file is not a disc image, as the command line says it.
std::string reasonSaid(ImageError error)
{
    switch (error) {
    case ImageError::None:
        break;
    case ImageError::TooShort:
        return "the file is too short to be a disc image";
    case ImageError::NotDsk:
        return "the file is not a .DSK disc image";
    case ImageError::NoTrack:
        return "the disc image has no track";
    case ImageError::TooManyTracks:
        return "the disc image has more tracks than its header can list";
    case ImageError::NotHfe:
        return "the file is not an .HFE disc image";
    case ImageError::HfeKind:
        return "the .HFE image is of a kind that is not read: only the first, in MFM, is";
    case ImageError::HfeNoTrack:
        return "the .HFE image has no track";
    }
    return "the file is not a disc image";
}

std::string sizeSaid(size_t bytes)
{
    return std::to_string(bytes) + (bytes == 1 ? " byte" : " bytes");
}

// Whether a disc is to be saved as an HxC Floppy Emulator's image: its
// name says, by ending in ".hfe".
bool namesHfe(const std::string& path)
{
    return path.size() >= 4 && capitals(path.substr(path.size() - 4)) == ".HFE";
}

const char* why(AddResult result)
{
    switch (result) {
    case AddResult::BadName: return "its name cannot be one of a CPC's directory";
    case AddResult::Exists: return "it is already on the disc";
    case AddResult::NoEntry: return "the directory is full";
    case AddResult::NoSpace: return "there is no room left on the disc";
    case AddResult::NoDisc: return "the disc is not in the DATA, VENDOR or PCW format";
    case AddResult::Done: break;
    }
    return "";
}

}  // namespace

ParsedLine parseCommandLine(const std::vector<std::string>& arguments)
{
    ParsedLine line;
    for (const std::string& argument : arguments) {
        const std::string word = capitals(argument);
        if (word == "-HELP" || word == "--HELP" || word == "-H" || word == "-?" || word == "/?") {
            line.usageAsked = true;
            return line;
        }
        if (argument.size() < 2 || argument[0] != '-') {
            line.error = "\"" + argument + "\" is not a command: commands start with '-'";
            return line;
        }
        Command command;
        command.letter = capital(argument[1]);
        command.argument = argument.substr(2);
        switch (command.letter) {
        case 'C':
            if (!command.argument.empty()) {
                line.error = "-C takes nothing after it";
                return line;
            }
            break;
        case 'L':
        case 'A':
        case 'S':
        case 'I':
            if (command.argument.empty()) {
                line.error = std::string("-") + command.letter + " needs a file name right after it, without a space";
                return line;
            }
            break;
        default:
            line.error = "\"" + argument + "\" is not a command";
            return line;
        }
        line.commands.push_back(std::move(command));
    }
    return line;
}

std::string usageText()
{
    return std::string("tuxdsk ") + versionString() + ": ManageDsk " + manageDskVersion()
           + "'s command line.\n"
             "\n"
             "Commands are done in the order they are given. What a command works on comes\n"
             "right after its letter, without a space; letters may be capitals or not.\n"
             "\n"
             "  -C          makes an empty disc (DATA format, 42 tracks)\n"
             "  -L<disc>    reads a disc from a .DSK file, or from an .HFE one\n"
             "  -A<mask>    adds files to the disc as they are: -A*.BIN, -Agame.bas\n"
             "  -I<file>/<name>/<type>/<start>/<entry>\n"
             "              adds a file under a name, with the header of its type:\n"
             "              BIN, BAS or ASCII; <start> and <entry> are the addresses it\n"
             "              loads at and is entered at, in decimal or as &4000\n"
             "  -S<disc>    saves the disc as a .DSK file; as an .HFE file, for a Gotek\n"
             "              or an HxC drive on a real CPC, if its name ends in .hfe\n"
             "\n"
             "  tuxdsk -C -A*.BIN -Stoto.dsk\n"
             "  tuxdsk -LOld.dsk -A*.BAS -Iscreen.raw/SCREEN.SCR/BIN/49152 -SNew.dsk\n";
}

ImportRequest parseImport(const std::string& argument)
{
    ImportRequest request;
    std::vector<std::string> parts;
    size_t from = 0;
    for (size_t slash; (slash = argument.find('/', from)) != std::string::npos; from = slash + 1)
        parts.push_back(argument.substr(from, slash - from));
    parts.push_back(argument.substr(from));

    // The file's own folders are parted with '/' too: the type is looked
    // for from the end, with at most the two addresses after it.
    size_t typeAt = 0;
    for (size_t after = 0; after <= 2 && typeAt == 0; ++after) {
        if (parts.size() < after + 3)
            break;
        const size_t at = parts.size() - 1 - after;
        bool numbers = true;
        for (size_t each = at + 1; each < parts.size(); ++each)
            numbers = numbers && address(parts[each]).has_value();
        if (numbers && importType(parts[at]))
            typeAt = at;
    }
    if (typeAt == 0) {
        request.error = "-I" + argument + ": expected <file>/<name>/<type>/<start>/<entry>, the type BIN, BAS or ASCII";
        return request;
    }
    request.type = *importType(parts[typeAt]);
    request.name = parts[typeAt - 1];
    for (size_t each = 0; each + 1 < typeAt; ++each)
        request.file += (each ? "/" : "") + parts[each];
    // A BASIC program loads at &170 unless told otherwise.
    request.start = request.type == ImportRequest::Type::Basic ? 0x170 : 0;
    if (typeAt + 1 < parts.size())
        request.start = *address(parts[typeAt + 1]);
    if (typeAt + 2 < parts.size())
        request.entry = *address(parts[typeAt + 2]);
    if (request.file.empty() || cpcName(request.name).empty())
        request.error = "-I" + argument + ": the file or its name on the disc is missing";
    return request;
}

Outcome run(const std::vector<Command>& commands)
{
    Outcome outcome;
    std::optional<CpcDisc> disc;
    bool changed = false;
    const auto fail = [&outcome](const std::string& error) {
        outcome.status = kStatusFailed;
        outcome.error = error;
        return outcome;
    };
    // The command line puts files up to the last block there can be, as
    // ManageDsk's does.
    AddOptions options;
    options.blocks = kAllBlocks;

    for (const Command& command : commands) {
        if (command.letter != 'C' && command.letter != 'L' && !disc)
            return fail(std::string("-") + command.letter + ": there is no disc yet, -C makes one and -L reads one");
        switch (command.letter) {
        case 'C':
            disc.emplace();
            changed = true;
            outcome.said.push_back("Disc created.");
            break;
        case 'L': {
            const auto file = readFile(command.argument);
            if (!file)
                return fail(command.argument + " cannot be read");
            ImageError reason = ImageError::None;
            std::optional<DiscImage> image = parseDisc(*file, &reason);
            if (!image)
                return fail(command.argument + ": " + reasonSaid(reason));
            disc.emplace(std::move(*image));
            changed = false;
            outcome.said.push_back("Read " + command.argument + ".");
            break;
        }
        case 'A': {
            const std::vector<std::string> files = filesOf(command.argument);
            if (files.empty()) {
                if (command.argument.find_first_of("*?") == std::string::npos)
                    return fail(command.argument + " cannot be read");
                outcome.said.push_back("No file is called " + command.argument + ".");
            }
            for (const std::string& path : files) {
                const auto content = readFile(path);
                if (!content)
                    return fail(path + " cannot be read");
                const AddResult result = disc->add(path, *content, options);
                // A file the disc already has is left as it is, and the
                // rest goes on: so does ManageDsk.
                if (result == AddResult::Exists) {
                    outcome.said.push_back(cpcName(path) + " is already on the disc: " + path + " was not added.");
                    continue;
                }
                if (result != AddResult::Done)
                    return fail(path + " cannot be added: " + why(result));
                changed = true;
                outcome.said.push_back("Added " + cpcName(path) + " (" + sizeSaid(content->size()) + ").");
            }
            break;
        }
        case 'I': {
            const ImportRequest request = parseImport(command.argument);
            if (!request.ok())
                return fail(request.error);
            const auto file = readFile(request.file);
            if (!file)
                return fail(request.file + " cannot be read");
            // What the file holds, whatever header it came with.
            const std::span<const uint8_t> data = withoutAmsdosHeader(*file);
            std::vector<uint8_t> content;
            if (request.type != ImportRequest::Type::Ascii)
                content = makeAmsdosHeader(cpcName(request.name),
                                           request.type == ImportRequest::Type::Basic ? kAmsdosBasic : kAmsdosBinary,
                                           static_cast<uint16_t>(request.start), static_cast<uint16_t>(request.entry),
                                           data.size());
            content.insert(content.end(), data.begin(), data.end());
            const AddResult result = disc->add(request.name, content, options);
            if (result == AddResult::Exists) {
                outcome.said.push_back(cpcName(request.name) + " is already on the disc: " + request.file
                                       + " was not added.");
                break;
            }
            if (result != AddResult::Done)
                return fail(request.file + " cannot be added: " + why(result));
            changed = true;
            outcome.said.push_back("Added " + cpcName(request.name) + " (" + sizeSaid(data.size()) + ").");
            break;
        }
        case 'S':
            if (!writeFile(command.argument, namesHfe(command.argument) ? hfeFile(disc->image()) : disc->image().bytes()))
                return fail(command.argument + " cannot be written");
            changed = false;
            outcome.said.push_back("Saved " + command.argument + ".");
            break;
        }
    }
    if (disc && changed)
        outcome.said.push_back("The disc was not saved: -S<disc> saves it.");
    return outcome;
}

}  // namespace tuxdsk::cli
