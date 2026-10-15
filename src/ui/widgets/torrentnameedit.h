// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_TORRENTNAMEEDIT_H
#define TREMOTESF_TORRENTNAMEEDIT_H

#include <vector>

#include <QLineEdit>

namespace tremotesf {
    class LocalTorrentFilesModel;

    /**
     * Name of the torrent whose files are in the model, i.e. of its top-level folder or file. Editing it renames them,
     * along with files named after it, right away. It is reset whenever the files get new paths otherwise.
     */
    class TorrentNameEdit final : public QLineEdit {
        Q_OBJECT

    public:
        explicit TorrentNameEdit(LocalTorrentFilesModel* model, QWidget* parent = nullptr);

        // Whether the files have a single top-level folder or file, without which there is no name to edit
        bool hasTitle() const { return mHasTitle; }
        // Whether the text is not a name that the files can get, in which case they keep their paths
        bool hasError() const;

    private:
        void reset();
        void onTextEdited(const QString& name);

        LocalTorrentFilesModel* mModel;
        QAction* mErrorAction;
        // Paths of the files before the text renamed them
        std::vector<QString> mBaseFilePaths{};
        bool mHasTitle{};
        bool mRenamingFiles{};

    signals:
        // hasTitle() or hasError() may have changed
        void stateChanged();
    };
}

#endif // TREMOTESF_TORRENTNAMEEDIT_H
