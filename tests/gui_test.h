#pragma once

#include <deque>
#include <functional>

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTimer>

#include "check.h"
#include "golden.h"
#include "prompts.h"

// What the tests of the windows share: the boxes that tell and ask, answered
// by the test, and the windows that ask something more, filled in by it.

namespace gui {

// What the windows told, and what they asked, in order.
inline QStringList g_told;
inline QStringList g_asked;
// The answers to give, in order, each with whether the question's box is
// ticked. A question nothing was planned for is answered Cancel.
struct Planned {
    prompts::Answer answer;
    bool ticked = false;
    std::function<void()> then;  // what the test does once the answer is given
};
inline std::deque<Planned> g_answers;

inline void answerBoxes()
{
    prompts::setProblemHandler([](const QString& text) { g_told << text; });
    prompts::setAnswerer([](const prompts::Question& question, bool* ticked) {
        g_asked << question.text;
        if (g_answers.empty()) {
            std::printf("a question nothing was planned for: %s\n", qPrintable(question.text));
            ++g_failures;
            return prompts::Answer::Cancel;
        }
        const Planned planned = g_answers.front();
        g_answers.pop_front();
        if (ticked)
            *ticked = planned.ticked;
        if (planned.then)
            planned.then();
        return planned.answer;
    });
}

inline void plan(prompts::Answer answer, bool ticked = false, std::function<void()> then = {})
{
    g_answers.push_back({answer, ticked, std::move(then)});
}

// The next window that asks (one shown with exec()) is given to `fill`,
// which fills it in and closes it.
inline void whenAsked(std::function<void(QDialog*)> fill)
{
    QTimer::singleShot(0, [fill = std::move(fill)] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) {
            std::printf("no window asked anything\n");
            ++g_failures;
            return;
        }
        fill(dialog);
    });
}

inline QString inputFile(const char* name)
{
    return QString::fromStdString(golden::inputPath(name));
}

inline std::vector<uint8_t> fileBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray all = file.readAll();
    return std::vector<uint8_t>(all.begin(), all.end());
}

}  // namespace gui
