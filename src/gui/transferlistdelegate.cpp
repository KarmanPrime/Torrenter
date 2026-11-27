/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "transferlistdelegate.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionViewItem>

#include "base/bittorrent/torrent.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"
#include "themetokens.h"
#include "torrentericons.h"
#include "transferlistmodel.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    bool isNumericColumn(const int column)
    {
        switch (column)
        {
        case TransferListModel::TR_SIZE:
        case TransferListModel::TR_TOTAL_SIZE:
        case TransferListModel::TR_SEEDS:
        case TransferListModel::TR_PEERS:
        case TransferListModel::TR_DLSPEED:
        case TransferListModel::TR_UPSPEED:
        case TransferListModel::TR_ETA:
        case TransferListModel::TR_RATIO:
        case TransferListModel::TR_RATIO_LIMIT:
        case TransferListModel::TR_POPULARITY:
        case TransferListModel::TR_QUEUE_POSITION:
        case TransferListModel::TR_AMOUNT_DOWNLOADED:
        case TransferListModel::TR_AMOUNT_UPLOADED:
        case TransferListModel::TR_AMOUNT_DOWNLOADED_SESSION:
        case TransferListModel::TR_AMOUNT_UPLOADED_SESSION:
        case TransferListModel::TR_AMOUNT_LEFT:
        case TransferListModel::TR_COMPLETED:
        case TransferListModel::TR_DLLIMIT:
        case TransferListModel::TR_UPLIMIT:
        case TransferListModel::TR_LAST_ACTIVITY:
        case TransferListModel::TR_AVAILABILITY:
        case TransferListModel::TR_REANNOUNCE:
            return true;
        default:
            return false;
        }
    }
}

namespace
{
    bool g_compactMode = false;
    QHash<int, qreal> &hoverFadeValues()
    {
        static QHash<int, qreal> values;
        return values;
    }
}

void TransferListDelegate::setCompactMode(bool enabled)
{
    g_compactMode = enabled;
}

bool TransferListDelegate::isCompactMode()
{
    return g_compactMode;
}

void TransferListDelegate::setHoverFade(const int row, const qreal alpha)
{
    hoverFadeValues()[row] = alpha;
}

qreal TransferListDelegate::hoverFade(const int row)
{
    return hoverFadeValues().value(row, 1.0);
}

TransferListDelegate::TransferListDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize TransferListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Design 01-main-dark: Comfortable density is the default. There is no
    // row-density preference in the settings yet, so 36 px is used for every
    // row (the QSS min-height matches this).
    Q_UNUSED(index);
    if (g_compactMode)
        return QSize(option.rect.width(), 56);
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(ThemeTokens::rowHeight);
    return size;
}

