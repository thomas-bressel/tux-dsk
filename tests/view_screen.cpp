// What the viewer shows as a screen: the bytes made pens in the three modes,
// laid out as the screen has them or flat, the CPC's colours, and what OCP
// Art Studio's files hold: packed screens, palettes and windows.

#include "check.h"
#include "view/screen.h"

using Bytes = std::vector<uint8_t>;

int main()
{
    using namespace tuxdsk;

    // ---- Colours ----
    CHECK(cpcColour(0).red == 0 && cpcColour(0).green == 0 && cpcColour(0).blue == 0);
    CHECK(cpcColour(1).blue == 0x7F && cpcColour(1).red == 0);    // blue
    CHECK(cpcColour(6).red == 0xFF && cpcColour(6).green == 0);   // bright red
    CHECK(cpcColour(18).green == 0xFF && cpcColour(18).red == 0); // bright green
    CHECK(cpcColour(24).red == 0xFF && cpcColour(24).green == 0xFF && cpcColour(24).blue == 0);  // bright yellow
    CHECK(cpcColour(26).red == 0xFF && cpcColour(26).green == 0xFF && cpcColour(26).blue == 0xFF);
    CHECK(cpcColour(99).red == 0 && cpcColour(-1).blue == 0);
    CHECK_EQ(inkOfHardware(0x54), 0);
    CHECK_EQ(inkOfHardware(0x44), 1);
    CHECK_EQ(inkOfHardware(0x4B), 26);
    CHECK_EQ(inkOfHardware(0x4A), 24);
    CHECK_EQ(inkOfHardware(0x53), 20);
    CHECK_EQ(inkOfHardware(0x0C), 6);   // without its &40
    CHECK_EQ(inkOfHardware(0x50), 1);   // the numbers that double others
    CHECK_EQ(inkOfHardware(0x41), 13);
    const Inks inks = defaultInks();
    CHECK(inks[0] == 1 && inks[1] == 24 && inks[2] == 20 && inks[3] == 6 && inks[15] == 16);

    // ---- A byte in each mode ----
    {
        ScreenView view;
        view.flat = true;
        view.columns = 1;
        view.lines = 1;
        const Bytes byte = {0b10110100};
        view.mode = 2;
        ScreenPicture picture = screenPicture(byte, view);
        CHECK(picture.width == 8 && picture.height == 1);
        CHECK(picture.pens == Bytes({1, 0, 1, 1, 0, 1, 0, 0}));
        // Mode 1: four pixels, a pen's low bit in the high half.
        view.mode = 1;
        picture = screenPicture(byte, view);
        CHECK(picture.pens == Bytes({1, 1, 2, 2, 1, 1, 1, 1}));
        // Mode 0: two pixels, their bits every other one.
        view.mode = 0;
        picture = screenPicture(byte, view);
        // bits 7,5,3,1 = 1,1,0,0: pen 1+4; bits 6,4,2,0 = 0,1,1,0: pen 4+2.
        CHECK(picture.pens == Bytes({5, 5, 5, 5, 6, 6, 6, 6}));
        picture = screenPicture(Bytes{0xFF}, view);
        CHECK(picture.pens == Bytes(8, 15));
    }

    // ---- The screen's own layout, and the flat one ----
    {
        Bytes memory(0x4000, 0);
        memory[0x0000] = 0x80;          // line 0
        memory[0x0800] = 0x40;          // line 1
        memory[0x3800] = 0x20;          // line 7
        memory[0x0050] = 0x10;          // line 8: the next row of characters
        memory[0x3800 + 24 * 80 + 79] = 0x01;  // line 199, last byte
        ScreenView view;
        view.mode = 2;
        ScreenPicture picture = screenPicture(memory, view);
        CHECK(picture.width == 640 && picture.height == 200);
        const auto pen = [&picture](int x, int y) { return picture.pens[static_cast<size_t>(y * picture.width + x)]; };
        CHECK(pen(0, 0) == 1 && pen(1, 1) == 1 && pen(2, 7) == 1 && pen(3, 8) == 1 && pen(639, 199) == 1);
        CHECK(pen(0, 1) == 0 && pen(0, 8) == 0);
        // Fewer columns and lines: the top left of it.
        view.columns = 40;
        view.lines = 16;
        picture = screenPicture(memory, view);
        CHECK(picture.width == 320 && picture.height == 16);
        CHECK(pen(3, 8) == 1);

        // Flat: one line after the other, from where one says.
        Bytes sprite = {0xEE, 0xEE, 0xEE, 0x80, 0x00, 0x00, 0x01, 0xFF, 0xFF};
        view = {};
        view.mode = 2;
        view.flat = true;
        view.columns = 2;
        view.lines = 3;
        view.offset = 3;
        picture = screenPicture(sprite, view);
        CHECK(picture.width == 16 && picture.height == 3);
        CHECK(pen(0, 0) == 1 && pen(1, 0) == 0 && pen(15, 1) == 1 && pen(0, 2) == 1 && pen(8, 2) == 1);
        // Past the end of the bytes there is nothing: pen 0.
        view.lines = 5;
        picture = screenPicture(sprite, view);
        CHECK(picture.height == 5 && pen(0, 3) == 0 && pen(15, 4) == 0);
        CHECK(screenPicture({}, view).pens == Bytes(16 * 5, 0));
    }

    // ---- OCP's packing ----
    {
        // "MJH", the length it unpacks to, bytes; 1 announces a run.
        const Bytes packed = {'M', 'J', 'H', 8, 0, 0xAA, 1, 5, 0x55, 0xBB, 0xCC, 'M', 'J', 'H', 3, 0, 1, 0, 0x11};
        Bytes expected = {0xAA, 0x55, 0x55, 0x55, 0x55, 0x55, 0xBB, 0xCC};
        expected.insert(expected.end(), 3, 0x11);  // a count of 0 is 256, cut at the block's length
        CHECK(screenMemory(packed) == expected);
        const Bytes plain = {1, 2, 3, 4};
        CHECK(screenMemory(plain) == plain);
        CHECK(screenMemory({}).empty());
        CHECK(screenMemory(Bytes{'M', 'J', 'H'}).empty());
    }

    // ---- The look a file carries ----
    {
        Bytes screen(0x4000, 0);
        CHECK(!lookInScreen(screen).has_value());
        const uint8_t mark[4] = {0x3A, 0xD0, 0xD7, 0xCD};
        std::copy(mark, mark + 4, screen.begin() + 0x7D0);
        screen[0x17D0] = 0;
        for (size_t ink = 0; ink < 16; ++ink)
            screen[0x17D1 + ink] = static_cast<uint8_t>(26 - ink);
        const auto look = lookInScreen(screen);
        CHECK(look && look->mode == 0 && look->inks[0] == 26 && look->inks[15] == 11);
        CHECK(!lookInScreen(Bytes(100, 0)).has_value());

        // A palette: the mode, two bytes, twelve Gate Array numbers an ink.
        Bytes palette(3 + 17 * 12, 0x54);
        palette[0] = 1;
        palette[3] = 0x44;           // ink 0: blue
        palette[3 + 12] = 0x4A;      // ink 1: bright yellow
        palette[3 + 15 * 12] = 0x4B; // ink 15: bright white
        const auto fromPalette = lookOfPalette(palette);
        CHECK(fromPalette && fromPalette->mode == 1 && fromPalette->inks[0] == 1 && fromPalette->inks[1] == 24);
        CHECK(fromPalette && fromPalette->inks[2] == 0 && fromPalette->inks[15] == 26);
        CHECK(!lookOfPalette(Bytes(50, 0)).has_value());
    }

    // ---- Windows ----
    {
        const Bytes data = {1, 2, 3, 4, 5, 6};
        const Bytes file = windowFile(data, 2, 3);
        CHECK(file == Bytes({1, 2, 3, 4, 5, 6, 0, 16, 0, 3, 3}));
        const auto window = readWindow(file);
        CHECK(window && window->data == data && window->lineBytes == 2 && window->height == 3);
        CHECK(!readWindow(Bytes{1, 2, 3}).has_value());
        CHECK(!readWindow(Bytes(20, 0)).has_value());
        // A window written from fewer bytes than it says is filled with 0.
        CHECK_EQ(windowFile(Bytes{9}, 4, 2).size(), 8 + 5);
        // Its width is in bits: one that is not whole bytes takes the next.
        const auto odd = readWindow(Bytes{1, 2, 3, 4, 0, 12, 0, 2, 3});
        CHECK(odd && odd->lineBytes == 2 && odd->height == 2);
    }

    return checkSummary("view_screen");
}
