#pragma once

#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QPushButton>
#include <QSpinBox>
#include <QString>

// What the controls of TuxDSK's windows are made with.

// The font of what is set in fixed-width type: file names, bytes, listings.
inline QFont fixedFont()
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

// A button of a window. Return does not press it: a window's buttons are
// many, and none of them is the window's own.
inline QPushButton* pageButton(const QString& text, const char* objectName)
{
    auto* button = new QPushButton(text);
    button->setObjectName(objectName);
    button->setAutoDefault(false);
    return button;
}

// An address or a number of the CPC: shown in hexadecimal after '&', and
// typed that way, after '#' or "0x", or in decimal.
class AddressBox : public QSpinBox {
public:
    explicit AddressBox(int maximum = 0xFFFF, QWidget* parent = nullptr)
        : QSpinBox(parent)
    {
        setRange(0, maximum);
        setFont(fixedFont());
        setAccelerated(true);
    }

protected:
    QString textFromValue(int value) const override
    {
        return QStringLiteral("&%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
    }

    int valueFromText(const QString& text) const override
    {
        QString digits = text.trimmed();
        int base = 10;
        if (digits.startsWith('&') || digits.startsWith('#')) {
            digits.remove(0, 1);
            base = 16;
        } else if (digits.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
            digits.remove(0, 2);
            base = 16;
        }
        return digits.toInt(nullptr, base);
    }

    QValidator::State validate(QString& text, int&) const override
    {
        const QString typed = text.trimmed();
        if (typed.isEmpty() || typed == QLatin1String("&") || typed == QLatin1String("#")
            || typed.compare(QLatin1String("0x"), Qt::CaseInsensitive) == 0)
            return QValidator::Intermediate;
        bool hexadecimal = typed.startsWith('&') || typed.startsWith('#');
        QString digits = typed.mid(hexadecimal ? 1 : 0);
        if (!hexadecimal && digits.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
            digits.remove(0, 2);
            hexadecimal = true;
        }
        bool ok = false;
        const int value = digits.toInt(&ok, hexadecimal ? 16 : 10);
        return ok && value >= minimum() && value <= maximum() ? QValidator::Acceptable : QValidator::Invalid;
    }
};

// A folder as a window shows it, with ~ for the user's own.
inline QString shownPath(const QString& path)
{
    QString shown = QDir::cleanPath(path);
#ifndef Q_OS_WIN
    const QString home = QDir::homePath();
    if (home.size() > 1 && (shown == home || shown.startsWith(home + QLatin1Char('/'))))
        shown.replace(0, home.size(), QStringLiteral("~"));
#endif
    return QDir::toNativeSeparators(shown);
}
