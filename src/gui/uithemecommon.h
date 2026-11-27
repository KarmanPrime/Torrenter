/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#pragma once

#include <QColor>
#include <QHash>
#include <QPalette>
#include <QSet>
#include <QString>

#include "base/global.h"
#include "color.h"
#include "themetokens.h"

inline const QString CONFIG_FILE_NAME = u"config.json"_s;
inline const QString STYLESHEET_FILE_NAME = u"stylesheet.qss"_s;
inline const QString KEY_COLORS = u"colors"_s;
inline const QString KEY_COLORS_LIGHT = u"colors.light"_s;
inline const QString KEY_COLORS_DARK = u"colors.dark"_s;

struct UIThemeColor
{
    QColor light;
    QColor dark;
};

inline QHash<QString, UIThemeColor> defaultUIThemeColors()
{
    QHash<QString, UIThemeColor> result;
    result.reserve(30);
    for (const QString &id : {u"Log.BannedPeer"_s, u"Log.Critical"_s, u"Log.Info"_s, u"Log.TimeStamp"_s, u"Log.Warning"_s, u"PiecesBar.Border"_s, u"PiecesBar.MissingPiece"_s, u"PiecesBar.PartialPiece"_s, u"PiecesBar.Piece"_s, u"ProgressBar"_s, u"RSS.ReadArticle"_s, u"RSS.UnreadArticle"_s, u"TransferList.CheckingDownloading"_s, u"TransferList.CheckingResumeData"_s, u"TransferList.CheckingUploading"_s, u"TransferList.Downloading"_s, u"TransferList.DownloadingMetadata"_s, u"TransferList.Error"_s, u"TransferList.ForcedDownloading"_s, u"TransferList.ForcedDownloadingMetadata"_s, u"TransferList.ForcedUploading"_s, u"TransferList.MissingFiles"_s, u"TransferList.Moving"_s, u"TransferList.QueuedDownloading"_s, u"TransferList.QueuedUploading"_s, u"TransferList.StalledDownloading"_s, u"TransferList.StalledUploading"_s, u"TransferList.StoppedDownloading"_s, u"TransferList.StoppedUploading"_s, u"TransferList.Uploading"_s})
    {
        result.insert(id, {ThemeTokens::colorById(id, ThemeTokens::Mode::Light)
                           , ThemeTokens::colorById(id, ThemeTokens::Mode::Dark)});
    }
    return result;
}

inline QHash<QString, UIThemeColor> defaultPaletteColors()
{
    QHash<QString, UIThemeColor> result;
    result.reserve(25);
    for (const QString &id : {u"Palette.AlternateBase"_s, u"Palette.Base"_s, u"Palette.BrightText"_s, u"Palette.BrightTextDisabled"_s, u"Palette.Button"_s, u"Palette.ButtonText"_s, u"Palette.ButtonTextDisabled"_s, u"Palette.Dark"_s, u"Palette.Highlight"_s, u"Palette.HighlightedText"_s, u"Palette.HighlightedTextDisabled"_s, u"Palette.Light"_s, u"Palette.Link"_s, u"Palette.LinkVisited"_s, u"Palette.Mid"_s, u"Palette.Midlight"_s, u"Palette.Shadow"_s, u"Palette.Text"_s, u"Palette.TextDisabled"_s, u"Palette.ToolTipBase"_s, u"Palette.ToolTipText"_s, u"Palette.ToolTipTextDisabled"_s, u"Palette.Window"_s, u"Palette.WindowText"_s, u"Palette.WindowTextDisabled"_s})
    {
        result.insert(id, {ThemeTokens::colorById(id, ThemeTokens::Mode::Light)
                           , ThemeTokens::colorById(id, ThemeTokens::Mode::Dark)});
    }
    return result;
}

inline QSet<QString> defaultUIThemeIcons()
{
    return {
        u"application-exit"_s,
        u"application-rss"_s,
        u"application-url"_s,
        u"browser-cookies"_s,
        u"chart-line"_s,
        u"checked-completed"_s,
        u"configure"_s,
        u"connected"_s,
        u"dialog-warning"_s,
        u"directory"_s,
        u"disconnected"_s,
        u"download"_s,
        u"downloading"_s,
        u"edit-clear"_s,
        u"edit-copy"_s,
        u"edit-find"_s,
        u"edit-rename"_s,
        u"error"_s,
        u"fileicon"_s,
        u"filter-active"_s,
        u"filter-all"_s,
        u"filter-inactive"_s,
        u"filter-stalled"_s,
        u"firewalled"_s,
        u"folder-documents"_s,
        u"folder-new"_s,
        u"folder-remote"_s,
        u"force-recheck"_s,
        u"go-bottom"_s,
        u"go-down"_s,
        u"go-top"_s,
        u"go-up"_s,
        u"hash"_s,
        u"help-about"_s,
        u"help-contents"_s,
        u"insert-link"_s,
        u"ip-blocked"_s,
        u"list-add"_s,
        u"list-remove"_s,
        u"loading"_s,
        u"mail-inbox"_s,
        u"name"_s,
        u"network-connect"_s,
        u"network-server"_s,
        u"object-locked"_s,
        u"pause-session"_s,
        u"paused"_s,
        u"peers"_s,
        u"peers-add"_s,
        u"peers-remove"_s,
        u"plugins"_s,
        u"preferences-advanced"_s,
        u"preferences-bittorrent"_s,
        u"preferences-desktop"_s,
        u"preferences-webui"_s,
        u"qbittorrent-tray"_s,
        u"qbittorrent-tray-mono"_s,
        u"queued"_s,
        u"ratio"_s,
        u"reannounce"_s,
        u"rss_read_article"_s,
        u"rss_unread_article"_s,
        u"security-high"_s,
        u"security-low"_s,
        u"set-location"_s,
        u"slow"_s,
        u"slow_off"_s,
        u"speedometer"_s,
        u"stalledDL"_s,
        u"stalledUP"_s,
        u"stopped"_s,
        u"system-log-out"_s,
        u"tags"_s,
        u"task-complete"_s,
        u"task-reject"_s,
        u"torrent-creator"_s,
        u"torrent-magnet"_s,
        u"torrent-start"_s,
        u"torrent-start-forced"_s,
        u"torrent-stop"_s,
        u"tracker-error"_s,
        u"tracker-warning"_s,
        u"trackerless"_s,
        u"trackers"_s,
        u"upload"_s,
        u"view-categories"_s,
        u"view-preview"_s,
        u"view-refresh"_s,
        u"view-statistics"_s,
        u"wallet-open"_s
    };
}