void TransferListDelegate::paintCompact(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto mode = ThemeTokens::currentMode();
    const QRect row = option.rect;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Causeful hover/selection fill, inset like every other row.
    if (option.state & QStyle::State_Selected)
    {
        QColor fill = ThemeTokens::color(ThemeTokens::Token::AccentSoft, mode);
        fill.setAlphaF(0.55);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(row.adjusted(4, 3, -4, -3), 8, 8);
    }
    else if (option.state & QStyle::State_MouseOver)
    {
        painter->setPen(Qt::NoPen);
        QColor hover = ThemeTokens::color(ThemeTokens::Token::Hover, mode);
        hover.setAlphaF(qBound<qreal>(0.0, 1.0, hoverFade(index.row())) * 0.65);
        painter->setBrush(hover);
        painter->drawRoundedRect(row.adjusted(4, 3, -4, -3), 8, 8);
    }

    const QModelIndex statusIdx = index.siblingAtColumn(TransferListModel::TR_STATUS);
    const qreal progress = index.siblingAtColumn(TransferListModel::TR_PROGRESS)
            .data(TransferListModel::UnderlyingDataRole).toReal();

    QString glyph = index.data(TransferListModel::FileTypeGlyphRole).toString();
    if (glyph.isEmpty())
        glyph = u"file"_s;
    TorrenterIcons::icon(glyph, ThemeTokens::color(ThemeTokens::Token::Text3, mode))
            .paint(painter, QRect(row.left() + 12, row.top() + 9, 16, 16));

    QFont font = option.font;
    font.setPixelSize(13);
    font.setWeight(QFont::Medium);
    QFontMetrics fm {font};
    const QRect nameRect = row.adjusted(36, 6, -110, -30);
    painter->setFont(font);
    painter->setPen(ThemeTokens::color(ThemeTokens::Token::Text1, mode));
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
        fm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, nameRect.width()));

    font.setPixelSize(12);
    font.setWeight(QFont::Normal);
    painter->setFont(font);
    painter->setPen(ThemeTokens::color(ThemeTokens::Token::Text3, mode));
    painter->drawText(row.adjusted(36, 6, -12, -30), Qt::AlignRight | Qt::AlignVCenter,
        index.siblingAtColumn(TransferListModel::TR_SIZE).data(Qt::DisplayRole).toString());

    // Line 2: 120px bar, percentage, live detail, state pill.
    const int barY = row.top() + 37;
    const QRect barRect {row.left() + 12, barY, 120, 4};
    const QColor stateColor = statusIdx.data(Qt::ForegroundRole).value<QColor>();
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeTokens::color(ThemeTokens::Token::Pressed, mode));
    painter->drawRoundedRect(barRect, 2, 2);
    const qreal displayed = ProgressBarPainter::lerped(static_cast<quint64>(index.row()) + 1, progress);
    if (displayed > 0)
    {
        painter->setBrush(stateColor.isValid() ? stateColor
            : ThemeTokens::color(ThemeTokens::Token::Accent, mode));
        painter->drawRoundedRect(QRectF(barRect.left(), barRect.top(),
            qMax<qreal>(2.0, barRect.width() * qMin<qreal>(1.0, displayed / 100.0)), 4), 2, 2);
    }

    const QString pct = index.siblingAtColumn(TransferListModel::TR_PROGRESS).data(Qt::DisplayRole).toString();
    painter->setPen(ThemeTokens::color(ThemeTokens::Token::Text2, mode));
    painter->drawText(QRect(barRect.right() + 8, barY - 7, 44, 18), Qt::AlignLeft | Qt::AlignVCenter, pct);

    QString detail;
    const QString down = index.siblingAtColumn(TransferListModel::TR_DLSPEED).data(Qt::DisplayRole).toString();
    const QString eta = index.siblingAtColumn(TransferListModel::TR_ETA).data(Qt::DisplayRole).toString();
    if (!down.isEmpty() && (down != C_INFINITY) && !down.startsWith(u'—'))
        detail = u"↓ "_s + down + (eta.isEmpty() ? QString() : (u" · "_s + eta));
    else
        detail = index.siblingAtColumn(TransferListModel::TR_RATIO).data(Qt::DisplayRole).toString();
    fm = QFontMetrics(font);
    const QRect detailRect = row.adjusted(barRect.right() + 56, 25, -105, -3);
    painter->setPen(ThemeTokens::color(ThemeTokens::Token::Text3, mode));
    painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter,
        fm.elidedText(detail, Qt::ElideRight, detailRect.width()));

    const QString stateText = statusIdx.data(Qt::DisplayRole).toString();
    fm = QFontMetrics(font);
    const int pillW = qMin(96, fm.horizontalAdvance(stateText) + 22);
    const QRect pill {row.right() - pillW - 12, barY - 7, pillW, 20};
    QColor fill = stateColor.isValid() ? stateColor : ThemeTokens::color(ThemeTokens::Token::Text3, mode);
    fill.setAlphaF(0.14);
    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRoundedRect(pill, 5, 5);
    painter->setPen(stateColor.isValid() ? stateColor : ThemeTokens::color(ThemeTokens::Token::Text2, mode));
    painter->drawText(pill.adjusted(8, 0, -8, 0), Qt::AlignHCenter | Qt::AlignVCenter, stateText);
    painter->restore();
}

void TransferListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (g_compactMode && (index.column() == TransferListModel::TR_NAME))
    {
        paintCompact(painter, option, index);
        return;
    }
    const auto mode = ThemeTokens::currentMode();
    const int column = index.column();

    // Name cell: 16px file-type glyph (text3), 10px gap, 13px/500 name.
    // State lives in the Status pill, not in an icon in the name cell.
    if (column == TransferListModel::TR_NAME)
    {
        const QModelIndex statusIndex = index.siblingAtColumn(TransferListModel::TR_STATUS);
        const auto state = statusIndex.data(TransferListModel::UnderlyingDataRole)
                .value<BitTorrent::TorrentState>();
        const qreal progress = index.siblingAtColumn(TransferListModel::TR_PROGRESS)
                .data(TransferListModel::UnderlyingDataRole).toReal();

        const bool muted = (state == BitTorrent::TorrentState::StoppedDownloading)
                || (state == BitTorrent::TorrentState::StoppedUploading)
                || (state == BitTorrent::TorrentState::QueuedDownloading)
                || (state == BitTorrent::TorrentState::QueuedUploading)
                || (((state == BitTorrent::TorrentState::Uploading)
                     || (state == BitTorrent::TorrentState::ForcedUploading))
                    && (progress >= 100));

        QString glyph = index.data(TransferListModel::FileTypeGlyphRole).toString();
        if (glyph.isEmpty())
            glyph = u"file"_s;

        const QRect cell = option.rect.adjusted(16, 0, -12, 0);
        const QIcon icon = TorrenterIcons::icon(glyph, ThemeTokens::color(ThemeTokens::Token::Text3, mode));
        icon.paint(painter, QRect(cell.left(), cell.center().y() - 8, 16, 16));

        QFont font = option.font;
        font.setPixelSize(13);
        font.setWeight(QFont::Medium);
        const QFontMetrics metrics {font};
        const QRect textRect = cell.adjusted(26, 0, 0, 0);
        const QString text = metrics.elidedText(index.data(Qt::DisplayRole).toString()
            , Qt::ElideRight, textRect.width());

        painter->save();
        painter->setFont(font);
        painter->setPen(ThemeTokens::color(muted ? ThemeTokens::Token::Text2
                                                 : ThemeTokens::Token::Text1, mode));
        painter->drawText(textRect, (Qt::AlignLeft | Qt::AlignVCenter), text);
        painter->restore();
        return;
    }

    // Numeric cells: zero/empty -> em dash, infinity -> ∞, both in text3.
    // Only the display is changed; the model's sort keys are untouched.
    if (isNumericColumn(column))
    {
        const QString valueText = index.data(Qt::DisplayRole).toString();
        // Empty means the "Hide zero and infinity values" preference is on:
        // keep hiding the value exactly as upstream does.
        if (valueText.isEmpty())
            return;

        const QVariant raw = index.data(TransferListModel::UnderlyingDataRole);
        const bool numeric = raw.isValid() && raw.canConvert<double>()
            && (raw.metaType().id() != QMetaType::QString);
        const double rawValue = numeric ? raw.toDouble() : 0.0;
        const bool infinite = valueText.contains(C_INFINITY);
        const bool zero = numeric && (rawValue == 0.0);

        QString text = valueText;
        if (infinite)
            text = C_INFINITY;
        else if (zero)
            text = u"\u2014"_s;   // em dash

        ThemeTokens::Token token = ThemeTokens::Token::Text2;
        if (infinite || zero)
            token = ThemeTokens::Token::Text3;
        else if ((column == TransferListModel::TR_DLSPEED) && (rawValue > 0))
            token = ThemeTokens::Token::Inkdown;
        else if ((column == TransferListModel::TR_UPSPEED) && (rawValue > 0))
            token = ThemeTokens::Token::Text1;

        const int alignment = index.data(Qt::TextAlignmentRole).toInt();
        const Qt::Alignment align = (alignment ? static_cast<Qt::Alignment>(alignment)
                                               : (Qt::AlignLeft | Qt::AlignVCenter));

        QFont font = option.font;
        font.setPixelSize(12);
        font.setWeight(QFont::Medium);

        painter->save();
        painter->setFont(font);
        painter->setPen(ThemeTokens::color(token, mode));
        painter->drawText(option.rect.adjusted(16, 0, -12, 0), align, text);
        painter->restore();
        return;
    }

    QStyleOptionViewItem padded {option};
    padded.rect = option.rect.adjusted(16, 0, -12, 0);  // design: 16px left cell padding

    switch (column)
    {
    case TransferListModel::TR_PROGRESS:
        {
            using namespace BitTorrent;

            const auto isEnableState = [](const TorrentState state) -> bool
            {
                switch (state)
                {
                case TorrentState::Error:
                case TorrentState::StoppedDownloading:
                case TorrentState::Unknown:
                    return false;
                default:
                    return true;
                }
            };

            const int progress = static_cast<int>(index.data(TransferListModel::UnderlyingDataRole).toReal());
            const QModelIndex statusIndex = index.siblingAtColumn(TransferListModel::TR_STATUS);
            const auto torrentState = statusIndex.data(TransferListModel::UnderlyingDataRole).value<TorrentState>();

            QStyleOptionViewItem customOption {padded};
            customOption.state.setFlag(QStyle::State_Enabled, isEnableState(torrentState));

            // Design override: progress fill follows the torrent state
            // (Downloading = mint, Seeding = violet, Stalled = amber).
            const QColor stateColor = statusIndex.data(Qt::ForegroundRole).value<QColor>();
            const bool striped = (torrentState == BitTorrent::TorrentState::DownloadingMetadata)
                || (torrentState == BitTorrent::TorrentState::ForcedDownloadingMetadata)
                || (torrentState == BitTorrent::TorrentState::CheckingDownloading)
                || (torrentState == BitTorrent::TorrentState::CheckingUploading)
                || (torrentState == BitTorrent::TorrentState::CheckingResumeData);
            m_progressBarPainter.paint(painter, customOption, index.data().toString(), progress, stateColor
                , striped, static_cast<quint64>(index.row()) + 1);
        }
        break;
    case TransferListModel::TR_STATUS:
        {
            const QColor stateColor = index.data(Qt::ForegroundRole).value<QColor>();
            if (!stateColor.isValid())
            {
                QStyledItemDelegate::paint(painter, padded, index);
                break;
            }

            const QRect pill = padded.rect.adjusted(0, 6, 0, -6);
            QColor fill {stateColor};
            fill.setAlphaF(0.12);

            QFont font = option.font;
            font.setPixelSize(12);
            font.setWeight(QFont::Medium);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
            painter->setBrush(stateColor);
            painter->drawEllipse(QRectF(pill.left() + 8, pill.center().y() - 3, 6, 6));
            painter->setPen(stateColor);
            painter->setFont(font);
            painter->drawText(pill.adjusted(22, 0, -8, 0)
                              , Qt::AlignLeft | Qt::AlignVCenter, index.data().toString());
            painter->restore();
        }
        break;
    default:
        QStyledItemDelegate::paint(painter, padded, index);
        break;
    }
}
