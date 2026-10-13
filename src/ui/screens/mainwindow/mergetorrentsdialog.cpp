// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <memory>
#include <utility>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "mergetorrentsdialog.h"
#include "rpc/rpc.h"
#include "rpc/torrent.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    MergeTorrentsDialog::MergeTorrentsDialog(const Torrent* source, const Torrent* target, Rpc* rpc, QWidget* parent)
        : QDialog(parent) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Merge Statistics"));

        auto layout = new QVBoxLayout(this);

        auto descriptionLabel = new QLabel(
            qApp->translate(
                "tremotesf",
                "Downloaded and uploaded amounts and downloading and seeding time of the old torrent will be added to "
                "the new torrent."
            ),
            this
        );
        descriptionLabel->setWordWrap(true);
        layout->addWidget(descriptionLabel);

        auto formLayout = new QFormLayout();
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        layout->addLayout(formLayout);

        auto sourceLabel = new QLabel(this);
        auto targetLabel = new QLabel(this);
        for (auto label : {sourceLabel, targetLabel}) {
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            label->setWordWrap(true);
        }
        //: Torrent whose statistics are merged into another one
        formLayout->addRow(qApp->translate("tremotesf", "Old torrent:"), sourceLabel);
        //: Torrent that receives statistics of another one
        formLayout->addRow(qApp->translate("tremotesf", "New torrent:"), targetLabel);

        struct Side {
            int id;
            QString name;
        };
        auto sides = std::make_shared<std::array<Side, 2>>(
            std::array{Side{source->data().id, source->data().name}, Side{target->data().id, target->data().name}}
        );
        const auto updateLabels = [=] {
            sourceLabel->setText((*sides)[0].name);
            targetLabel->setText((*sides)[1].name);
        };
        updateLabels();

        //: Button that exchanges old and new torrents
        auto swapButton =
            new QPushButton(QIcon::fromTheme("exchange-positions"_L1), qApp->translate("tremotesf", "S&wap"), this);
        swapButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QObject::connect(swapButton, &QPushButton::clicked, this, [=] {
            std::swap((*sides)[0], (*sides)[1]);
            updateLabels();
        });
        formLayout->addRow(QString(), swapButton);

        //: Check box label
        auto removeSourceCheckBox =
            new QCheckBox(qApp->translate("tremotesf", "Remove the old torrent (files are kept)"), this);
        removeSourceCheckBox->setChecked(true);
        layout->addWidget(removeSourceCheckBox);

        auto dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        //: Dialog confirmation button
        dialogButtonBox->button(QDialogButtonBox::Ok)->setText(qApp->translate("tremotesf", "Merge"));
        layout->addWidget(dialogButtonBox);

        QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, [=, this] {
            rpc->mergeTorrents(std::array{(*sides)[0].id}, (*sides)[1].id, removeSourceCheckBox->isChecked());
            accept();
        });
        QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        resize(sizeHint().expandedTo(QSize(512, 0)));
    }
}
