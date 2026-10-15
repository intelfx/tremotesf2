// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_CONTENTLAYOUTCOMBOBOX_H
#define TREMOTESF_CONTENTLAYOUTCOMBOBOX_H

#include <QComboBox>

#include "settings.h"

namespace tremotesf {
    // Selects how torrent's files are laid out in its location
    class ContentLayoutComboBox final : public QComboBox {
        Q_OBJECT

    public:
        explicit ContentLayoutComboBox(QWidget* parent = nullptr);

        Settings::TorrentContentLayout contentLayout() const;
        void setContentLayout(Settings::TorrentContentLayout layout);

        // Sets text of the item that keeps torrent's files where they are, which is "Original" by default
        void setOriginalLayoutText(const QString& text);
    };
}

#endif // TREMOTESF_CONTENTLAYOUTCOMBOBOX_H
