// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "torrentfilepaths.h"

using namespace Qt::StringLiterals;
using namespace tremotesf;

namespace {
    using Paths = std::vector<QString>;
}

class TorrentFilePathsTest final : public QObject {
    Q_OBJECT

private slots:
    void checkApplyPathMoves() {
        const Paths paths{u"A/1"_s, u"A/2"_s, u"A/B/3"_s, u"AB/4"_s, u"5"_s};
        QCOMPARE(
            applyPathMoves(paths, std::vector{PathMove{.from = u"A"_s, .to = u"X/Y"_s}}),
            (Paths{u"X/Y/1"_s, u"X/Y/2"_s, u"X/Y/B/3"_s, u"AB/4"_s, u"5"_s})
        );
        QCOMPARE(
            applyPathMoves(
                paths,
                std::vector{
                    PathMove{.from = u"A/B/3"_s, .to = u"3"_s},
                    PathMove{.from = u"A"_s, .to = u"C"_s},
                    PathMove{.from = u"A/1"_s, .to = u"C/one"_s}
                }
            ),
            (Paths{u"C/one"_s, u"C/2"_s, u"3"_s, u"AB/4"_s, u"5"_s})
        );
        QCOMPARE(applyPathMoves(paths, std::vector<PathMove>{}), paths);
    }

    void checkCheckFilePaths() {
        using Type = FilePathsError::Type;
        QCOMPARE(checkFilePaths(Paths{u"A/1"_s, u"A/B/2"_s, u"3"_s}), std::nullopt);
        for (const auto& path : {u""_s, u"/A"_s, u"A/"_s, u"A//1"_s, u"./1"_s, u"A/../1"_s, u".."_s}) {
            QCOMPARE(checkFilePaths(Paths{u"A/1"_s, path}), (FilePathsError{.type = Type::InvalidPath, .path = path}));
        }
        QCOMPARE(
            checkFilePaths(Paths{u"A/1"_s, u"A/2"_s, u"A/1"_s}),
            (FilePathsError{.type = Type::DuplicatePath, .path = u"A/1"_s})
        );
        QCOMPARE(
            checkFilePaths(Paths{u"A/B/1"_s, u"A/B"_s}),
            (FilePathsError{.type = Type::FileAndFolder, .path = u"A/B"_s})
        );
        QCOMPARE(checkFilePaths(Paths{u"A/B 1"_s, u"A/B"_s, u"A/B.1"_s}), std::nullopt);
    }

    void checkTopmostEntries() {
        QCOMPARE(
            topmostEntries(Paths{u"A/1"_s, u"B"_s, u"A"_s, u"B/C/2"_s, u"AB/3"_s, u"A"_s}),
            (Paths{u"B"_s, u"A"_s, u"AB/3"_s})
        );
    }

    void checkChangedFilePaths() {
        using ChangedPaths = std::vector<std::optional<QString>>;
        QCOMPARE(
            changedFilePaths(Paths{u"A/1"_s, u"A/2"_s, u"A/3"_s}, Paths{u"A/1"_s, u"B/2"_s, u"A/3"_s}),
            (ChangedPaths{std::nullopt, u"B/2"_s, std::nullopt})
        );
        QCOMPARE(changedFilePaths(Paths{u"A/1"_s, u"A/2"_s}, Paths{u"A/1"_s, u"A/2"_s}), ChangedPaths{});
    }

    void checkRemoveTopLevelFolder() {
        QCOMPARE(
            removeTopLevelFolder(Paths{u"Album/1.flac"_s, u"Album/CD2/2.flac"_s}),
            (Paths{u"1.flac"_s, u"CD2/2.flac"_s})
        );
        QCOMPARE(removeTopLevelFolder(Paths{u"Movie/Movie.mkv"_s}), (Paths{u"Movie.mkv"_s}));
        QCOMPARE(removeTopLevelFolder(Paths{u"Movie.mkv"_s}), std::nullopt);
        QCOMPARE(removeTopLevelFolder(Paths{u"Album/1.flac"_s, u"Album2/2.flac"_s}), std::nullopt);
    }

