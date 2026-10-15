// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RENAMETORRENTFILESDIALOG_H
#define TREMOTESF_RENAMETORRENTFILESDIALOG_H

#include <vector>

#include <QDialog>

#include "coroutines/scope.h"
#include "rpc/torrentfile.h"
#include "ui/savewindowstatedispatcher.h"

class QDialogButtonBox;

class KMessageWidget;

namespace tremotesf {
    class ContentLayoutComboBox;
    class LocalTorrentFilesModel;
    class Rpc;
    class Torrent;
    class TorrentFilesView;
    class TorrentNameEdit;

    // Renames, moves and selects files of an added torrent like the add torrent dialog does, and applies it all on OK
    class RenameTorrentFilesDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit RenameTorrentFilesDialog(Torrent* torrent, Rpc* rpc, QWidget* parent = nullptr);

        void accept() override;

    private:
        void setupUi();
        void updateUi();
        Coroutine<> loadFiles(int torrentId);
        // Torrent if it still exists
        Torrent* torrent() const;
        void applyWantedAndPriority(Torrent* torrent) const;

        Rpc* mRpc;
        QString mTorrentHash;
        LocalTorrentFilesModel* mFilesModel;
        // Files as they were loaded, to find which ones changed
        std::vector<TorrentFile> mFiles{};
        bool mLoadingFailed{};
        CoroutineScope mLoadFilesCoroutineScope{};

        KMessageWidget* mMessageWidget{};
        TorrentNameEdit* mNameEdit{};
        ContentLayoutComboBox* mContentLayoutComboBox{};
        TorrentFilesView* mFilesView{};
        QDialogButtonBox* mDialogButtonBox{};

        SaveWindowStateHandler mSaveStateHandler;
    };
}

#endif // TREMOTESF_RENAMETORRENTFILESDIALOG_H
