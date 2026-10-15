// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfilesview.h"

#include <algorithm>
#include <concepts>
#include <ranges>
#include <utility>

#include <QAction>
#include <QActionGroup>
#include <QCompleter>
#include <QCoreApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QPainter>

#include "rpc/mounteddirectoriesutils.h"
#include "rpc/serversettings.h"

#include "basetreeview.h"
#include "desktoputils.h"
#include "filemanagerlauncher.h"
#include "settings.h"
#include "rpc/rpc.h"
#include "rpc/torrent.h"
#include "ui/itemmodels/torrentfilesmodelentry.h"
#include "ui/itemmodels/torrentfilesproxymodel.h"
#include "ui/screens/addtorrent/localtorrentfilesmodel.h"
#include "ui/screens/torrentproperties/torrentfilesmodel.h"

#include "progressbardelegate.h"
#include "textinputdialog.h"
#include "tooltipwhenelideddelegate.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        template<std::invocable<const QModelIndex&> Function>
        void forEachIndex(const QAbstractItemModel* model, const QModelIndex& parent, Function&& function) {
            for (int row = 0, rows = model->rowCount(parent); row < rows; ++row) {
                const auto index = model->index(row, 0, parent);
                function(index);
                forEachIndex(model, index, function);
            }
        }

        // Folders that hold the entry, from the outermost one
        std::vector<QString> parentFolders(const QString& path) {
            std::vector<QString> folders{};
            for (qsizetype i = path.indexOf(u'/'); i != -1; i = path.indexOf(u'/', i + 1)) {
                folders.push_back(path.first(i));
            }
            return folders;
        }
    }

    TorrentFilesView::TorrentFilesView(LocalTorrentFilesModel* model, Rpc* rpc, QWidget* parent)
        : QTreeView(parent),
          mLocalFile(true),
          mModel(model),
          mProxyModel(new TorrentFilesProxyModel(
              mModel, LocalTorrentFilesModel::SortRole, model->columnNumber(BaseTorrentFilesModel::Column::Name), this
          )),
          mRpc(rpc) {
        init();
        setItemDelegate(new TooltipWhenElidedDelegate(this));
        if (!header()->restoreState(Settings::instance()->get_localTorrentFilesViewHeaderState())) {
            sortByColumn(model->columnNumber(BaseTorrentFilesModel::Column::Name), Qt::AscendingOrder);
        }
    }

    TorrentFilesView::TorrentFilesView(TorrentFilesModel* model, Rpc* rpc, QWidget* parent)
        : QTreeView(parent),
          mLocalFile(false),
          mModel(model),
          mProxyModel(new TorrentFilesProxyModel(
              mModel, TorrentFilesModel::SortRole, model->columnNumber(BaseTorrentFilesModel::Column::Name), this
          )),
          mRpc(rpc) {
        init();
        setItemDelegate(new TooltipWhenElidedDelegate(this));
        setItemDelegateForColumn(
            model->columnNumber(BaseTorrentFilesModel::Column::ProgressBar),
            new ProgressBarDelegate(TorrentFilesModel::SortRole, this)
        );
        if (!header()->restoreState(Settings::instance()->get_torrentFilesViewHeaderState())) {
            sortByColumn(model->columnNumber(BaseTorrentFilesModel::Column::Name), Qt::AscendingOrder);
        }

        QObject::connect(this, &TorrentFilesView::activated, this, [=, this](const auto& index) {
            const QModelIndex sourceIndex(mProxyModel->mapToSource(index));
            auto entry = static_cast<const TorrentFilesModelEntry*>(mProxyModel->mapToSource(index).internalPointer());
            if (!entry->isDirectory()
                && isServerLocalOrTorrentIsMounted(mRpc, static_cast<const TorrentFilesModel*>(mModel)->torrent())
                && entry->wantedState() != TorrentFilesModelEntry::WantedState::Unwanted) {
                desktoputils::openFile(
                    static_cast<const TorrentFilesModel*>(mModel)->localFilePath(sourceIndex),
                    nativeParentWidget()
                );
            }
        });
    }

    void TorrentFilesView::showFileRenameDialog(
        const QString& fileName, QWidget* parent, const std::function<void(const QString&)>& onAccepted
    ) {
        auto dialog = new TextInputDialog(
            //: Dialog title
            qApp->translate("tremotesf", "Rename"),
            qApp->translate("tremotesf", "File name:"),
            fileName,
            //: Dialog confirmation button
            qApp->translate("tremotesf", "Rename"),
            false,
            parent
        );
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dialog, &QDialog::accepted, parent, [=] { onAccepted(dialog->text()); });
        dialog->show();
    }

    void TorrentFilesView::saveState() {
        if (mLocalFile) {
            Settings::instance()->set_localTorrentFilesViewHeaderState(header()->saveState());
        } else {
            Settings::instance()->set_torrentFilesViewHeaderState(header()->saveState());
        }
    }

    void TorrentFilesView::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const {
        // Background of a row covers its branch indicators too
        if (const auto background = index.data(Qt::BackgroundRole); background.isValid()) {
            painter->fillRect(rect, background.value<QBrush>());
        }
        QTreeView::drawBranches(painter, rect, index);
    }

    void TorrentFilesView::init() {
        setCommonTreeViewProperties(this, false);
        setContextMenuPolicy(Qt::CustomContextMenu);
        setModel(mProxyModel);
        setSelectionMode(QAbstractItemView::ExtendedSelection);

        QObject::connect(mModel, &BaseTorrentFilesModel::modelAboutToBeReset, this, &TorrentFilesView::saveTreeState);
        QObject::connect(mModel, &BaseTorrentFilesModel::modelReset, this, &TorrentFilesView::onModelReset);
        QObject::connect(this, &TorrentFilesView::customContextMenuRequested, this, &TorrentFilesView::showContextMenu);

        onModelReset();
    }

    // The tree is built anew when torrent's files get new paths. Models that build it asynchronously
    // are reset to an empty tree first, so the state is restored on the next reset that has rows
    void TorrentFilesView::saveTreeState() {
        if (mModel->rowCount() == 0) {
            return;
        }
        TreeState state{.torrentId = mTreeTorrentId, .scrollPosition = verticalScrollBar()->value()};
        forEachIndex(mProxyModel, {}, [&](const QModelIndex& index) {
            if (mProxyModel->hasChildren(index)) {
                const QString path = entryPath(index);
                state.directories.insert(path);
                if (isExpanded(index)) {
                    state.expandedDirectories.insert(path);
                }
            }
        });
        for (const QModelIndex& index : selectionModel()->selectedRows()) {
            state.selectedEntries.insert(entryPath(index));
        }
        if (mPendingPathMoves) {
            const auto& moves = *mPendingPathMoves;
            state.expandedDirectories = applyPathMoves(std::as_const(state.expandedDirectories).values(), moves)
                                        | std::ranges::to<QSet<QString>>();
            state.selectedEntries.clear();
            for (const PathMove& move : moves) {
                state.selectedEntries.insert(move.to);
                for (QString& folder : parentFolders(move.to)) {
                    state.expandedDirectories.insert(std::move(folder));
                }
            }
            state.scrollToSelection = true;
            mPendingPathMoves.reset();
        }
        mSavedTreeState = std::move(state);
    }

    void TorrentFilesView::restoreTreeState(const TreeState& state) {
        QItemSelection selection{};
        forEachIndex(mProxyModel, {}, [&](const QModelIndex& index) {
            const QString path = entryPath(index);
            if (state.expandedDirectories.contains(path)) {
                expand(index);
            }
            if (state.selectedEntries.contains(path)) {
                selection.select(index, index);
            }
        });
        selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        updateGeometries();
        if (state.scrollToSelection && !selection.isEmpty()) {
            const auto index = selection.first().topLeft();
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            scrollTo(index);
        } else {
            verticalScrollBar()->setValue(state.scrollPosition);
        }
    }

    void TorrentFilesView::onModelReset() {
        const int topLevelRows = mModel->rowCount();
        if (topLevelRows > 0) {
            const bool hasDirectories = std::ranges::any_of(std::views::iota(0, topLevelRows), [&](int row) {
                return mModel->rowCount(mModel->index(row, 0)) > 0;
            });
            setRootIsDecorated(hasDirectories);
            auto savedTreeState = std::exchange(mSavedTreeState, std::nullopt);
            mTreeTorrentId = torrentId();
            if (savedTreeState && savedTreeState->torrentId != mTreeTorrentId) {
                savedTreeState.reset();
            }
            if (hasDirectories && topLevelRows == 1) {
                const auto topLevelIndex = mProxyModel->index(0, 0);
                // Unless it was already there
                if (!savedTreeState || !savedTreeState->directories.contains(entryPath(topLevelIndex))) {
                    expand(topLevelIndex);
                }
            }
            if (savedTreeState) {
                restoreTreeState(*savedTreeState);
            }
        }
    }

    std::optional<int> TorrentFilesView::torrentId() const {
        if (mLocalFile) {
            return std::nullopt;
        }
        const auto* const torrent = static_cast<const TorrentFilesModel*>(mModel)->torrent();
        return torrent ? std::optional(torrent->data().id) : std::nullopt;
    }

    QString TorrentFilesView::entryPath(const QModelIndex& index) const {
        return static_cast<const TorrentFilesModelEntry*>(mProxyModel->mapToSource(index).internalPointer())->path();
    }

    void TorrentFilesView::showContextMenu(QPoint pos) {
        if (!indexAt(pos).isValid()) {
            return;
        }

        const QModelIndexList sourceIndexes(mProxyModel->sourceIndexes(selectionModel()->selectedRows()));

        QMenu contextMenu;

        if (!mLocalFile) {
            bool show = true;
            for (const QModelIndex& index : sourceIndexes) {
                if (static_cast<const TorrentFilesModelEntry*>(index.internalPointer())->wantedState()
                    == TorrentFilesModelEntry::WantedState::Unwanted) {
                    show = false;
                    break;
                }
            }
            if (show) {
                const bool localOrMounted =
                    isServerLocalOrTorrentIsMounted(mRpc, static_cast<const TorrentFilesModel*>(mModel)->torrent());
                QAction* openAction = contextMenu.addAction(
                    QIcon::fromTheme("document-open"_L1),
                    //: Context menu item
                    qApp->translate("tremotesf", "&Open")
                );
                openAction->setEnabled(localOrMounted);
                QObject::connect(openAction, &QAction::triggered, this, [sourceIndexes, this] {
                    for (const QModelIndex& index : sourceIndexes) {
                        desktoputils::openFile(
                            static_cast<const TorrentFilesModel*>(mModel)->localFilePath(index),
                            nativeParentWidget()
                        );
                    }
                });

                QAction* openDownloadDirectoryAction = contextMenu.addAction(
                    QIcon::fromTheme("go-jump"_L1),
                    //: Context menu item
                    qApp->translate("tremotesf", "Open &Download Directory")
                );
                openDownloadDirectoryAction->setEnabled(localOrMounted);
                QObject::connect(openDownloadDirectoryAction, &QAction::triggered, this, [sourceIndexes, this] {
                    std::vector<QString> files{};
                    files.reserve(static_cast<size_t>(sourceIndexes.size()));
                    for (const QModelIndex& index : sourceIndexes) {
                        files.push_back(static_cast<const TorrentFilesModel*>(mModel)->localFilePath(index));
                    }
                    launchFileManagerAndSelectFiles(files, nativeParentWidget());
                });
            }
        }

        contextMenu.addSeparator();

        QAction* downloadAction = contextMenu.addAction(
            QIcon::fromTheme("download"_L1),
            //: Context menu item to select file for downloading
            qApp->translate("tremotesf", "&Download")
        );
        QObject::connect(downloadAction, &QAction::triggered, this, [=, this] {
            mModel->setFilesWanted(sourceIndexes, true);
        });

        QAction* notDownloadAction = contextMenu.addAction(
            QIcon::fromTheme("dialog-cancel"_L1),
            //: Context menu item to unselect file for downloading
            qApp->translate("tremotesf", "&Not Download")
        );
        QObject::connect(notDownloadAction, &QAction::triggered, this, [=, this] {
            mModel->setFilesWanted(sourceIndexes, false);
        });

        contextMenu.addSeparator();

        QMenu* priorityMenu = contextMenu.addMenu(qApp->translate("tremotesf", "&Priority"));
        QActionGroup priorityGroup(this);
        priorityGroup.setExclusive(true);

        //: File loading priority
        QAction* highPriorityAction = priorityGroup.addAction(qApp->translate("tremotesf", "&High"));
        highPriorityAction->setCheckable(true);
        QObject::connect(highPriorityAction, &QAction::triggered, this, [=, this](bool checked) {
            if (checked) {
                mModel->setFilesPriority(sourceIndexes, TorrentFilesModelEntry::Priority::High);
            }
        });

        //: File loading priority
        QAction* normalPriorityAction = priorityGroup.addAction(qApp->translate("tremotesf", "&Normal"));
        normalPriorityAction->setCheckable(true);
        QObject::connect(normalPriorityAction, &QAction::triggered, this, [=, this](bool checked) {
            if (checked) {
                mModel->setFilesPriority(sourceIndexes, TorrentFilesModelEntry::Priority::Normal);
            }
        });

        //: File loading priority
        QAction* lowPriorityAction = priorityGroup.addAction(qApp->translate("tremotesf", "&Low"));
        lowPriorityAction->setCheckable(true);
        QObject::connect(lowPriorityAction, &QAction::triggered, this, [=, this](bool checked) {
            if (checked) {
                mModel->setFilesPriority(sourceIndexes, TorrentFilesModelEntry::Priority::Low);
            }
        });

        //: File loading priority
        QAction* mixedPriorityAction = priorityGroup.addAction(qApp->translate("tremotesf", "Mixed"));
        mixedPriorityAction->setCheckable(true);
        mixedPriorityAction->setChecked(true);
        mixedPriorityAction->setVisible(false);

        priorityMenu->addActions(priorityGroup.actions());

        if (sourceIndexes.size() == 1) {
            auto entry = static_cast<const TorrentFilesModelEntry*>(sourceIndexes.first().internalPointer());
            if (entry->wantedState() == TorrentFilesModelEntry::WantedState::Wanted) {
                downloadAction->setEnabled(false);
            } else if (entry->wantedState() == TorrentFilesModelEntry::WantedState::Unwanted) {
                notDownloadAction->setEnabled(false);
            }

            switch (entry->priority()) {
            case TorrentFilesModelEntry::Priority::Low:
                lowPriorityAction->setChecked(true);
                break;
            case TorrentFilesModelEntry::Priority::Normal:
                normalPriorityAction->setChecked(true);
                break;
            case TorrentFilesModelEntry::Priority::High:
                highPriorityAction->setChecked(true);
                break;
            case TorrentFilesModelEntry::Priority::Mixed:
                mixedPriorityAction->setVisible(true);
            }
        }

        addFlattenAndExtractActions(contextMenu, sourceIndexes);

        if (mRpc->serverSettings()->data().canRenameFiles()) {
            contextMenu.addSeparator();
            QAction* renameAction = contextMenu.addAction(
                QIcon::fromTheme("edit-rename"_L1),
                //: Context menu item
                qApp->translate("tremotesf", "&Rename")
            );
            renameAction->setEnabled(sourceIndexes.size() == 1);
            QObject::connect(renameAction, &QAction::triggered, this, [=, this] {
                const QModelIndex& index = sourceIndexes.first();
                auto entry = static_cast<const TorrentFilesModelEntry*>(index.internalPointer());
                showFileRenameDialog(entry->name(), this, [=, this](const auto& newName) {
                    mModel->renameFile(index, newName);
                });
            });
        }

        addMoveAction(contextMenu, sourceIndexes);

        contextMenu.exec(viewport()->mapToGlobal(pos));
    }

    void TorrentFilesView::addFlattenAndExtractActions(QMenu& menu, const QModelIndexList& sourceIndexes) {
        if (!canSetFilePaths()) {
            return;
        }
        menu.addSeparator();

        QAction* flattenAction = menu.addAction(
            QIcon::fromTheme("format-indent-less"_L1),
            //: Context menu item, moves everything in selected folders out of them, removing the folders
            qApp->translate("tremotesf", "&Flatten")
        );
        flattenAction->setEnabled(std::ranges::all_of(sourceIndexes, [](const QModelIndex& index) {
            return static_cast<const TorrentFilesModelEntry*>(index.internalPointer())->isDirectory();
        }));
        QObject::connect(flattenAction, &QAction::triggered, this, [=, this] {
            const auto folders =
                sourceIndexes
                | std::views::transform([](const QModelIndex& index) {
                      return static_cast<const TorrentFilesModelEntry*>(index.internalPointer())->path();
                  })
                | std::ranges::to<std::vector>();
            const auto filePaths = mModel->currentFilePaths();
            auto moves = flattenFolders(filePaths, folders);
            if (const auto error = checkFilePaths(applyPathMoves(filePaths, moves)); error) {
                auto* const messageBox = new QMessageBox(
                    QMessageBox::Warning,
                    //: Dialog title
                    qApp->translate("tremotesf", "Can't flatten folders"),
                    filePathsErrorString(*error),
                    QMessageBox::Close,
                    this
                );
                messageBox->setAttribute(Qt::WA_DeleteOnClose);
                messageBox->show();
                return;
            }
            moveEntries(std::move(moves));
        });

        const auto entries = selectedEntries(sourceIndexes);
        QAction* extractAction = menu.addAction(
            QIcon::fromTheme("folder-new"_L1),
            //: Context menu item, moves selected files and folders into a new folder
            qApp->translate("tremotesf", "E&xtract into Subfolder...")
        );
        extractAction->setEnabled(std::ranges::all_of(entries, [&](const QString& path) {
            return parentFolder(path) == parentFolder(entries.front());
        }));
        QObject::connect(extractAction, &QAction::triggered, this, [=, this] {
            const auto movesFor = [=](const QString& folderName) {
                return extractIntoFolder(entries, folderName).value_or(std::vector<PathMove>{});
            };
            const bool singleFile =
                entries.size() == 1
                && !static_cast<const TorrentFilesModelEntry*>(sourceIndexes.first().internalPointer())->isDirectory();
            auto* const dialog = new TextInputDialog(
                //: Dialog title
                qApp->translate("tremotesf", "Extract into Subfolder"),
                qApp->translate("tremotesf", "Subfolder name:"),
                singleFile ? fileNameWithoutExtension(entryName(entries.front())) : QString{},
                //: Dialog confirmation button
                qApp->translate("tremotesf", "Extract"),
                false,
                this
            );
            dialog->setCheck([check = checkMoves(movesFor)](const QString& folderName) {
                return folderName.isEmpty() ? TextInputDialog::CheckResult{.acceptable = false} : check(folderName);
            });
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dialog, &QDialog::accepted, this, [=, this] { moveEntries(movesFor(dialog->text())); });
            dialog->show();
        });
    }

    bool TorrentFilesView::canSetFilePaths() const {
        return mRpc->isConnected() && mRpc->serverSettings()->data().canSetFilePaths();
    }

    std::vector<QString> TorrentFilesView::selectedEntries(const QModelIndexList& sourceIndexes) const {
        return topmostEntries(
            sourceIndexes
            | std::views::transform([](const QModelIndex& index) {
                  return static_cast<const TorrentFilesModelEntry*>(index.internalPointer())->path();
              })
            | std::ranges::to<std::vector>()
        );
    }

    void TorrentFilesView::moveEntries(std::vector<PathMove> moves) {
        auto paths = applyPathMoves(mModel->currentFilePaths(), moves);
        mPendingPathMoves = std::move(moves);
        mModel->setFilePaths(std::move(paths));
        if (mLocalFile) {
            // Model of a torrent being added has either rebuilt the tree already, or renamed its entries in place
            mPendingPathMoves.reset();
        }
    }

    TextInputDialog::Check
    TorrentFilesView::checkMoves(std::function<std::vector<PathMove>(const QString&)> movesForText) const {
        return [filePaths = mModel->currentFilePaths(), movesForText = std::move(movesForText)](const QString& text) {
            const auto newFilePaths = applyPathMoves(filePaths, movesForText(text));
            if (const auto error = checkFilePaths(newFilePaths); error) {
                return TextInputDialog::CheckResult{.acceptable = false, .message = filePathsErrorString(*error)};
            }
            const auto movedFiles =
                std::ranges::count_if(std::views::zip(filePaths, newFilePaths), [](const auto& paths) {
                    return std::get<0>(paths) != std::get<1>(paths);
                });
            if (movedFiles == 0) {
                return TextInputDialog::CheckResult{.acceptable = false};
            }
            return TextInputDialog::CheckResult{
                .acceptable = true,
                //: %Ln is number of torrent's files whose paths change
                .message =
                    qApp->translate("tremotesf", "%Ln files will be moved", nullptr, static_cast<int>(movedFiles))
            };
        };
    }

    void TorrentFilesView::addMoveAction(QMenu& menu, const QModelIndexList& sourceIndexes) {
        if (!canSetFilePaths()) {
            return;
        }
        QAction* moveAction = menu.addAction(
            QIcon::fromTheme("edit-move"_L1),
            //: Context menu item, moves selected files and folders
            qApp->translate("tremotesf", "&Move...")
        );
        QObject::connect(moveAction, &QAction::triggered, this, [entries = selectedEntries(sourceIndexes), this] {
            const auto filePaths = mModel->currentFilePaths();
            // One entry gets the path that the user enters, several ones go into the folder that the user enters
            const bool single = entries.size() == 1;
            const auto movesFor = [=](const QString& text) {
                return single ? std::vector{moveEntry(entries.front(), text)} : moveEntriesIntoFolder(entries, text);
            };
            auto* const dialog = new TextInputDialog(
                //: Dialog title
                qApp->translate("tremotesf", "Move"),
                single ? qApp->translate("tremotesf", "New path:")
                       //: Text field label, empty folder means torrent's location
                       : qApp->translate("tremotesf", "Move into folder (empty for the top level):"),
                single ? entries.front() : commonFolder(entries),
                //: Dialog confirmation button
                qApp->translate("tremotesf", "Move"),
                false,
                this
            );
            dialog->setCheck(checkMoves(movesFor));

            QStringList folders{};
            for (const QString& path : filePaths) {
                for (const QString& folder : parentFolders(path)) {
                    folders.push_back(folder + u'/');
                }
            }
            folders.removeDuplicates();
            auto* const lineEdit = dialog->lineEdit();
            lineEdit->setCompleter(new QCompleter(folders, dialog));
            if (single) {
                const auto nameStart = static_cast<int>(entries.front().lastIndexOf(u'/') + 1);
                lineEdit->setSelection(nameStart, static_cast<int>(entries.front().size()) - nameStart);
            }

            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dialog, &QDialog::accepted, this, [=, this] { moveEntries(movesFor(dialog->text())); });
            dialog->show();
        });
    }
}
