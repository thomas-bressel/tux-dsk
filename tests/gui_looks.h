#pragma once

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QTest>
#include <QWidget>

// TuxAPE's two looks, for the tests that save pictures of a window: the
// plugin's windows take their colours from the palette TuxAPE sets, light
// or dark (its src/app/theme.cpp), and are to be looked at in both.

inline QPalette tuxapeLook(bool dark)
{
    const auto pick = [dark](const char* light, const char* darkOne) { return QColor(dark ? darkOne : light); };
    QPalette look;
    look.setColor(QPalette::Window, pick("#F0F0F0", "#1E1E1E"));
    look.setColor(QPalette::WindowText, pick("#000000", "#FFFFFF"));
    look.setColor(QPalette::Base, pick("#FFFFFF", "#121212"));
    look.setColor(QPalette::AlternateBase, pick("#F2F2F2", "#1C1C1C"));
    look.setColor(QPalette::Text, pick("#000000", "#FFFFFF"));
    look.setColor(QPalette::Button, pick("#E6E6E6", "#333333"));
    look.setColor(QPalette::ButtonText, pick("#000000", "#FFFFFF"));
    look.setColor(QPalette::BrightText, pick("#FFFFFF", "#FFFFFF"));
    look.setColor(QPalette::ToolTipBase, pick("#FFFFE0", "#333333"));
    look.setColor(QPalette::ToolTipText, pick("#000000", "#FFFFFF"));
    look.setColor(QPalette::PlaceholderText, pick("#595959", "#A6A6A6"));
    look.setColor(QPalette::Highlight, pick("#0B3D91", "#9CC8FF"));
    look.setColor(QPalette::HighlightedText, pick("#FFFFFF", "#000000"));
    look.setColor(QPalette::Link, pick("#0000A0", "#9CC8FF"));
    look.setColor(QPalette::LinkVisited, pick("#5A0080", "#D7A8FF"));
    look.setColor(QPalette::Light, pick("#FFFFFF", "#5A5A5A"));
    look.setColor(QPalette::Midlight, pick("#F7F7F7", "#454545"));
    look.setColor(QPalette::Mid, pick("#8C8C8C", "#5A5A5A"));
    look.setColor(QPalette::Dark, pick("#6E6E6E", "#101010"));
    look.setColor(QPalette::Shadow, pick("#000000", "#000000"));
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        look.setColor(QPalette::Disabled, role, pick("#6E6E6E", "#8C8C8C"));
    look.setColor(QPalette::Disabled, QPalette::Highlight, pick("#B0B0B0", "#4A4A4A"));
    look.setColor(QPalette::Disabled, QPalette::HighlightedText, pick("#000000", "#FFFFFF"));
    return look;
}

// A picture of a window in each look: <path>.png in the light one and
// <path>_dark.png in the dark one. The application is left with the look it
// had. False if a picture could not be saved.
inline bool saveInBothLooks(QWidget* window, const QString& path)
{
    const QPalette before = QApplication::palette();
    bool saved = true;
    for (const bool dark : {false, true}) {
        QApplication::setPalette(tuxapeLook(dark));
        // The windows are drawn again when they next have their turn.
        QTest::qWait(60);
        saved = window->grab().save(path + (dark ? QStringLiteral("_dark.png") : QStringLiteral(".png"))) && saved;
    }
    QApplication::setPalette(before);
    QTest::qWait(20);
    return saved;
}
