// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TEXTINPUTDIALOG_H
#define TREMOTESF_TEXTINPUTDIALOG_H

#include <functional>

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class KMessageWidget;

namespace tremotesf {
    class TextInputDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit TextInputDialog(
            const QString& title,
            const QString& labelText,
            const QString& text,
            const QString& okButtonText,
            bool multiline,
            QWidget* parent = nullptr
        );
        QString text() const;
        // Text field of a single line dialog
        QLineEdit* lineEdit() const { return mLineEdit; }

        struct CheckResult {
            bool acceptable;
            // Shown under the text field, as an error if the text is not acceptable
            QString message{};
        };
        using Check = std::function<CheckResult(const QString& text)>;

        /**
         * Checks the text whenever it changes, instead of only requiring it to be non-empty
         */
        void setCheck(Check check);

    private:
        void checkText();

        QLineEdit* mLineEdit = nullptr;
        QPlainTextEdit* mPlainTextEdit = nullptr;
        KMessageWidget* mMessageWidget = nullptr;
        QPushButton* mOkButton = nullptr;
        Check mCheck{};
    };
}

#endif
