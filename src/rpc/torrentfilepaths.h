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
