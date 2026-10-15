// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentnameedit.h"

#include <ranges>

#include <QAction>
#include <QCoreApplication>
#include <QIcon>

#include "rpc/torrentfilepaths.h"
#include "ui/screens/addtorrent/localtorrentfilesmodel.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    TorrentNameEdit::TorrentNameEdit(LocalTorrentFilesModel* model, QWidget* parent)
        : QLineEdit(parent),
          mModel(model),
          mErrorAction(addAction(QIcon::fromTheme("dialog-error"_L1), QLineEdit::TrailingPosition)) {
        mErrorAction->setVisible(false);
        QObject::connect(this, &QLineEdit::textEdited, this, &TorrentNameEdit::onTextEdited);
        QObject::connect(mModel, &LocalTorrentFilesModel::filePathsChanged, this, &TorrentNameEdit::reset);
    }

    bool TorrentNameEdit::hasError() const { return mErrorAction->isVisible(); }

    void TorrentNameEdit::reset() {
        if (mRenamingFiles) {
            return;
        }
        mBaseFilePaths = mModel->currentFilePaths();
        mBaseHighlightedFiles = mModel->highlightedFiles();
        const auto title = torrentTitle(mBaseFilePaths);
        mHasTitle = title.has_value();
        setText(title.value_or(QString{}));
        setToolTip(
            title ? QString{} : qApp->translate("tremotesf", "Torrent's files have no single top-level folder or file")
        );
        mErrorAction->setVisible(false);
        emit stateChanged();
    }

    void TorrentNameEdit::onTextEdited(const QString& name) {
        const auto error = name.isEmpty() ? qApp->translate("tremotesf", "Name can't be empty")
                                          : torrentTitleError(mBaseFilePaths, name);
        auto filePaths =
            error ? mBaseFilePaths : applyPathMoves(mBaseFilePaths, renameTorrentTitle(mBaseFilePaths, name).value());
        auto highlightedFiles = mBaseHighlightedFiles;
        for (const auto& [id, path, basePath] : std::views::zip(std::views::iota(0), filePaths, mBaseFilePaths)) {
            if (entryName(path) != entryName(basePath)) {
                highlightedFiles.insert(id);
            }
        }
        mRenamingFiles = true;
        mModel->setFilePaths(std::move(filePaths));
        mRenamingFiles = false;
        mModel->setHighlightedFiles(std::move(highlightedFiles));
        mErrorAction->setVisible(error.has_value());
        mErrorAction->setToolTip(error.value_or(QString{}));
        emit stateChanged();
    }
}
