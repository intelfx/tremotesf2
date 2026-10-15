// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#define QTEST_THROW_ON_FAIL

#include <ranges>
#include <set>

#include <QTest>
#include <QSignalSpy>

#include "basetorrentfilesmodel.h"
#include "torrentfilestreebuilder.h"
#include "formatutils.h"
#include "log/log.h"

#include <fmt/ranges.h>

SPECIALIZE_FORMATTER_FOR_QDEBUG(QVariant)

using namespace Qt::StringLiterals;

namespace tremotesf {
    using formatutils::formatByteSize;
    using formatutils::formatProgress;

    namespace {
        QModelIndex indexForPath(QLatin1String path, QAbstractItemModel& model) {
            QModelIndex index{};
            for (const auto& part : path.tokenize(u'/')) {
                const auto childIndexes = std::views::iota(0, model.rowCount(index))
                                          | std::views::transform([&](int row) { return model.index(row, 0, index); });
                const auto found = std::ranges::find(childIndexes, part, [&](const QModelIndex& childIndex) {
                    return childIndex.data().toString();
                });
                if (found == childIndexes.end()) {
                    warning().log("Did not find child with name {}", part);
                    QFAIL("Nope");
                }
                index = *found;
            }
            return index;
        }

        struct ExpectedDataChanged {
            QModelIndex topLeft;
            QModelIndex bottomRight{topLeft.siblingAtColumn(topLeft.model()->columnCount() - 1)};
            QList<int> roles{};

            std::strong_ordering operator<=>(const ExpectedDataChanged&) const = default;

            static ExpectedDataChanged allColumnsAndRoles(QModelIndex topLeft, QModelIndex bottomRight) {
                return {
                    .topLeft = topLeft,
                    .bottomRight = bottomRight.siblingAtColumn(bottomRight.model()->columnCount() - 1)
                };
            }

            static ExpectedDataChanged allColumnsAndRoles(QModelIndex index) {
                return allColumnsAndRoles(index, index);
            }

            static ExpectedDataChanged
            specificColumnAndRoles(QModelIndex topLeft, QModelIndex bottomRight, int column, QList<int> roles) {
                return {
                    .topLeft = topLeft.siblingAtColumn(column),
                    .bottomRight = bottomRight.siblingAtColumn(column),
                    .roles = std::move(roles)
                };
            }

            static ExpectedDataChanged specificColumnAndRoles(QModelIndex index, int column, QList<int> roles) {
                return specificColumnAndRoles(index, index, column, std::move(roles));
            }
        };

        [[maybe_unused]]
        QDebug operator<<(QDebug debug, const ExpectedDataChanged& dataChanged) {
            const QDebugStateSaver saver(debug);
            debug.nospace()
                << "{.topLeft = "
                << dataChanged.topLeft
                << ", bottomRight = "
                << dataChanged.bottomRight
                << ", roles = "
                << dataChanged.roles
                << "}";
            return debug;
        }

        std::set<ExpectedDataChanged> actualDataChanged(QSignalSpy& spy) {
            return spy
                   | std::views::transform([](QList<QVariant> args) {
                         return ExpectedDataChanged{
                             .topLeft = args.at(0).toModelIndex(),
                             .bottomRight = args.at(1).toModelIndex(),
                             .roles = args.at(2).toList()
                                      | std::views::transform([](const QVariant& role) { return role.toInt(); })
                                      | std::ranges::to<QList>()
                         };
                     })
                   | std::ranges::to<std::set>();
        }
    }

    class TestTorrentFilesModel : public BaseTorrentFilesModel {
        Q_OBJECT
    public:
        TestTorrentFilesModel()
            : BaseTorrentFilesModel(
                  {Column::Name, Column::Size, Column::ProgressBar, Column::Progress, Column::Priority}
              ) {}

        void renameFile(const QModelIndex&, const QString&) override {}

        struct File {
            QString path;
            qint64 size;
            qint64 completedSize;
            TorrentFilesModelEntry::Priority priority;
            bool wanted;
        };

