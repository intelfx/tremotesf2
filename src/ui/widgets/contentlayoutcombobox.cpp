// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contentlayoutcombobox.h"

#include <array>

#include <QCoreApplication>

#include "stdutils.h"

namespace tremotesf {
    namespace {
        constexpr std::array contentLayoutComboBoxItems{
            Settings::TorrentContentLayout::Original,
            Settings::TorrentContentLayout::CreateSubfolder,
            Settings::TorrentContentLayout::NoSubfolder
        };
    }

    ContentLayoutComboBox::ContentLayoutComboBox(QWidget* parent) : QComboBox(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        for (const auto contentLayout : contentLayoutComboBoxItems) {
            switch (contentLayout) {
            case Settings::TorrentContentLayout::Original:
                //: Content layout of added torrent: files are laid out as in the torrent file
                addItem(qApp->translate("tremotesf", "Original"));
                break;
            case Settings::TorrentContentLayout::CreateSubfolder:
                //: Content layout of added torrent: a torrent with a single file gets a folder for it
                addItem(qApp->translate("tremotesf", "Create subfolder"));
                break;
            case Settings::TorrentContentLayout::NoSubfolder:
                //: Content layout of added torrent: files are not put into the torrent's top-level folder
                addItem(qApp->translate("tremotesf", "Don't create subfolder"));
                break;
            }
        }
    }

    Settings::TorrentContentLayout ContentLayoutComboBox::contentLayout() const {
        if (currentIndex() == -1) {
            return Settings::TorrentContentLayout::Original;
        }
        return contentLayoutComboBoxItems.at(static_cast<size_t>(currentIndex()));
    }

    void ContentLayoutComboBox::setContentLayout(Settings::TorrentContentLayout layout) {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        setCurrentIndex(indexOfCasted<int>(contentLayoutComboBoxItems, layout).value());
    }
}
