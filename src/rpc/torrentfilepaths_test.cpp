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