        void populateEverythingNormal() {
            const std::array files{
                File{
                    .path = "topdir/subdir1/subsubddir/file1",
                    .size = 666,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                },
                File{
                    .path = "topdir/subdir1/subsubddir/file2",
                    .size = 100000,
                    .completedSize = 4234,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                },
                File{
                    .path = "topdir/subdir2/file1",
                    .size = 3333333,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                },
                File{
                    .path = "topdir/subdir2/file2",
                    .size = 111,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                }
            };
            TorrentFilesTreeBuilder builder(files.size());
            for (const File& file : files) {
                builder.addFile(
                    file.path.tokenize(u'/', Qt::SkipEmptyParts),
                    true,
                    file.size,
                    file.completedSize,
                    file.wanted,
                    file.priority
                );
            }
            builder.calculateDirectoriesRecursively();
            mRootEntry = std::move(builder.rootEntry);
            mFiles = std::move(builder.files);
        }

        void populateWithoutSingleTopLevelEntry() {
            const std::array files{
                File{
                    .path = "dir1/file1",
                    .size = 100,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                },
                File{
                    .path = "file2",
                    .size = 20,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                },
                File{
                    .path = "dir1/file3",
                    .size = 3,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                }
            };
            const auto paths = files | std::views::transform(&File::path);
            QVERIFY(!TorrentFilesTreeBuilder::hasSingleTopLevelEntry(paths));
            TorrentFilesTreeBuilder builder(files.size());
            builder.initializeRootDirectory({});
            for (const File& file : files) {
                builder.addFile(
                    file.path.tokenize(u'/', Qt::SkipEmptyParts),
                    false,
                    file.size,
                    file.completedSize,
                    file.wanted,
                    file.priority
                );
            }
            builder.calculateDirectoriesRecursively();
            mRootEntry = std::move(builder.rootEntry);
            mFiles = std::move(builder.files);
        }

        void populateMixedState() {
            const std::array files{
                File{
                    .path = "topdir/subdir1/subsubddir/file1",
                    .size = 666,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                },
                File{
                    .path = "topdir/subdir1/subsubddir/file2",
                    .size = 100000,
                    .completedSize = 4234,
                    .priority = TorrentFilesModelEntry::Priority::Low,
                    .wanted = false
                },
                File{
                    .path = "topdir/subdir2/file1",
                    .size = 3333333,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                },
                File{
                    .path = "topdir/subdir2/file2",
                    .size = 111,
                    .completedSize = 0,
                    .priority = TorrentFilesModelEntry::Priority::Normal,
                    .wanted = true
                }
            };
            TorrentFilesTreeBuilder builder(files.size());
            for (const File& file : files) {
                builder.addFile(
                    file.path.tokenize(u'/', Qt::SkipEmptyParts),
                    true,
                    file.size,
                    file.completedSize,
                    file.wanted,
                    file.priority
                );
            }
            builder.calculateDirectoriesRecursively();
            mRootEntry = std::move(builder.rootEntry);
            mFiles = std::move(builder.files);
        }

        using BaseTorrentFilesModel::renameEntries;
        using BaseTorrentFilesModel::updateFiles;
    };

    class TorrentFilesModelTest : public QObject {
        Q_OBJECT

    private slots:
        void checkInitialStateNormal() {
            TestTorrentFilesModel model{};
            model.populateEverythingNormal();

            checkTree(
                model,
                {.name = "topdir"_L1,
                 .size = 3434110,
                 .progress = 4234.0 / 3434110.0,
                 .priority = TorrentFilesModelEntry::Priority::Normal,
                 .checkState = Qt::CheckState::Checked,

                 .children = {
                     {.name = "subdir1"_L1,
                      .size = 100666,
                      .progress = 4234.0 / 100666.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked,

                      .children =
                          {{.name = "subsubddir"_L1,
                            .size = 100666,
                            .progress = 4234.0 / 100666.0,
                            .priority = TorrentFilesModelEntry::Priority::Normal,
                            .checkState = Qt::CheckState::Checked,

                            .children =
                                {{.name = "file1"_L1,
                                  .size = 666,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Checked},
                                 {.name = "file2"_L1,
                                  .size = 100000,
                                  .progress = 4234.0 / 100666.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Checked}}}}},

                     {.name = "subdir2"_L1,
                      .size = 3333444,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked,

                      .children = {
                          {.name = "file1"_L1,
                           .size = 3333333,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked},
                          {.name = "file2"_L1,
                           .size = 111,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked}
                      }}
                 }}
            );
        }

