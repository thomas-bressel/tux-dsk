#pragma once

#include <functional>

#include <QString>

class QWidget;

// What TuxDSK's windows tell and ask the user in a box of its own: a
// problem, and ManageDsk's question about each of several files, which is
// answered Yes, Yes for all, No or Cancel.
//
// A test that drives the windows has no one to answer: it says what the
// answers are, and notes what was told.

namespace tuxdsk::prompts {

enum class Answer { Yes, YesToAll, No, Cancel };

struct Question {
    QString title;
    QString text;
    bool several = false;  // more files follow: "Yes for all" is offered
    QString option;        // a box to tick with the answer, if there is one
};

void problem(QWidget* parent, const QString& text);
// Something worth a box that is not a problem: what a comparison found.
void inform(QWidget* parent, const QString& text);
Answer ask(QWidget* parent, const Question& question, bool* optionTicked = nullptr);

// For tests: null puts the boxes back. The handler is given what problem()
// and inform() are.
void setProblemHandler(std::function<void(const QString&)> handler);
void setAnswerer(std::function<Answer(const Question&, bool*)> answerer);

}  // namespace tuxdsk::prompts
