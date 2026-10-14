// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_ADDTORRENTHELPERS_H
#define TREMOTESF_ADDTORRENTHELPERS_H

#include <optional>
#include <span>
#include <vector>

#include <QString>

#include "bencodeparser.h"
#include "settings.h"
#include "rpc/torrent.h"

class QDialog;
class QWidget;

namespace tremotesf {
    class Rpc;
    class TorrentMetainfoFile;

    struct AddTorrentParameters {
        QString downloadDirectory;
        TorrentData::Priority priority;
        bool startAfterAdding;
        bool deleteTorrentFile;
        bool moveTorrentFileToTrash;
        Settings::TorrentContentLayout contentLayout;
    };

    AddTorrentParameters getAddTorrentParameters(Rpc* rpc);
    AddTorrentParameters getInitialAddTorrentParameters(Rpc* rpc);

    /**
     * Paths that torrent's files get with the content layout
     * @param filePaths Paths of torrent's files in the torrent file
     * @return std::nullopt if the content layout doesn't change the paths
     */
    std::optional<std::vector<QString>>
    applyContentLayout(std::span<const QString> filePaths, Settings::TorrentContentLayout layout);

    // Takes paths of torrent's files out of the torrent file
    std::vector<QString> takeFilePaths(TorrentMetainfoFile& torrentFile);

    QDialog*
    createAskForMergingTrackersDialog(Torrent* torrent, std::vector<std::set<QString>> trackers, QWidget* parent);

    QString bencodeErrorString(bencode::Error::Type type);
}

#endif // TREMOTESF_ADDTORRENTHELPERS_H