        void checkInitialStateMixed() {
            TestTorrentFilesModel model{};
            model.populateMixedState();

            checkTree(
                model,
                {.name = "topdir"_L1,
                 .size = 3434110,
                 .progress = 4234.0 / 3434110.0,
                 .priority = TorrentFilesModelEntry::Priority::Mixed,
                 .checkState = Qt::CheckState::PartiallyChecked,

                 .children = {
                     {.name = "subdir1"_L1,
                      .size = 100666,
                      .progress = 4234.0 / 100666.0,
                      .priority = TorrentFilesModelEntry::Priority::Mixed,
                      .checkState = Qt::CheckState::PartiallyChecked,

                      .children =
                          {{.name = "subsubddir"_L1,
                            .size = 100666,
                            .progress = 4234.0 / 100666.0,
                            .priority = TorrentFilesModelEntry::Priority::Mixed,
                            .checkState = Qt::CheckState::PartiallyChecked,

                            .children =
                                {{.name = "file1"_L1,
                                  .size = 666,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Checked},
                                 {.name = "file2"_L1,
                                  .size = 100000,
                                  .progress = 4234.0 / 100666.0,
                                  .priority = TorrentFilesModelEntry::Priority::Low,
                                  .checkState = Qt::CheckState::Unchecked}}}}},

                     {.name = "subdir2"_L1,
                      .size = 3333444,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked,

                      .children = {
                          {.name = "file1"_L1,
                           .size = 3333333,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked},
                          {.name = "file2"_L1,
                           .size = 111,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked}
                      }}
                 }}
            );
        }

        void checkSetFilesWantedFromRoot() {
            TestTorrentFilesModel model{};
            model.populateMixedState();

            QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

            model.setFilesWanted({model.index(0, 0)}, false);

            const auto actualSignals = actualDataChanged(dataChanged);
            const int nameColumn = model.columnNumber(BaseTorrentFilesModel::Column::Name);
            const QList<int> expectedRoles{Qt::CheckStateRole};
            const auto expectedSignals = std::set{
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir/file1"_L1, model),
                    nameColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir"_L1, model),
                    nameColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir2/file1"_L1, model),
                    indexForPath("topdir/subdir2/file2"_L1, model),
                    nameColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1"_L1, model),
                    indexForPath("topdir/subdir2"_L1, model),
                    nameColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(indexForPath("topdir"_L1, model), nameColumn, expectedRoles)
            };

            QCOMPARE(actualSignals, expectedSignals);

            checkTree(
                model,
                {.name = "topdir"_L1,
                 .size = 3434110,
                 .progress = 4234.0 / 3434110.0,
                 .priority = TorrentFilesModelEntry::Priority::Mixed,
                 .checkState = Qt::CheckState::Unchecked,

                 .children = {
                     {.name = "subdir1"_L1,
                      .size = 100666,
                      .progress = 4234.0 / 100666.0,
                      .priority = TorrentFilesModelEntry::Priority::Mixed,
                      .checkState = Qt::CheckState::Unchecked,

                      .children =
                          {{.name = "subsubddir"_L1,
                            .size = 100666,
                            .progress = 4234.0 / 100666.0,
                            .priority = TorrentFilesModelEntry::Priority::Mixed,
                            .checkState = Qt::CheckState::Unchecked,

                            .children =
                                {{.name = "file1"_L1,
                                  .size = 666,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Unchecked},
                                 {.name = "file2"_L1,
                                  .size = 100000,
                                  .progress = 4234.0 / 100666.0,
                                  .priority = TorrentFilesModelEntry::Priority::Low,
                                  .checkState = Qt::CheckState::Unchecked}}}}},

                     {.name = "subdir2"_L1,
                      .size = 3333444,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Unchecked,

                      .children = {
                          {.name = "file1"_L1,
                           .size = 3333333,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Unchecked},
                          {.name = "file2"_L1,
                           .size = 111,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Unchecked}
                      }}
                 }}
            );
        }

