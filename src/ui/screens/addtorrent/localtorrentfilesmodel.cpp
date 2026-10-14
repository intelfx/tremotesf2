// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "localtorrentfilesmodel.h"

#include <ranges>

#include "addtorrenthelpers.h"
#include "coroutines/threadpool.h"
#include "ui/itemmodels/torrentfilesmodelentry.h"
#include "ui/itemmodels/torrentfilestreebuilder.h"
#include "torrentfileparser.h"

namespace tremotesf {
    namespace {
        struct CreateTreeResult {
            std::unique_ptr<TorrentFilesModelEntry> rootEntry;
            std::vector<TorrentFilesModelEntry*> files;
        };

        CreateTreeResult createTree(TorrentMetainfoFile torrentFile) {
            if (torrentFile.isSingleFile() == 1) {
                auto rootEntry = std::make_unique<TorrentFilesModelEntry>(TorrentFilesModelEntry::createFile(
                    0,
                    0,
                    std::move(torrentFile.rootFileName),
                    torrentFile.singleFileSize(),
                    0,
                    true,
                    TorrentFilesModelEntry::Priority::Normal
                ));
                auto rootEntryPtr = rootEntry.get();
                return {.rootEntry = std::move(rootEntry), .files = {rootEntryPtr}};
            }

            auto files = torrentFile.files();
            TorrentFilesTreeBuilder builder(files.size());
            builder.initializeRootDirectory(std::move(torrentFile.rootFileName));
            for (TorrentMetainfoFile::File file : files) {
                builder.addFile(file.path(), false, file.size, 0, true, TorrentFilesModelEntry::Priority::Normal);
            }
            builder.calculateDirectoriesRecursively();
            return {.rootEntry = std::move(builder.rootEntry), .files = std::move(builder.files)};
        }
    }

    LocalTorrentFilesModel::LocalTorrentFilesModel(QObject* parent)
        : BaseTorrentFilesModel({Column::Name, Column::Size, Column::Priority}, parent) {}

    Coroutine<> LocalTorrentFilesModel::load(TorrentMetainfoFile torrentFile) {
        beginResetModel();
        try {
            auto [rootEntry, files] = co_await runOnThreadPool(&createTree, std::move(torrentFile));
            mRootEntry = std::move(rootEntry);
            mFiles = std::move(files);
            mTorrentFilePaths =
                mFiles | std::views::transform(&TorrentFilesModelEntry::path) | std::ranges::to<std::vector>();
            mLoaded = true;
            endResetModel();
            if (mContentLayout != Settings::TorrentContentLayout::Original) {
                layOutFiles();
            }
        } catch (const bencode::Error&) {
            endResetModel();
            throw;
        }
    }

    bool LocalTorrentFilesModel::isLoaded() const { return mLoaded; }

    std::vector<int> LocalTorrentFilesModel::unwantedFiles() const {
        std::vector<int> files;
        for (const TorrentFilesModelEntry* file : mFiles) {
            if (file->wantedState() == TorrentFilesModelEntry::WantedState::Unwanted) {
                files.push_back(file->fileId());
            }
        }
        return files;
    }

    std::vector<int> LocalTorrentFilesModel::highPriorityFiles() const {
        std::vector<int> files;
        for (const TorrentFilesModelEntry* file : mFiles) {
            if (file->priority() == TorrentFilesModelEntry::Priority::High) {
                files.push_back(file->fileId());
            }
        }
        return files;
    }

    std::vector<int> LocalTorrentFilesModel::lowPriorityFiles() const {
        std::vector<int> files;
        for (const TorrentFilesModelEntry* file : mFiles) {
            if (file->priority() == TorrentFilesModelEntry::Priority::Low) {
                files.push_back(file->fileId());
            }
        }
        return files;
    }

    const std::map<QString, QString>& LocalTorrentFilesModel::renamedFiles() const { return mRenamedFiles; }

    std::vector<std::optional<QString>> LocalTorrentFilesModel::filePaths() const {
        std::vector<std::optional<QString>> paths{};
        paths.reserve(mFiles.size());
        bool anyChanged = false;
        for (const auto& [file, torrentFilePath] : std::views::zip(mFiles, mTorrentFilePaths)) {
            auto path = file->path();
            if (path == torrentFilePath) {
                paths.emplace_back();
            } else {
                paths.emplace_back(std::move(path));
                anyChanged = true;
            }
        }
        if (!anyChanged) {
            paths.clear();
        }
        return paths;
    }

    void LocalTorrentFilesModel::setContentLayout(Settings::TorrentContentLayout layout) {
        if (layout == mContentLayout) {
            return;
        }
        mContentLayout = layout;
        if (mLoaded) {
            layOutFiles();
        }
    }

    void LocalTorrentFilesModel::layOutFiles() {
        const auto laidOutPaths = applyContentLayout(mTorrentFilePaths, mContentLayout);
        const auto& paths = laidOutPaths ? *laidOutPaths : mTorrentFilePaths;

        TorrentFilesTreeBuilder builder(paths.size());
        const bool hasSingleTopLevelEntry = TorrentFilesTreeBuilder::hasSingleTopLevelEntry(paths);
        if (!hasSingleTopLevelEntry) {
            builder.initializeRootDirectory({});
        }
        for (const auto& [path, file] : std::views::zip(paths, mFiles)) {
            builder.addFile(
                path.tokenize(u'/', Qt::SkipEmptyParts),
                hasSingleTopLevelEntry,
                file->size(),
                0,
                file->wantedState() != TorrentFilesModelEntry::WantedState::Unwanted,
                file->priority()
            );
        }
        builder.calculateDirectoriesRecursively();

        beginResetModel();
        mRootEntry = std::move(builder.rootEntry);
        mFiles = std::move(builder.files);
        mRenamedFiles.clear();
        endResetModel();
    }

    void LocalTorrentFilesModel::renameFile(const QModelIndex& index, const QString& newName) {
        auto entry = static_cast<TorrentFilesModelEntry*>(index.internalPointer());
        mRenamedFiles.emplace(entry->path(), newName);
        fileRenamed(entry, newName);
    }
}
