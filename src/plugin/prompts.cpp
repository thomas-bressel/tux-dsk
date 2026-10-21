#include "prompts.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>

namespace tuxdsk::prompts {

namespace {

std::function<void(const QString&)> g_problemHandler;
std::function<Answer(const Question&, bool*)> g_answerer;

}  // namespace

void problem(QWidget* parent, const QString& text)
{
    if (g_problemHandler) {
        g_problemHandler(text);
        return;
    }
    QMessageBox::warning(parent, QCoreApplication::translate("tuxdsk::prompts", "TuxDSK"), text);
}

void inform(QWidget* parent, const QString& text)
{
    if (g_problemHandler) {
        g_problemHandler(text);
        return;
    }
    QMessageBox::information(parent, QCoreApplication::translate("tuxdsk::prompts", "TuxDSK"), text);
}

Answer ask(QWidget* parent, const Question& question, bool* optionTicked)
{
    if (g_answerer)
        return g_answerer(question, optionTicked);
    QMessageBox box(QMessageBox::Question, question.title, question.text, QMessageBox::NoButton, parent);
    QPushButton* yes = box.addButton(QCoreApplication::translate("tuxdsk::prompts", "&Yes"), QMessageBox::YesRole);
    QPushButton* all = question.several
                           ? box.addButton(QCoreApplication::translate("tuxdsk::prompts", "Yes for &All"), QMessageBox::YesRole)
                           : nullptr;
    QPushButton* no = box.addButton(QCoreApplication::translate("tuxdsk::prompts", "&No"), QMessageBox::NoRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(no);
    QCheckBox* option = nullptr;
    if (!question.option.isEmpty()) {
        option = new QCheckBox(question.option);
        box.setCheckBox(option);
    }
    box.exec();
    if (optionTicked)
        *optionTicked = option && option->isChecked();
    if (box.clickedButton() == yes)
        return Answer::Yes;
    if (all && box.clickedButton() == all)
        return Answer::YesToAll;
    return box.clickedButton() == no ? Answer::No : Answer::Cancel;
}

void setProblemHandler(std::function<void(const QString&)> handler)
{
    g_problemHandler = std::move(handler);
}

void setAnswerer(std::function<Answer(const Question&, bool*)> answerer)
{
    g_answerer = std::move(answerer);
}

}  // namespace tuxdsk::prompts
