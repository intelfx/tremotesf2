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

    void checkCommonFolder() {
        QCOMPARE(commonFolder(Paths{u"A/B/1"_s, u"A/C"_s}), u"A"_s);
        QCOMPARE(commonFolder(Paths{u"A/B/1"_s, u"A/B/2"_s}), u"A/B"_s);
        QCOMPARE(commonFolder(Paths{u"A/B"_s}), u"A"_s);
        QCOMPARE(commonFolder(Paths{u"AB/1"_s, u"A/2"_s}), QString{});
        QCOMPARE(commonFolder(Paths{u"A/1"_s, u"B"_s}), QString{});
        QCOMPARE(commonFolder(Paths{}), QString{});
    }

    void checkMoveEntry() {
        QCOMPARE(moveEntry(u"A/1"_s, u"B/one"_s), (PathMove{.from = u"A/1"_s, .to = u"B/one"_s}));
        QCOMPARE(moveEntry(u"A/1"_s, u"B/"_s), (PathMove{.from = u"A/1"_s, .to = u"B/1"_s}));

        // A folder moved onto its parent goes away, like when flattening it
        const Paths paths{u"A/B/C/1"_s, u"A/B/C/D/2"_s, u"A/B/3"_s};
        QCOMPARE(
            applyPathMoves(paths, std::vector{moveEntry(u"A/B/C"_s, u"A/B"_s)}),
            (Paths{u"A/B/1"_s, u"A/B/D/2"_s, u"A/B/3"_s})
        );
        // With "/", it stays where it is
        QCOMPARE(applyPathMoves(paths, std::vector{moveEntry(u"A/B/C"_s, u"A/B/"_s)}), paths);
        // A folder moved into itself
        QCOMPARE(
            applyPathMoves(paths, std::vector{moveEntry(u"A/B/C"_s, u"A/B/C/E"_s)}),
            (Paths{u"A/B/C/E/1"_s, u"A/B/C/E/D/2"_s, u"A/B/3"_s})
        );
    }

    void checkMoveEntriesIntoFolder() {
        const Paths entries{u"A/1"_s, u"B"_s};
        QCOMPARE(
            moveEntriesIntoFolder(entries, u"C/D"_s),
            (std::vector{PathMove{.from = u"A/1"_s, .to = u"C/D/1"_s}, PathMove{.from = u"B"_s, .to = u"C/D/B"_s}})
        );
        QCOMPARE(
            moveEntriesIntoFolder(entries, u"C/"_s),
            (std::vector{PathMove{.from = u"A/1"_s, .to = u"C/1"_s}, PathMove{.from = u"B"_s, .to = u"C/B"_s}})
        );
        QCOMPARE(
            moveEntriesIntoFolder(entries, QString{}),
            (std::vector{PathMove{.from = u"A/1"_s, .to = u"1"_s}, PathMove{.from = u"B"_s, .to = u"B"_s}})
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

    void checkFlattenFolders() {
        const Paths paths{u"A/B/C/1"_s, u"A/B/C/D/2"_s, u"A/B/3"_s, u"A/4"_s};
        QCOMPARE(
            flattenFolders(paths, Paths{u"A/B/C"_s}),
            (std::vector{
                PathMove{.from = u"A/B/C/1"_s, .to = u"A/B/1"_s},
                PathMove{.from = u"A/B/C/D"_s, .to = u"A/B/D"_s}
            })
        );
        QCOMPARE(
            applyPathMoves(paths, flattenFolders(paths, Paths{u"A/B/C"_s, u"A/B"_s})),
            (Paths{u"A/1"_s, u"A/D/2"_s, u"A/3"_s, u"A/4"_s})
        );
        QCOMPARE(
            applyPathMoves(paths, flattenFolders(paths, Paths{u"A"_s})),
            (Paths{u"B/C/1"_s, u"B/C/D/2"_s, u"B/3"_s, u"4"_s})
        );
        QCOMPARE(applyPathMoves(paths, flattenFolders(paths, Paths{u"A"_s})), removeTopLevelFolder(paths).value());
    }

    void checkExtractIntoFolder() {
        QCOMPARE(
            extractIntoFolder(Paths{u"A/1"_s, u"A/B"_s}, u"C"_s),
            (std::vector{PathMove{.from = u"A/1"_s, .to = u"A/C/1"_s}, PathMove{.from = u"A/B"_s, .to = u"A/C/B"_s}})
        );
        QCOMPARE(
            extractIntoFolder(Paths{u"Movie.mkv"_s}, u"Movie"_s),
            (std::vector{PathMove{.from = u"Movie.mkv"_s, .to = u"Movie/Movie.mkv"_s}})
        );
        QCOMPARE(extractIntoFolder(Paths{u"A/1"_s, u"B/2"_s}, u"C"_s), std::nullopt);
        QCOMPARE(extractIntoFolder(Paths{u"A/1"_s, u"2"_s}, u"C"_s), std::nullopt);
        QCOMPARE(extractIntoFolder(Paths{}, u"C"_s), std::nullopt);
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

    void checkTorrentTitle() {
        QCOMPARE(torrentTitle(Paths{u"Movie.mkv"_s}), u"Movie"_s);
        QCOMPARE(torrentTitle(Paths{u"archive.tar.gz"_s}), u"archive.tar"_s);
        QCOMPARE(torrentTitle(Paths{u"README"_s}), u"README"_s);
        QCOMPARE(torrentTitle(Paths{u".hidden"_s}), u".hidden"_s);
        QCOMPARE(torrentTitle(Paths{u"Movie.2020/Movie.2020.mkv"_s, u"Movie.2020/Sub/1.srt"_s}), u"Movie.2020"_s);
        QCOMPARE(torrentTitle(Paths{u"A/1"_s, u"B/2"_s}), std::nullopt);
        QCOMPARE(torrentTitle(Paths{u"A/1"_s, u"2"_s}), std::nullopt);
        QCOMPARE(torrentTitle(Paths{}), std::nullopt);
    }

    void checkRenameTorrentTitle() {
        const auto rename = [](const Paths& paths, const QString& title) {
            return applyPathMoves(paths, renameTorrentTitle(paths, title).value());
        };
        QCOMPARE(rename(Paths{u"Movie.mkv"_s}, u"T"_s), (Paths{u"T.mkv"_s}));
        QCOMPARE(rename(Paths{u"archive.tar.gz"_s}, u"T"_s), (Paths{u"T.gz"_s}));
        QCOMPARE(rename(Paths{u"README"_s}, u"T"_s), (Paths{u"T"_s}));
        QCOMPARE(
            rename(
                Paths{
                    u"Movie/Movie.mkv"_s,
                    u"Movie/Movie.en.srt"_s,
                    u"Movie/Movie"_s,
                    u"Movie/sample.mkv"_s,
                    u"Movie/Movies.txt"_s,
                    u"Movie/Sub/Movie.srt"_s
                },
                u"T"_s
            ),
            (Paths{u"T/T.mkv"_s, u"T/T.en.srt"_s, u"T/T"_s, u"T/sample.mkv"_s, u"T/Movies.txt"_s, u"T/Sub/Movie.srt"_s})
        );
        // The only file
        QCOMPARE(rename(Paths{u"Release/abc.mkv"_s}, u"T"_s), (Paths{u"T/T.mkv"_s}));
        QCOMPARE(rename(Paths{u"Release/Release.en.srt"_s}, u"T"_s), (Paths{u"T/T.en.srt"_s}));
        QCOMPARE(rename(Paths{u"Release/Sub/abc.mkv"_s}, u"T"_s), (Paths{u"T/Sub/abc.mkv"_s}));
        QCOMPARE(
            rename(Paths{u"Release/abc.mkv"_s, u"Release/def.mkv"_s}, u"T"_s),
            (Paths{u"T/abc.mkv"_s, u"T/def.mkv"_s})
        );
        QCOMPARE(renameTorrentTitle(Paths{u"A/1"_s, u"B/2"_s}, u"T"_s), std::nullopt);
    }

    void checkTorrentTitleError() {
        QCOMPARE(torrentTitleError(Paths{u"Movie/Movie.mkv"_s}, u"T"_s), std::nullopt);
        QVERIFY(torrentTitleError(Paths{u"Movie/Movie.mkv"_s}, u"A/B"_s).has_value());
        QVERIFY(torrentTitleError(Paths{u"Movie/Movie.mkv"_s}, u".."_s).has_value());
        QVERIFY(torrentTitleError(Paths{u"Movie/Movie.mkv"_s, u"Movie/T.mkv"_s}, u"T"_s).has_value());
        QVERIFY(torrentTitleError(Paths{u"A/1"_s, u"B/2"_s}, u"T"_s).has_value());
    }
};

QTEST_GUILESS_MAIN(TorrentFilePathsTest)

#include "torrentfilepaths_test.moc"
