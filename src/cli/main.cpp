// tuxdsk: ManageDsk's command line, done with TuxDSK's discs. The line is
// read, and what it asks for is done, one command after the other
// (commands.h).

#include <cstdio>
#include <string>
#include <vector>

#include "cli/commands.h"

int main(int argc, char* argv[])
{
    using namespace tuxdsk::cli;
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    const ParsedLine line = parseCommandLine(arguments);
    // ManageDsk shows its window when it is given nothing; this has none.
    if (line.usageAsked || arguments.empty()) {
        std::fputs(usageText().c_str(), stdout);
        return kStatusDone;
    }
    if (!line.ok()) {
        std::fprintf(stderr, "tuxdsk: %s\n", line.error.c_str());
        std::fputs("tuxdsk: -help lists the commands.\n", stderr);
        return kStatusBadLine;
    }

    const Outcome outcome = run(line.commands);
    for (const std::string& said : outcome.said)
        std::fprintf(stdout, "%s\n", said.c_str());
    std::fflush(stdout);
    if (outcome.status != kStatusDone)
        std::fprintf(stderr, "tuxdsk: %s\n", outcome.error.c_str());
    return outcome.status;
}
