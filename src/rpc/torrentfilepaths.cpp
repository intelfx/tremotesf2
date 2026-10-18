// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfilepaths.h"

#include <algorithm>
#include <ranges>

#include <QCoreApplication>
#include <QSet>

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        // Whether the path is the entry or is inside it
        bool isInEntry(QStringView path, QStringView entry) {
            return path.startsWith(entry) && (path.size() == entry.size() || path.at(entry.size()) == u'/');
        }

        // Paths of folders that hold the entry, from the outermost one
        auto parentFolders(QStringView path) {
            return std::views::iota(qsizetype{0}, path.size())
                   | std::views::filter([=](qsizetype i) { return path.at(i) == u'/'; })
                   | std::views::transform([=](qsizetype i) { return path.first(i); });
        }

        bool isValidPath(QStringView path) {
            return std::ranges::none_of(path.tokenize(u'/'), [](QStringView part) {
                return part.isEmpty() || part == u"." || part == u"..";
            });
        }
    }

    std::vector<QString> applyPathMoves(std::span<const QString> filePaths, std::span<const PathMove> moves) {
        return filePaths
               | std::views::transform([&](const QString& path) {
                     const PathMove* move{};
                     for (const PathMove& candidate : moves) {
                         if (isInEntry(path, candidate.from) && (!move || candidate.from.size() > move->from.size())) {
                             move = &candidate;
                         }
                     }
                     return move ? move->to + QStringView(path).sliced(move->from.size()) : path;
                 })
               | std::ranges::to<std::vector>();
    }

    QString entryName(const QString& path) { return path.sliced(path.lastIndexOf(u'/') + 1); }

    QString parentFolder(const QString& path) { return path.first(std::max(path.lastIndexOf(u'/'), qsizetype{0})); }

    QString fileNameWithoutExtension(const QString& fileName) {
        const auto extension = fileName.lastIndexOf(u'.');
        return extension > 0 ? fileName.first(extension) : fileName;
    }

    std::optional<FilePathsError> checkFilePaths(std::span<const QString> filePaths) {
        QSet<QStringView> paths{};
        paths.reserve(static_cast<qsizetype>(filePaths.size()));
        for (const QString& path : filePaths) {
            if (path.isEmpty() || !isValidPath(path)) {
                return FilePathsError{.type = FilePathsError::Type::InvalidPath, .path = path};
            }
            if (paths.contains(path)) {
                return FilePathsError{.type = FilePathsError::Type::DuplicatePath, .path = path};
            }
            paths.insert(path);
        }
        for (const QString& path : filePaths) {
            for (const QStringView folder : parentFolders(path)) {
                if (paths.contains(folder)) {
                    return FilePathsError{.type = FilePathsError::Type::FileAndFolder, .path = folder.toString()};
                }
            }
        }
        return std::nullopt;
    }

    QString filePathsErrorString(const FilePathsError& error) {
        switch (error.type) {
        case FilePathsError::Type::InvalidPath:
            //: %1 is a path of torrent's file
            return qApp->translate("tremotesf", "Invalid path «%1»").arg(error.path);
        case FilePathsError::Type::DuplicatePath:
            //: %1 is a path of torrent's file
            return qApp->translate("tremotesf", "More than one file would have path «%1»").arg(error.path);
        case FilePathsError::Type::FileAndFolder:
            //: %1 is a path of torrent's file
            return qApp->translate("tremotesf", "«%1» would be both a file and a folder").arg(error.path);
        }
        return {};
    }

    std::vector<QString> topmostEntries(std::span<const QString> entryPaths) {
        const auto entries = entryPaths | std::ranges::to<QSet<QStringView>>();
        QSet<QStringView> added{};
        std::vector<QString> topmost{};
        for (const QString& path : entryPaths) {
            if (!added.contains(path) && std::ranges::none_of(parentFolders(path), [&](QStringView folder) {
                    return entries.contains(folder);
                })) {
                added.insert(path);
                topmost.push_back(path);
            }
        }
        return topmost;
    }

    QString commonFolder(std::span<const QString> entryPaths) {
        if (entryPaths.empty()) {
            return {};
        }
        QString folder = parentFolder(entryPaths.front());
        for (const QString& path : entryPaths.subspan(1)) {
            while (!folder.isEmpty() && !path.startsWith(folder + u'/')) {
                folder = parentFolder(folder);
            }
        }
        return folder;
    }

    PathMove moveEntry(const QString& path, const QString& newPath) {
        return {.from = path, .to = newPath.endsWith(u'/') ? newPath + entryName(path) : newPath};
    }

    std::vector<PathMove> moveEntriesIntoFolder(std::span<const QString> entryPaths, const QString& folder) {
        const QString prefix = folder.isEmpty() || folder.endsWith(u'/') ? folder : folder + u'/';
        return entryPaths
               | std::views::transform([&](const QString& path) {
                     return PathMove{.from = path, .to = prefix + entryName(path)};
                 })
               | std::ranges::to<std::vector>();
    }

    std::vector<std::optional<QString>>
    changedFilePaths(std::span<const QString> filePaths, std::span<const QString> newFilePaths) {
        std::vector<std::optional<QString>> paths{};
        paths.reserve(newFilePaths.size());
        bool anyChanged = false;
        for (const auto& [path, newPath] : std::views::zip(filePaths, newFilePaths)) {
            if (newPath == path) {
                paths.emplace_back();
            } else {
                paths.emplace_back(newPath);
                anyChanged = true;
            }
        }
        if (!anyChanged) {
            paths.clear();
        }
        return paths;
    }

    std::optional<std::vector<QString>> removeTopLevelFolder(std::span<const QString> filePaths) {
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
        return filePaths
               | std::views::transform([&](const QString& path) { return path.sliced(prefix.size()); })
               | std::ranges::to<std::vector>();
    }

    std::optional<std::vector<QString>>
    addTopLevelFolder(std::span<const QString> filePaths, const QString& folderName) {
        if (filePaths.empty() || removeTopLevelFolder(filePaths)) {
            return std::nullopt;
        }
        if (filePaths.size() == 1) {
            const auto& fileName = filePaths.front();
            return std::vector{fileNameWithoutExtension(fileName) + u'/' + fileName};
        }
        if (folderName.isEmpty()) {
            return std::nullopt;
        }
        return filePaths
               | std::views::transform([&](const QString& path) { return folderName + u'/' + path; })
               | std::ranges::to<std::vector>();
    }

    std::vector<PathMove> flattenFolders(std::span<const QString> filePaths, std::span<const QString> folders) {
        const auto flattened = folders | std::ranges::to<QSet<QString>>();
        // Path without the flattened folders
        const auto flattenedPath = [&](const QString& path) {
            QString result{};
            for (qsizetype start = 0; start <= path.size();) {
                auto end = path.indexOf(u'/', start);
                if (end == -1) {
                    end = path.size();
                }
                if (!flattened.contains(path.first(end))) {
                    if (!result.isEmpty()) {
                        result += u'/';
                    }
                    result += QStringView(path).sliced(start, end - start);
                }
                start = end + 1;
            }
            return result;
        };
        // Entries that are directly in flattened folders move with everything in them
        std::vector<PathMove> moves{};
        QSet<QString> movedEntries{};
        for (const QString& path : filePaths) {
            for (const QStringView folder : parentFolders(path)) {
                if (!flattened.contains(folder.toString())) {
                    continue;
                }
                const auto end = path.indexOf(u'/', folder.size() + 1);
                QString entry = end == -1 ? path : path.first(end);
                if (!flattened.contains(entry) && !movedEntries.contains(entry)) {
                    movedEntries.insert(entry);
                    moves.push_back({.from = entry, .to = flattenedPath(entry)});
                }
            }
        }
        return moves;
    }

    std::optional<std::vector<PathMove>>
    extractIntoFolder(std::span<const QString> entryPaths, const QString& folderName) {
        if (entryPaths.empty()) {
            return std::nullopt;
        }
        const QString parent = parentFolder(entryPaths.front());
        if (!std::ranges::all_of(entryPaths, [&](const QString& path) { return parentFolder(path) == parent; })) {
            return std::nullopt;
        }
        const QString prefix = (parent.isEmpty() ? folderName : parent + u'/' + folderName) + u'/';
        return entryPaths
               | std::views::transform([&](const QString& path) {
                     return PathMove{.from = path, .to = prefix + entryName(path)};
                 })
               | std::ranges::to<std::vector>();
    }

    std::optional<TorrentFilesLayout>
    moveTopLevelFolderIntoLocation(const QString& location, std::span<const QString> filePaths) {
        auto paths = removeTopLevelFolder(filePaths);
        if (!paths) {
            return std::nullopt;
        }
        const auto topLevelFolder = QStringView(filePaths.front()).chopped(paths->front().size() + 1);
        return TorrentFilesLayout{
            .location = location.endsWith(u'/') ? location + topLevelFolder : location + u'/' + topLevelFolder,
            .filePaths = std::move(*paths)
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

    std::optional<QString> torrentTitle(std::span<const QString> filePaths) {
        if (filePaths.size() == 1 && !filePaths.front().contains(u'/')) {
            return fileNameWithoutExtension(filePaths.front());
        }
        if (const auto paths = removeTopLevelFolder(filePaths); paths) {
            return filePaths.front().first(filePaths.front().size() - paths->front().size() - 1);
        }
        return std::nullopt;
    }

    std::optional<std::vector<PathMove>> renameTorrentTitle(std::span<const QString> filePaths, const QString& title) {
        const auto oldTitle = torrentTitle(filePaths);
        if (!oldTitle) {
            return std::nullopt;
        }
        const auto& firstPath = filePaths.front();
        if (!firstPath.contains(u'/')) {
            return std::vector{PathMove{.from = firstPath, .to = title + firstPath.sliced(oldTitle->size())}};
        }

        std::vector moves{PathMove{.from = *oldTitle, .to = title}};
        const auto folderPrefix = *oldTitle + u'/';
        for (const QString& path : filePaths) {
            const auto name = QStringView(path).sliced(folderPrefix.size());
            if (!name.contains(u'/') && (name == *oldTitle || name.startsWith(*oldTitle + u'.'))) {
                moves.push_back({.from = path, .to = title + u'/' + title + name.sliced(oldTitle->size())});
            }
        }
        if (moves.size() == 1 && filePaths.size() == 1) {
            const auto name = entryName(firstPath);
            if (parentFolder(firstPath) == *oldTitle) {
                moves.push_back(
                    {.from = firstPath,
                     .to = title + u'/' + title + QStringView(name).sliced(fileNameWithoutExtension(name).size())}
                );
            }
        }
        return moves;
    }

    std::optional<QString> torrentTitleError(std::span<const QString> filePaths, const QString& title) {
        if (title.contains(u'/')) {
            //: %1 is a character that can't be used in names of files
            return qApp->translate("tremotesf", "Name can't contain «%1»").arg(u'/');
        }
        const auto moves = renameTorrentTitle(filePaths, title);
        if (!moves) {
            return qApp->translate("tremotesf", "Torrent has no single top-level folder or file");
        }
        if (const auto error = checkFilePaths(applyPathMoves(filePaths, *moves)); error) {
            return filePathsErrorString(*error);
        }
        return std::nullopt;
    }
}
