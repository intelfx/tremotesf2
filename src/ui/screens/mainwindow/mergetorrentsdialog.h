// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_MERGETORRENTSDIALOG_H
#define TREMOTESF_MERGETORRENTSDIALOG_H

#include <QDialog>

namespace tremotesf {
    class Rpc;
    class Torrent;

    class MergeTorrentsDialog : public QDialog {
        Q_OBJECT
    public:
        explicit MergeTorrentsDialog(const Torrent* source, const Torrent* target, Rpc* rpc, QWidget* parent);
    };
}

#endif // TREMOTESF_MERGETORRENTSDIALOG_H
