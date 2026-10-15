// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TORRENTFILESVIEW_H
#define TREMOTESF_TORRENTFILESVIEW_H

#include <functional>
#include <optional>

#include <QSet>
#include <QTreeView>

namespace tremotesf {
    class BaseTorrentFilesModel;
    class LocalTorrentFilesModel;
    class Rpc;
    class TorrentFilesModel;
    class TorrentFilesProxyModel;

    class TorrentFilesView final : public QTreeView {
        Q_OBJECT

    public:
        explicit TorrentFilesView(LocalTorrentFilesModel* model, Rpc* rpc, QWidget* parent = nullptr);
        explicit TorrentFilesView(TorrentFilesModel* model, Rpc* rpc, QWidget* parent = nullptr);
        ~TorrentFilesView() override = default;
        Q_DISABLE_COPY_MOVE(TorrentFilesView)

        static void showFileRenameDialog(
            const QString& fileName, QWidget* parent, const std::function<void(const QString&)>& onAccepted
        );

        void saveState();

    private:
        struct TreeState {
            std::optional<int> torrentId;
            QSet<QString> directories{};
            QSet<QString> expandedDirectories{};
            QSet<QString> selectedEntries{};
            int scrollPosition;
        };

        void init();
        void saveTreeState();
        void restoreTreeState(const TreeState& state);
        void onModelReset();
        void showContextMenu(QPoint pos);

        std::optional<int> torrentId() const;
        QString entryPath(const QModelIndex& index) const;

        bool mLocalFile;
        BaseTorrentFilesModel* mModel;
        TorrentFilesProxyModel* mProxyModel;
        Rpc* mRpc;
        // Torrent whose files are in the tree, which may already be gone from the model when it's reset
        std::optional<int> mTreeTorrentId{};
        std::optional<TreeState> mSavedTreeState{};
    };
}

#endif // TREMOTESF_TORRENTFILESVIEW_H
