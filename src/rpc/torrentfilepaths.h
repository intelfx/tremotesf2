// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_TORRENTFILEPATHS_H
#define TREMOTESF_RPC_TORRENTFILEPATHS_H

#include <optional>
#include <span>
#include <vector>

#include <QString>

#include "pathutils.h"

namespace tremotesf {
    struct TorrentFilesLayout {
        QString location;
        std::vector<QString> filePaths;

        bool operator==(const TorrentFilesLayout&) const = default;
    };

    // Moves a file, or a folder with everything in it, to a new path
    struct PathMove {
        QString from;
        QString to;

        bool operator==(const PathMove&) const = default;
    };

    /**
     * Paths that torrent's files get with the moves. A file follows the move with the longest `from`
     * that is either its path or a folder above it, e.g. with moves "A" -> "B" and "A/1" -> "C/1",
     * "A/1" becomes "C/1" and "A/2" becomes "B/2".
     */
    std::vector<QString> applyPathMoves(std::span<const QString> filePaths, std::span<const PathMove> moves);

    // Name of a file or a folder
    QString entryName(const QString& path);
    // Folder that holds a file or a folder, or an empty string for the top level
    QString parentFolder(const QString& path);

    struct FilePathsError {
        enum class Type {
            // Path is empty, or has an empty, "." or ".." component
            InvalidPath,
            // Several files have this path
            DuplicatePath,
            // This is a path of a file and a folder of another file at the same time
            FileAndFolder
        };
        Type type;
        QString path;

        bool operator==(const FilePathsError&) const = default;
    };

    // Checks that paths of torrent's files are valid and don't clash, which servers require
    std::optional<FilePathsError> checkFilePaths(std::span<const QString> filePaths);

    QString filePathsErrorString(const FilePathsError& error);

    /**
     * Paths of entries that are not inside other entries, e.g. "A" and "B/1" of "A", "A/1" and "B/1".
     * Duplicates are dropped as well.
     */
    std::vector<QString> topmostEntries(std::span<const QString> entryPaths);

    /**
     * New path of each file, or std::nullopt for a file whose path doesn't change, as servers take them
     * @return Empty if no file's path changes
     */
    std::vector<std::optional<QString>>
    changedFilePaths(std::span<const QString> filePaths, std::span<const QString> newFilePaths);

    /**
     * Removes the top-level folder of torrent's files, e.g. turns "Album/1.flac" and "Album/2.flac"
     * into "1.flac" and "2.flac".
     * @return std::nullopt if the files are not in a single top-level folder
     */
    std::optional<std::vector<QString>> removeTopLevelFolder(std::span<const QString> filePaths);

    /**
     * Puts torrent's single file into a top-level folder named after it without its extension,
     * e.g. turns "Movie.mkv" into "Movie/Movie.mkv".
     * @return std::nullopt if the torrent doesn't consist of a single file that is not in any folder
     */
    std::optional<std::vector<QString>> addTopLevelFolder(std::span<const QString> filePaths);

    /**
     * Moves the top-level folder of torrent's files into its location, e.g. turns location "/music"
     * with files "Album/1.flac" and "Album/2.flac" into location "/music/Album" with files "1.flac" and "2.flac".
     * @param location Normalized location
     * @return std::nullopt if the files are not in a single top-level folder
     */
    std::optional<TorrentFilesLayout>
    moveTopLevelFolderIntoLocation(const QString& location, std::span<const QString> filePaths);

    /**
     * Moves the last folder of torrent's location into the torrent, as its top-level folder, e.g. turns location
     * "/music/Album" with files "1.flac" and "2.flac" into location "/music" with files "Album/1.flac" and "Album/2.flac".
     * @param location Normalized location
     * @return std::nullopt if the location has no parent folder
     */
    std::optional<TorrentFilesLayout>
    moveLocationFolderIntoTorrent(const QString& location, std::span<const QString> filePaths, PathOs pathOs);
}

#endif // TREMOTESF_RPC_TORRENTFILEPATHS_H
