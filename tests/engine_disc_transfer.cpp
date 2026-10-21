// A file from the computer to a disc and back, in ManageDsk's modes: as it
// is, as ASCII, as a binary; with its header or without.

#include "check.h"
#include "disc/transfer.h"
#include "golden.h"

using golden::Bytes;

int main()
{
    using namespace tuxdsk;
    const Bytes raw = golden::input("code.bin");         // 300 bytes, no header
    const Bytes headed = golden::input("withhdr.bin");   // a header, with rubbish in it, and 300 bytes
    const Bytes data(headed.begin() + 128, headed.end());

    // ---- To a disc ----
    ImportOptions options;
    CHECK(importedFile("A.BIN", raw, options) == raw);
    CHECK(importedFile("A.BIN", headed, options) == headed);

    options.kind = ImportKind::Ascii;
    CHECK(importedFile("A.TXT", raw, options) == raw);
    CHECK(importedFile("A.TXT", headed, options) == data);

    options.kind = ImportKind::Binary;
    // A file with a header keeps it...
    CHECK(importedFile("A.BIN", headed, options) == headed);
    // ...with other addresses if they are given...
    options.setAddresses = true;
    options.loadAddress = 0x4000;
    options.entryAddress = 0x4003;
    Bytes moved = importedFile("A.BIN", headed, options);
    auto header = amsdosHeader(moved);
    CHECK(header && header->name == "WITHHDR.BIN" && header->loadAddress == 0x4000 && header->entryAddress == 0x4003);
    CHECK(header && header->length == 300 && moved.size() == headed.size() && moved[0x1C] == 0xAA);
    // ...and one without is given one.
    Bytes made = importedFile("new.bin", raw, options);
    header = amsdosHeader(made);
    CHECK_EQ(made.size(), 428);
    CHECK(header && header->name == "NEW.BIN" && header->type == kAmsdosBinary && header->loadAddress == 0x4000);
    CHECK(header && header->entryAddress == 0x4003 && header->length == 300);
    CHECK(std::equal(raw.begin(), raw.end(), made.begin() + 128));
    options.type = kAmsdosBasic;
    options.loadAddress = 0x170;
    header = amsdosHeader(importedFile("p.bas", raw, options));
    CHECK(header && header->type == kAmsdosBasic && header->loadAddress == 0x170);
    // The clean-up, on the way in.
    options = {};
    options.cleanHeader = true;
    const Bytes cleaned = importedFile("A.BIN", headed, options);
    CHECK(hasAmsdosHeader(cleaned) && cleaned[0x1C] == 0 && cleaned[0x7F] == 0 && cleaned.size() == headed.size());
    CHECK(importedFile("A.BIN", raw, options) == raw);

    // ---- From a disc: the records, 512 bytes for a file of 428 ----
    Bytes records = headed;
    records.resize(512, 0xEE);
    CHECK(exportedFile(records, true) == headed);
    CHECK(exportedFile(records, false) == data);
    const Bytes clean = exportedFile(records, true, true);
    CHECK(clean.size() == 428 && clean[0x1C] == 0 && hasAmsdosHeader(clean));
    CHECK(exportedFile(records, false, true) == data);
    // A file without a header is all its records, either way.
    Bytes plain = raw;
    plain.resize(384, 0);
    CHECK(exportedFile(plain, true) == plain);
    CHECK(exportedFile(plain, false) == plain);
    // A header that says more than there is, or nothing: what there is.
    Bytes lying = makeAmsdosHeader("L.BIN", kAmsdosBinary, 0, 0, 5000);
    lying.resize(256, 7);
    CHECK_EQ(exportedFile(lying, true).size(), 256);
    CHECK_EQ(exportedFile(lying, false).size(), 128);
    Bytes silent = makeAmsdosHeader("S.BIN", kAmsdosBinary, 0x100, 0, 0);
    silent.resize(256, 7);
    CHECK_EQ(exportedFile(silent, false).size(), 128);
    CHECK(exportedFile({}, true).empty());

    return checkSummary("engine_disc_transfer");
}
