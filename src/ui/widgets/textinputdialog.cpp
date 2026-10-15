// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textinputdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <KMessageWidget>

namespace tremotesf {
    TextInputDialog::TextInputDialog(
        const QString& title,
        const QString& labelText,
        const QString& text,
        const QString& okButtonText,
        bool multiline,
        QWidget* parent
    )
        : QDialog(parent) {
        setWindowTitle(title);

        auto layout = new QVBoxLayout(this);
        layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

        auto label = new QLabel(labelText, this);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        layout->addWidget(label);

        if (multiline) {
            mPlainTextEdit = new QPlainTextEdit(text, this);
            layout->addWidget(mPlainTextEdit);
        } else {
            mLineEdit = new QLineEdit(text, this);
            layout->addWidget(mLineEdit);
        }

        mMessageWidget = new KMessageWidget(this);
        mMessageWidget->setCloseButtonVisible(false);
        mMessageWidget->setWordWrap(true);
        mMessageWidget->hide();
        layout->addWidget(mMessageWidget);

        auto dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        mOkButton = dialogButtonBox->button(QDialogButtonBox::Ok);
        if (!okButtonText.isEmpty()) {
            mOkButton->setText(okButtonText);
        }
        QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, &TextInputDialog::accept);
        QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &TextInputDialog::reject);

        if (multiline) {
            QObject::connect(mPlainTextEdit, &QPlainTextEdit::textChanged, this, &TextInputDialog::checkText);
        } else {
            QObject::connect(mLineEdit, &QLineEdit::textChanged, this, &TextInputDialog::checkText);
        }
        checkText();

        layout->addWidget(dialogButtonBox);

        resize(sizeHint().expandedTo(QSize(720, 0)));
    }

    QString TextInputDialog::text() const { return mLineEdit ? mLineEdit->text() : mPlainTextEdit->toPlainText(); }

    void TextInputDialog::setCheck(Check check) {
        mCheck = std::move(check);
        checkText();
    }

    void TextInputDialog::checkText() {
        const QString text = this->text();
        const auto [acceptable, message] = mCheck ? mCheck(text) : CheckResult{.acceptable = !text.isEmpty()};
        mOkButton->setEnabled(acceptable);
        if (message.isEmpty()) {
            mMessageWidget->hide();
        } else {
            mMessageWidget->setMessageType(acceptable ? KMessageWidget::Information : KMessageWidget::Error);
            mMessageWidget->setText(message);
            mMessageWidget->show();
        }
    }
}