    void checkAddTopLevelFolder() {
        QCOMPARE(addTopLevelFolder(Paths{u"Movie.mkv"_s}), (Paths{u"Movie/Movie.mkv"_s}));
        QCOMPARE(addTopLevelFolder(Paths{u"archive.tar.gz"_s}), (Paths{u"archive.tar/archive.tar.gz"_s}));
        QCOMPARE(addTopLevelFolder(Paths{u"README"_s}), (Paths{u"README/README"_s}));
        QCOMPARE(addTopLevelFolder(Paths{u".hidden"_s}), (Paths{u".hidden/.hidden"_s}));
        QCOMPARE(addTopLevelFolder(Paths{u"Movie/Movie.mkv"_s}), std::nullopt);
        QCOMPARE(addTopLevelFolder(Paths{u"1.flac"_s, u"2.flac"_s}), std::nullopt);
    }

    void checkMoveTopLevelFolderIntoLocation() {
        QCOMPARE(
            moveTopLevelFolderIntoLocation(u"/music"_s, Paths{u"Album/1.flac"_s, u"Album/CD2/2.flac"_s}),
            (TorrentFilesLayout{.location = u"/music/Album"_s, .filePaths = {u"1.flac"_s, u"CD2/2.flac"_s}})
        );
        QCOMPARE(
            moveTopLevelFolderIntoLocation(u"/"_s, Paths{u"Album/1.flac"_s}),
            (TorrentFilesLayout{.location = u"/Album"_s, .filePaths = {u"1.flac"_s}})
        );
        QCOMPARE(
            moveTopLevelFolderIntoLocation(u"C:/"_s, Paths{u"Album/1.flac"_s}),
            (TorrentFilesLayout{.location = u"C:/Album"_s, .filePaths = {u"1.flac"_s}})
        );

        // no single top-level folder
        QCOMPARE(moveTopLevelFolderIntoLocation(u"/music"_s, Paths{}), std::nullopt);
        QCOMPARE(moveTopLevelFolderIntoLocation(u"/music"_s, Paths{u"1.flac"_s}), std::nullopt);
        QCOMPARE(moveTopLevelFolderIntoLocation(u"/music"_s, Paths{u"Album/1.flac"_s, u"2.flac"_s}), std::nullopt);
        QCOMPARE(
            moveTopLevelFolderIntoLocation(u"/music"_s, Paths{u"Album/1.flac"_s, u"Album2/2.flac"_s}),
            std::nullopt
        );
    }

    void checkMoveLocationFolderIntoTorrent() {
        QCOMPARE(
            moveLocationFolderIntoTorrent(u"/music/Album"_s, Paths{u"1.flac"_s, u"CD2/2.flac"_s}, PathOs::Unix),
            (TorrentFilesLayout{.location = u"/music"_s, .filePaths = {u"Album/1.flac"_s, u"Album/CD2/2.flac"_s}})
        );
        QCOMPARE(
            moveLocationFolderIntoTorrent(u"/Album"_s, Paths{u"1.flac"_s}, PathOs::Unix),
            (TorrentFilesLayout{.location = u"/"_s, .filePaths = {u"Album/1.flac"_s}})
        );
        QCOMPARE(
            moveLocationFolderIntoTorrent(u"C:/Album"_s, Paths{u"1.flac"_s}, PathOs::Windows),
            (TorrentFilesLayout{.location = u"C:/"_s, .filePaths = {u"Album/1.flac"_s}})
        );
        QCOMPARE(
            moveLocationFolderIntoTorrent(u"//server/share/Album"_s, Paths{u"1.flac"_s}, PathOs::Windows),
            (TorrentFilesLayout{.location = u"//server/share"_s, .filePaths = {u"Album/1.flac"_s}})
        );

        // no parent folder
        QCOMPARE(moveLocationFolderIntoTorrent(u"/"_s, Paths{u"1.flac"_s}, PathOs::Unix), std::nullopt);
        QCOMPARE(moveLocationFolderIntoTorrent(u"C:/"_s, Paths{u"1.flac"_s}, PathOs::Windows), std::nullopt);
        QCOMPARE(moveLocationFolderIntoTorrent(u"//server/share"_s, Paths{u"1.flac"_s}, PathOs::Windows), std::nullopt);
    }
};

QTEST_GUILESS_MAIN(TorrentFilePathsTest)

#include "torrentfilepaths_test.moc"