        void checkSetFilesWantedFromFile() {
            TestTorrentFilesModel model{};
            model.populateMixedState();

            QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

            model.setFilesWanted({indexForPath("topdir/subdir1/subsubddir/file2"_L1, model)}, true);

            const auto actualSignals = actualDataChanged(dataChanged);
            const int nameColumn = model.columnNumber(BaseTorrentFilesModel::Column::Name);
            const QList<int> expectedRoles{Qt::CheckStateRole};
            const auto expectedSignals = std::set{
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir/file2"_L1, model),
                    nameColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir"_L1, model),
                    nameColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1"_L1, model),
                    nameColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(indexForPath("topdir"_L1, model), nameColumn, expectedRoles)
            };
            QCOMPARE(actualSignals, expectedSignals);

            checkTree(
                model,
                {.name = "topdir"_L1,
                 .size = 3434110,
                 .progress = 4234.0 / 3434110.0,
                 .priority = TorrentFilesModelEntry::Priority::Mixed,
                 .checkState = Qt::CheckState::Checked,

                 .children = {
                     {.name = "subdir1"_L1,
                      .size = 100666,
                      .progress = 4234.0 / 100666.0,
                      .priority = TorrentFilesModelEntry::Priority::Mixed,
                      .checkState = Qt::CheckState::Checked,

                      .children =
                          {{.name = "subsubddir"_L1,
                            .size = 100666,
                            .progress = 4234.0 / 100666.0,
                            .priority = TorrentFilesModelEntry::Priority::Mixed,
                            .checkState = Qt::CheckState::Checked,

                            .children =
                                {{.name = "file1"_L1,
                                  .size = 666,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Checked},
                                 {.name = "file2"_L1,
                                  .size = 100000,
                                  .progress = 4234.0 / 100666.0,
                                  .priority = TorrentFilesModelEntry::Priority::Low,
                                  .checkState = Qt::CheckState::Checked}}}}},

                     {.name = "subdir2"_L1,
                      .size = 3333444,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked,

                      .children = {
                          {.name = "file1"_L1,
                           .size = 3333333,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked},
                          {.name = "file2"_L1,
                           .size = 111,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked}
                      }}
                 }}
            );
        }

        void checkSetFilesPriorityFromRoot() {
            TestTorrentFilesModel model{};
            model.populateMixedState();

            QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

            model.setFilesPriority({model.index(0, 0)}, TorrentFilesModelEntry::Priority::Low);

            const auto actualSignals = actualDataChanged(dataChanged);
            const int priorityColumn = model.columnNumber(BaseTorrentFilesModel::Column::Priority);
            const QList<int> expectedRoles{Qt::DisplayRole, BaseTorrentFilesModel::SortRole};
            const auto expectedSignals = std::set{
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir/file1"_L1, model),
                    priorityColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir"_L1, model),
                    priorityColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir2/file1"_L1, model),
                    indexForPath("topdir/subdir2/file2"_L1, model),
                    priorityColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1"_L1, model),
                    indexForPath("topdir/subdir2"_L1, model),
                    priorityColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir"_L1, model),
                    priorityColumn,
                    expectedRoles
                )
            };
            QCOMPARE(actualSignals, expectedSignals);

            checkTree(
                model,
                {.name = "topdir"_L1,
                 .size = 3434110,
                 .progress = 4234.0 / 3434110.0,
                 .priority = TorrentFilesModelEntry::Priority::Low,
                 .checkState = Qt::CheckState::PartiallyChecked,

                 .children = {
                     {.name = "subdir1"_L1,
                      .size = 100666,
                      .progress = 4234.0 / 100666.0,
                      .priority = TorrentFilesModelEntry::Priority::Low,
                      .checkState = Qt::CheckState::PartiallyChecked,

                      .children =
                          {{.name = "subsubddir"_L1,
                            .size = 100666,
                            .progress = 4234.0 / 100666.0,
                            .priority = TorrentFilesModelEntry::Priority::Low,
                            .checkState = Qt::CheckState::PartiallyChecked,

                            .children =
                                {{.name = "file1"_L1,
                                  .size = 666,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::Low,
                                  .checkState = Qt::CheckState::Checked},
                                 {.name = "file2"_L1,
                                  .size = 100000,
                                  .progress = 4234.0 / 100666.0,
                                  .priority = TorrentFilesModelEntry::Priority::Low,
                                  .checkState = Qt::CheckState::Unchecked}}}}},

                     {.name = "subdir2"_L1,
                      .size = 3333444,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Low,
                      .checkState = Qt::CheckState::Checked,

                      .children = {
                          {.name = "file1"_L1,
                           .size = 3333333,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Low,
                           .checkState = Qt::CheckState::Checked},
                          {.name = "file2"_L1,
                           .size = 111,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Low,
                           .checkState = Qt::CheckState::Checked}
                      }}
                 }}
            );
        }

