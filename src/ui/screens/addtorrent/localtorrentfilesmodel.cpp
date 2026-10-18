// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "localtorrentfilesmodel.h"

#include <algorithm>
#include <iterator>
#include <ranges>

#include <QBrush>
#include <QGuiApplication>
#include <QPalette>

#include "addtorrenthelpers.h"
#include "coroutines/threadpool.h"
#include "rpc/torrentfilepaths.h"
#include "ui/itemmodels/torrentfilesmodelentry.h"
#include "ui/itemmodels/torrentfilestreebuilder.h"
#include "torrentfileparser.h"

namespace tremotesf {
    namespace {
        TorrentFilesTree createTree(TorrentMetainfoFile torrentFile) {
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

    QVariant LocalTorrentFilesModel::data(const QModelIndex& index, int role) const {
        if (role == Qt::BackgroundRole && index.isValid()) {
            const auto* const entry = static_cast<const TorrentFilesModelEntry*>(index.internalPointer());
            if (!entry->isDirectory() && mHighlightedFiles.contains(entry->fileId())) {
                // Same as background of KMessageWidget's information message
                QColor color = QGuiApplication::palette().color(QPalette::Highlight);
                color.setAlphaF(0.2f);
                return QBrush(color);
            }
        }
        return BaseTorrentFilesModel::data(index, role);
    }

    Coroutine<> LocalTorrentFilesModel::load(TorrentMetainfoFile torrentFile) {
        beginResetModel();
        mTorrentName = torrentFile.rootFileName;
        try {
            finishLoading(co_await runOnThreadPool(&createTree, std::move(torrentFile)));
        } catch (const bencode::Error&) {
            endResetModel();
            throw;
        }
    }

    Coroutine<> LocalTorrentFilesModel::load(std::vector<TorrentFile> files, QString torrentName) {
        beginResetModel();
        mTorrentName = std::move(torrentName);
        finishLoading(co_await runOnThreadPool([files = std::move(files)] { return createTorrentFilesTree(files); }));
    }

    void LocalTorrentFilesModel::finishLoading(TorrentFilesTree tree) {
        mRootEntry = std::move(tree.rootEntry);
        mFiles = std::move(tree.files);
        mInitialFilePaths =
            mFiles | std::views::transform(&TorrentFilesModelEntry::path) | std::ranges::to<std::vector>();
        mLoaded = true;
        endResetModel();
        if (mContentLayout != Settings::TorrentContentLayout::Original) {
            layOutFiles();
        } else {
            emit filePathsChanged();
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
        return changedFilePaths(mInitialFilePaths, currentFilePaths());
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

    void LocalTorrentFilesModel::setFilePaths(std::vector<QString> paths) {
        applyFilePaths(paths);
        mFilePathsSet = true;
    }

    void LocalTorrentFilesModel::discardSetFilePaths() {
        if (mFilePathsSet) {
            layOutFiles();
        }
    }

    const std::set<int>& LocalTorrentFilesModel::highlightedFiles() const { return mHighlightedFiles; }

    void LocalTorrentFilesModel::setHighlightedFiles(std::set<int> fileIds) {
        std::vector<int> changedFiles{};
        std::ranges::set_symmetric_difference(mHighlightedFiles, fileIds, std::back_inserter(changedFiles));
        mHighlightedFiles = std::move(fileIds);
        updateFiles(changedFiles, [](size_t, TorrentFilesModelEntry*) {});
    }

    void LocalTorrentFilesModel::layOutFiles() {
        setHighlightedFiles({});
        const auto laidOutPaths = applyContentLayout(mInitialFilePaths, mContentLayout, mTorrentName);
        applyFilePaths(laidOutPaths ? *laidOutPaths : mInitialFilePaths);
        mFilePathsSet = false;
    }

    void LocalTorrentFilesModel::applyFilePaths(std::span<const QString> paths) {
        mRenamedFiles.clear();
        if (renameEntries(paths)) {
            emit filePathsChanged();
            return;
        }

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
                file->completedSize(),
                file->wantedState() != TorrentFilesModelEntry::WantedState::Unwanted,
                file->priority()
            );
        }
        builder.calculateDirectoriesRecursively();

        beginResetModel();
        mRootEntry = std::move(builder.rootEntry);
        mFiles = std::move(builder.files);
        endResetModel();
        emit filePathsChanged();
    }

    void LocalTorrentFilesModel::renameFile(const QModelIndex& index, const QString& newName) {
        auto entry = static_cast<TorrentFilesModelEntry*>(index.internalPointer());
        mRenamedFiles.emplace(entry->path(), newName);
        fileRenamed(entry, newName);
        emit filePathsChanged();
    }
}
