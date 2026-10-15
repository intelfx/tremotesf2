// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_LOCALTORRENTFILESMODEL_H
#define TREMOTESF_LOCALTORRENTFILESMODEL_H

#include <map>
#include <optional>
#include <set>
#include <span>
#include <vector>

#include "settings.h"
#include "ui/itemmodels/basetorrentfilesmodel.h"
#include "coroutines/coroutinefwd.h"

namespace tremotesf {
    class TorrentMetainfoFile;

    class LocalTorrentFilesModel final : public BaseTorrentFilesModel {
        Q_OBJECT

    public:
        explicit LocalTorrentFilesModel(QObject* parent = nullptr);

        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        /**
         * @throws bencode::Error
         */
        Coroutine<> load(TorrentMetainfoFile torrentFile);

        bool isLoaded() const;

        std::vector<int> unwantedFiles() const;
        std::vector<int> highPriorityFiles() const;
        std::vector<int> lowPriorityFiles() const;

        const std::map<QString, QString>& renamedFiles() const;

        /**
         * Path of each file, or std::nullopt for a file that has the same path as in the torrent file.
         * Empty if no file's path differs.
         */
        std::vector<std::optional<QString>> filePaths() const;

        // Lays out the files anew, which discards renamed files
        void setContentLayout(Settings::TorrentContentLayout layout);

        void renameFile(const QModelIndex& index, const QString& newName) override;
        void setFilePaths(std::vector<QString> paths) override;
        // Lays out the files anew if their paths were set with setFilePaths(), which only some servers can take
        void discardSetFilePaths();

        // Ids of files whose rows are highlighted. Laying out the files anew clears them.
        const std::set<int>& highlightedFiles() const;
        void setHighlightedFiles(std::set<int> fileIds);

    private:
        void layOutFiles();
        void applyFilePaths(std::span<const QString> paths);

        bool mLoaded{};
        bool mFilePathsSet{};
        Settings::TorrentContentLayout mContentLayout{Settings::TorrentContentLayout::Original};

        std::map<QString, QString> mRenamedFiles{};
        std::vector<QString> mTorrentFilePaths{};
        std::set<int> mHighlightedFiles{};

    signals:
        // Files were loaded, or got new paths
        void filePathsChanged();
    };
}

#endif // TREMOTESF_LOCALTORRENTFILESMODEL_H