        void checkSetFilesPriorityFromFile() {
            TestTorrentFilesModel model{};
            model.populateMixedState();

            QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

            model.setFilesPriority(
                {indexForPath("topdir/subdir1/subsubddir/file2"_L1, model)},
                TorrentFilesModelEntry::Priority::Normal
            );

            const auto actualSignals = actualDataChanged(dataChanged);
            const int priorityColumn = model.columnNumber(BaseTorrentFilesModel::Column::Priority);
            const QList<int> expectedRoles{Qt::DisplayRole, BaseTorrentFilesModel::SortRole};
            const auto expectedSignals = std::set{
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir/file2"_L1, model),
                    priorityColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1/subsubddir"_L1, model),
                    priorityColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir/subdir1"_L1, model),
                    priorityColumn,
                    expectedRoles
                ),
                ExpectedDataChanged::specificColumnAndRoles(
                    indexForPath("topdir"_L1, model),
                    priorityColumn,
                    expectedRoles
                )
            };
            QCOMPARE(actualSignals, expectedSignals);

            checkTree(
                model,
                {.name = "topdir"_L1,
                 .size = 3434110,
                 .progress = 4234.0 / 3434110.0,
                 .priority = TorrentFilesModelEntry::Priority::Normal,
                 .checkState = Qt::CheckState::PartiallyChecked,

                 .children = {
                     {.name = "subdir1"_L1,
                      .size = 100666,
                      .progress = 4234.0 / 100666.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::PartiallyChecked,

                      .children =
                          {{.name = "subsubddir"_L1,
                            .size = 100666,
                            .progress = 4234.0 / 100666.0,
                            .priority = TorrentFilesModelEntry::Priority::Normal,
                            .checkState = Qt::CheckState::PartiallyChecked,

                            .children =
                                {{.name = "file1"_L1,
                                  .size = 666,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Checked},
                                 {.name = "file2"_L1,
                                  .size = 100000,
                                  .progress = 4234.0 / 100666.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Unchecked}}}}},

                     {.name = "subdir2"_L1,
                      .size = 3333444,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked,

                      .children = {
                          {.name = "file1"_L1,
                           .size = 3333333,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked},
                          {.name = "file2"_L1,
                           .size = 111,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked}
                      }}
                 }}
            );
        }

        void checkHasSingleTopLevelEntry() {
            using Paths = std::vector<QString>;
            QVERIFY(TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{}));
            QVERIFY(TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{u"file"_s}));
            QVERIFY(TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{u"dir/file"_s}));
            QVERIFY(TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{u"dir/file1"_s, u"dir/sub/file2"_s}));
            QVERIFY(!TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{u"file1"_s, u"file2"_s}));
            QVERIFY(!TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{u"dir/file1"_s, u"file2"_s}));
            QVERIFY(!TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{u"dir1/file1"_s, u"dir2/file2"_s}));
            QVERIFY(!TorrentFilesTreeBuilder::hasSingleTopLevelEntry(Paths{u"dir/file1"_s, u"dirx/file2"_s}));
        }

        void checkWithoutSingleTopLevelEntry() {
            TestTorrentFilesModel model{};
            model.populateWithoutSingleTopLevelEntry();

            QCOMPARE(model.rowCount(), 2);
            const auto dir1 = indexForPath("dir1"_L1, model);
            const auto file1 = indexForPath("dir1/file1"_L1, model);
            const auto file2 = indexForPath("file2"_L1, model);
            QCOMPARE(model.parent(dir1), QModelIndex{});
            QCOMPARE(model.parent(file2), QModelIndex{});
            QCOMPARE(model.parent(file1), dir1);
            QCOMPARE(static_cast<const TorrentFilesModelEntry*>(file1.internalPointer())->path(), u"dir1/file1"_s);
            QCOMPARE(static_cast<const TorrentFilesModelEntry*>(file2.internalPointer())->path(), u"file2"_s);

            checkTree(
                model,
                {.name = "dir1"_L1,
                 .size = 103,
                 .progress = 0.0,
                 .priority = TorrentFilesModelEntry::Priority::Normal,
                 .checkState = Qt::CheckState::Checked,
                 .children = {
                     {.name = "file1"_L1,
                      .size = 100,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked},
                     {.name = "file3"_L1,
                      .size = 3,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked}
                 }}
            );

            QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
            model.updateFiles(std::array{1}, [](size_t, TorrentFilesModelEntry* file) {
                file->update(true, TorrentFilesModelEntry::Priority::High, 0);
            });
            QCOMPARE(actualDataChanged(dataChanged), std::set{ExpectedDataChanged::allColumnsAndRoles(file2)});
            dataChanged.clear();

            model.setFilesWanted({dir1}, false);
            QCOMPARE(file1.data(Qt::CheckStateRole).value<Qt::CheckState>(), Qt::CheckState::Unchecked);
            QCOMPARE(file2.data(Qt::CheckStateRole).value<Qt::CheckState>(), Qt::CheckState::Checked);
        }

        void checkRenameEntries() {
            TestTorrentFilesModel model{};
            model.populateEverythingNormal();
            using Paths = std::vector<QString>;

            QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
            QVERIFY(model.renameEntries(
                Paths{
                    u"top/subdir1/subsubddir/file1"_s,
                    u"top/subdir1/subsubddir/file2"_s,
                    u"top/subdir2/file2"_s,
                    u"top/subdir2/file1"_s
                }
            ));
            const auto top = indexForPath("top"_L1, model);
            const auto file1 = indexForPath("top/subdir2/file2"_L1, model);
            const auto file2 = indexForPath("top/subdir2/file1"_L1, model);
            QCOMPARE(file1.siblingAtColumn(1).data().toString(), formatByteSize(3333333));
            QCOMPARE(file2.siblingAtColumn(1).data().toString(), formatByteSize(111));
            QCOMPARE(
                actualDataChanged(dataChanged),
                (std::set{
                    ExpectedDataChanged::allColumnsAndRoles(top),
                    ExpectedDataChanged::allColumnsAndRoles(file1),
                    ExpectedDataChanged::allColumnsAndRoles(file2)
                })
            );
            dataChanged.clear();

            // A file goes to another directory
            QVERIFY(!model.renameEntries(
                Paths{
                    u"top/subdir1/file1"_s,
                    u"top/subdir1/subsubddir/file2"_s,
                    u"top/subdir2/file2"_s,
                    u"top/subdir2/file1"_s
                }
            ));
            QVERIFY(!model.renameEntries(
                Paths{
                    u"top/subdir2/subsubddir/file1"_s,
                    u"top/subdir1/subsubddir/file2"_s,
                    u"top/subdir2/file2"_s,
                    u"top/subdir2/file1"_s
                }
            ));
            // Directories would be merged
            QVERIFY(!model.renameEntries(
                Paths{
                    u"top/subdir2/subsubddir/file1"_s,
                    u"top/subdir2/subsubddir/file2"_s,
                    u"top/subdir2/file2"_s,
                    u"top/subdir2/file1"_s
                }
            ));
            QVERIFY(!model.renameEntries(Paths{u"top/subdir2/file2"_s}));
            QVERIFY(dataChanged.isEmpty());
            QVERIFY(indexForPath("top/subdir1/subsubddir/file1"_L1, model).isValid());
        }

        void checkRenameEntriesWithoutSingleTopLevelEntry() {
            TestTorrentFilesModel model{};
            model.populateWithoutSingleTopLevelEntry();
            using Paths = std::vector<QString>;

            QVERIFY(model.renameEntries(Paths{u"dir/file1"_s, u"file2"_s, u"dir/file3"_s}));
            QCOMPARE(model.rowCount(), 2);
            QCOMPARE(model.parent(indexForPath("dir/file1"_L1, model)), indexForPath("dir"_L1, model));

            // A file and a directory with the same name
            QVERIFY(!model.renameEntries(Paths{u"dir/file1"_s, u"dir"_s, u"dir/file3"_s}));
            QVERIFY(indexForPath("file2"_L1, model).isValid());
        }

        void checkUpdateFiles() {
            TestTorrentFilesModel model{};
            model.populateMixedState();

            std::array changed{TestTorrentFilesModel::File{}};

            QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

            model.updateFiles(std::array{0, 1}, [](size_t index, TorrentFilesModelEntry* file) {
                switch (index) {
                case 0:
                    file->update(true, TorrentFilesModelEntry::Priority::High, 0);
                    break;
                case 1:
                    file->update(true, TorrentFilesModelEntry::Priority::Normal, 0);
                    break;
                }
            });

            const auto actualSignals = actualDataChanged(dataChanged);
            const auto expectedSignals = std::set{
                ExpectedDataChanged::allColumnsAndRoles(
                    indexForPath("topdir/subdir1/subsubddir/file1"_L1, model),
                    indexForPath("topdir/subdir1/subsubddir/file2"_L1, model)
                ),
                ExpectedDataChanged::allColumnsAndRoles(indexForPath("topdir/subdir1/subsubddir"_L1, model)),
                ExpectedDataChanged::allColumnsAndRoles(indexForPath("topdir/subdir1"_L1, model)),
                ExpectedDataChanged::allColumnsAndRoles(indexForPath("topdir"_L1, model))
            };
            QCOMPARE(actualSignals, expectedSignals);

            checkTree(
                model,
                {.name = "topdir"_L1,
                 .size = 3434110,
                 .progress = 0.0,
                 .priority = TorrentFilesModelEntry::Priority::Mixed,
                 .checkState = Qt::CheckState::Checked,

                 .children = {
                     {.name = "subdir1"_L1,
                      .size = 100666,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Mixed,
                      .checkState = Qt::CheckState::Checked,

                      .children =
                          {{.name = "subsubddir"_L1,
                            .size = 100666,
                            .progress = 0.0,
                            .priority = TorrentFilesModelEntry::Priority::Mixed,
                            .checkState = Qt::CheckState::Checked,

                            .children =
                                {{.name = "file1"_L1,
                                  .size = 666,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::High,
                                  .checkState = Qt::CheckState::Checked},
                                 {.name = "file2"_L1,
                                  .size = 100000,
                                  .progress = 0.0,
                                  .priority = TorrentFilesModelEntry::Priority::Normal,
                                  .checkState = Qt::CheckState::Checked}}}}},

                     {.name = "subdir2"_L1,
                      .size = 3333444,
                      .progress = 0.0,
                      .priority = TorrentFilesModelEntry::Priority::Normal,
                      .checkState = Qt::CheckState::Checked,

                      .children = {
                          {.name = "file1"_L1,
                           .size = 3333333,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked},
                          {.name = "file2"_L1,
                           .size = 111,
                           .progress = 0.0,
                           .priority = TorrentFilesModelEntry::Priority::Normal,
                           .checkState = Qt::CheckState::Checked}
                      }}
                 }}
            );
        }

    private:
        struct ExpectedData {
            QLatin1String name;
            long long size;
            double progress;
            TorrentFilesModelEntry::Priority priority;
            Qt::CheckState checkState;

            std::vector<ExpectedData> children{};
        };

        void checkTree(BaseTorrentFilesModel& model, const ExpectedData& expectedRootEntry) {
            checkEntryPresentation(model, {}, 0, expectedRootEntry, 0);
        }

        void checkEntryPresentation(
            BaseTorrentFilesModel& model, const QModelIndex& parent, int row, const ExpectedData& expected, int depth
        ) {
            info().log("{}* {}", u" "_s.repeated(depth), expected.name);

            const auto data = [&](BaseTorrentFilesModel::Column column, int role = Qt::DisplayRole) {
                return model.index(row, static_cast<int>(column), parent).data(role);
            };

            QCOMPARE(data(BaseTorrentFilesModel::Column::Name).toString(), expected.name);
            QCOMPARE(data(BaseTorrentFilesModel::Column::Size).toString(), formatByteSize(expected.size));
            QCOMPARE(data(BaseTorrentFilesModel::Column::Progress).toString(), formatProgress(expected.progress));
            QCOMPARE(
                data(BaseTorrentFilesModel::Column::Name, Qt::CheckStateRole).value<Qt::CheckState>(),
                expected.checkState
            );
            const auto expectedPriorityString = [&] {
                switch (expected.priority) {
                case TorrentFilesModelEntry::Priority::Low:
                    return qApp->translate("tremotesf", "Low");
                case TorrentFilesModelEntry::Priority::Normal:
                    return qApp->translate("tremotesf", "Normal");
                case TorrentFilesModelEntry::Priority::High:
                    return qApp->translate("tremotesf", "High");
                case TorrentFilesModelEntry::Priority::Mixed:
                    return qApp->translate("tremotesf", "Mixed");
                }
                return QString{};
            }();
            QCOMPARE(data(BaseTorrentFilesModel::Column::Priority).toString(), expectedPriorityString);

            const auto thisIndex = model.index(row, 0, parent);
            QCOMPARE(model.rowCount(thisIndex), static_cast<int>(expected.children.size()));
            if (!expected.children.empty()) {
                int i = 0;
                for (const auto& child : expected.children) {
                    checkEntryPresentation(model, thisIndex, i, child, depth + 1);
                    ++i;
                }
            }
        }
    };
}

QTEST_GUILESS_MAIN(tremotesf::TorrentFilesModelTest)

#include "torrentfilesmodel_test.moc"
