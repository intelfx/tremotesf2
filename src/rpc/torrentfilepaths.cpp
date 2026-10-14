// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfilepaths.h"

#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;

namespace tremotesf {
    std::optional<TorrentFilesLayout>
    moveTopLevelFolderIntoLocation(const QString& location, std::span<const QString> filePaths) {
        if (filePaths.empty()) {
            return std::nullopt;
        }
        const auto separator = filePaths.front().indexOf(u'/');
        if (separator <= 0) {
            return std::nullopt;
        }
        const auto prefix = QStringView(filePaths.front()).first(separator + 1);
        if (!std::ranges::all_of(filePaths, [&](const QString& path) {
                return path.size() > prefix.size() && path.startsWith(prefix);
            })) {
            return std::nullopt;
        }
        const auto topLevelFolder = prefix.chopped(1);
        return TorrentFilesLayout{
            .location = location.endsWith(u'/') ? location + topLevelFolder : location + u'/' + topLevelFolder,
            .filePaths = filePaths
                         | std::views::transform([&](const QString& path) { return path.sliced(prefix.size()); })
                         | std::ranges::to<std::vector>()
        };
    }

    std::optional<TorrentFilesLayout>
    moveLocationFolderIntoTorrent(const QString& location, std::span<const QString> filePaths, PathOs pathOs) {
        const auto separator = location.lastIndexOf(u'/');
        if (separator == -1 || separator == location.size() - 1) {
            // Relative path, or a root like "/" or "C:/"
            return std::nullopt;
        }
        const auto folder = location.sliced(separator + 1);
        auto parent = separator == 0 ? u"/"_s : normalizePath(location.first(separator), pathOs);
        if (pathOs == PathOs::Windows && parent.startsWith("//"_L1) && parent.count(u'/') < 3) {
            // "//server" is not a folder, unlike "//server/share"
            return std::nullopt;
        }
        return TorrentFilesLayout{
            .location = std::move(parent),
            .filePaths = filePaths
                         | std::views::transform([&](const QString& path) { return folder + u'/' + path; })
                         | std::ranges::to<std::vector>()
        };
    }
}
