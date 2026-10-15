// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "renametorrentfilesdialog.h"

#include <map>

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QTimer>

#include <KMessageWidget>

#include "coroutines/coroutines.h"
#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "rpc/torrent.h"
#include "ui/screens/addtorrent/localtorrentfilesmodel.h"
#include "ui/widgets/contentlayoutcombobox.h"
#include "ui/widgets/torrentfilesview.h"
#include "ui/widgets/torrentnameedit.h"

namespace tremotesf {
    RenameTorrentFilesDialog::RenameTorrentFilesDialog(Torrent* torrent, Rpc* rpc, QWidget* parent)
        : QDialog(parent),
          mRpc(rpc),
          mTorrentHash(torrent->data().hashString),
          mFilesModel(new LocalTorrentFilesModel(this)),
          mSaveStateHandler(this, [this] { mFilesView->saveState(); }) {
        setupUi();
        resize(sizeHint().expandedTo(QSize(512, 512)));
        mLoadFilesCoroutineScope.launch(loadFiles(torrent->data().id, torrent->data().name));
    }

    void RenameTorrentFilesDialog::accept() {
        if (auto* const torrent = this->torrent(); torrent) {
            if (auto filePaths = mFilesModel->filePaths(); !filePaths.empty()) {
                mRpc->renameTorrentFiles(torrent->data().id, std::move(filePaths), std::nullopt);
            }
            applyWantedAndPriority(torrent);
        }
        QDialog::accept();
    }

    void RenameTorrentFilesDialog::setupUi() {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Rename Torrent and Files"));

        auto* const layout = new QFormLayout(this);
        layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mMessageWidget = new KMessageWidget(this);
        mMessageWidget->setCloseButtonVisible(false);
        mMessageWidget->hide();
        layout->addRow(mMessageWidget);

        mNameEdit = new TorrentNameEdit(mFilesModel, this);
        //: Input field's label, name of torrent's top-level folder or file, which renames them
        layout->addRow(qApp->translate("tremotesf", "Name:"), mNameEdit);
        QObject::connect(mNameEdit, &TorrentNameEdit::stateChanged, this, &RenameTorrentFilesDialog::updateUi);

        mContentLayoutComboBox = new ContentLayoutComboBox(this);
        //: Content layout of added torrent: files stay where they are now
        mContentLayoutComboBox->setOriginalLayoutText(qApp->translate("tremotesf", "Current"));
        //: Combo box label
        layout->addRow(qApp->translate("tremotesf", "Content layout:"), mContentLayoutComboBox);
        QObject::connect(mContentLayoutComboBox, &QComboBox::currentIndexChanged, this, [this] {
            mFilesModel->setContentLayout(mContentLayoutComboBox->contentLayout());
        });

        mFilesView = new TorrentFilesView(mFilesModel, mRpc, this);
        layout->addRow(mFilesView);

        mDialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QObject::connect(mDialogButtonBox, &QDialogButtonBox::accepted, this, &RenameTorrentFilesDialog::accept);
        QObject::connect(mDialogButtonBox, &QDialogButtonBox::rejected, this, &RenameTorrentFilesDialog::reject);
        layout->addRow(mDialogButtonBox);

        QObject::connect(mRpc, &Rpc::connectedChanged, this, &RenameTorrentFilesDialog::updateUi);
        QObject::connect(mRpc, &Rpc::torrentsUpdated, this, &RenameTorrentFilesDialog::updateUi);

        // Message widget is shown without animation until the dialog is visible
        QTimer::singleShot(0, this, &RenameTorrentFilesDialog::updateUi);
    }

    void RenameTorrentFilesDialog::updateUi() {
        const bool connected = mRpc->isConnected();
        const bool hasTorrent = torrent() != nullptr;
        const bool enabled = connected && hasTorrent && mFilesModel->isLoaded();
        mContentLayoutComboBox->setEnabled(enabled);
        mFilesView->setEnabled(enabled);
        mNameEdit->setEnabled(enabled && mNameEdit->hasTitle());

        if (enabled) {
            if (mMessageWidget->isShowAnimationRunning()) {
                mMessageWidget->hide();
            } else {
                mMessageWidget->animatedHide();
            }
        } else {
            if (!connected) {
                mMessageWidget->setMessageType(KMessageWidget::Warning);
                //: Server connection status
                mMessageWidget->setText(qApp->translate("tremotesf", "Disconnected"));
            } else if (!hasTorrent) {
                mMessageWidget->setMessageType(KMessageWidget::Warning);
                //: Message that appears when torrent is removed
                mMessageWidget->setText(qApp->translate("tremotesf", "Torrent Removed"));
            } else if (mLoadingFailed) {
                mMessageWidget->setMessageType(KMessageWidget::Error);
                mMessageWidget->setText(qApp->translate("tremotesf", "Error loading torrent's files"));
            } else {
                mMessageWidget->setMessageType(KMessageWidget::Information);
                //: Placeholder shown while torrent's files are being loaded
                mMessageWidget->setText(qApp->translate("tremotesf", "Loading"));
            }
            mMessageWidget->animatedShow();
        }

        mDialogButtonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(enabled && mRpc->serverSettings()->data().canSetFilePaths() && !mNameEdit->hasError());
    }

    Coroutine<> RenameTorrentFilesDialog::loadFiles(int torrentId, QString torrentName) {
        auto files = co_await mRpc->fetchTorrentFiles(torrentId);
        if (!files) {
            mLoadingFailed = true;
            updateUi();
            co_return;
        }
        mFiles = *files;
        co_await mFilesModel->load(std::move(*files), std::move(torrentName));
        // Files named after the torrent get its name right away, as if it was typed
        mNameEdit->renameFiles();
        updateUi();
    }

    Torrent* RenameTorrentFilesDialog::torrent() const { return mRpc->torrentByHash(mTorrentHash); }

    void RenameTorrentFilesDialog::applyWantedAndPriority(Torrent* torrent) const {
        std::vector<bool> wanted(mFiles.size(), true);
        for (const int id : mFilesModel->unwantedFiles()) {
            wanted.at(static_cast<size_t>(id)) = false;
        }
        std::vector priorities(mFiles.size(), TorrentFile::Priority::Normal);
        for (const int id : mFilesModel->highPriorityFiles()) {
            priorities.at(static_cast<size_t>(id)) = TorrentFile::Priority::High;
        }
        for (const int id : mFilesModel->lowPriorityFiles()) {
            priorities.at(static_cast<size_t>(id)) = TorrentFile::Priority::Low;
        }

        std::vector<int> wantedFiles{};
        std::vector<int> unwantedFiles{};
        std::map<TorrentFile::Priority, std::vector<int>> filesByPriority{};
        for (size_t i = 0; i < mFiles.size(); ++i) {
            const auto id = static_cast<int>(i);
            if (wanted[i] != mFiles[i].wanted) {
                (wanted[i] ? wantedFiles : unwantedFiles).push_back(id);
            }
            if (priorities[i] != mFiles[i].priority) {
                filesByPriority[priorities[i]].push_back(id);
            }
        }
        if (!wantedFiles.empty()) {
            torrent->setFilesWanted(wantedFiles, true);
        }
        if (!unwantedFiles.empty()) {
            torrent->setFilesWanted(unwantedFiles, false);
        }
        for (const auto& [priority, ids] : filesByPriority) {
            torrent->setFilesPriority(ids, priority);
        }
    }
}
