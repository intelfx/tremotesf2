// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TORRENTFILESVIEW_H
#define TREMOTESF_TORRENTFILESVIEW_H

#include <functional>
#include <optional>

#include <QSet>
#include <QTreeView>

#include "rpc/torrentfilepaths.h"
#include "textinputdialog.h"

class QMenu;

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

    protected:
        void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;
        void dataChanged(
            const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles = QList<int>()
        ) override;

    private:
        struct TreeState {
            std::optional<int> torrentId;
            QSet<QString> directories{};
            QSet<QString> expandedDirectories{};
            QSet<QString> selectedEntries{};
            int scrollPosition;
            bool scrollToSelection{};
        };

        void init();
        void saveTreeState();
        void restoreTreeState(const TreeState& state);
        void onModelReset();
        void showContextMenu(QPoint pos);
        void addMoveAction(QMenu& menu, const QModelIndexList& sourceIndexes);

        bool canSetFilePaths() const;
        // Paths of selected files and folders, without those that are inside selected folders
        std::vector<QString> selectedEntries(const QModelIndexList& sourceIndexes) const;
        void moveEntries(std::vector<PathMove> moves);
        // Check for a dialog whose text makes moves of torrent's files and folders
        TextInputDialog::Check checkMoves(std::function<std::vector<PathMove>(const QString&)> movesForText) const;
        void addFlattenAndExtractActions(QMenu& menu, const QModelIndexList& sourceIndexes);

        std::optional<int> torrentId() const;
        QString entryPath(const QModelIndex& index) const;

        bool mLocalFile;
        BaseTorrentFilesModel* mModel;
        TorrentFilesProxyModel* mProxyModel;
        Rpc* mRpc;
        // Torrent whose files are in the tree, which may already be gone from the model when it's reset
        std::optional<int> mTreeTorrentId{};
        std::optional<TreeState> mSavedTreeState{};
        // Moves that were sent to the model, to show where the entries went once the tree is rebuilt
        std::optional<std::vector<PathMove>> mPendingPathMoves{};
    };
}

#endif // TREMOTESF_TORRENTFILESVIEW_H
