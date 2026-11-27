/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Vladimir Golovnev <glassez@yandex.ru>
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
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "torrentcontentitemdelegate.h"

#include <QComboBox>
#include <QFont>
#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QSize>
#include <QTreeView>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/torrent.h"
#include "gui/themetokens.h"
#include "gui/torrentericons.h"
#include "gui/torrentcontentmodel.h"
#include "gui/torrentcontentmodelitem.h"

using namespace Qt::Literals::StringLiterals;

TorrentContentItemDelegate::TorrentContentItemDelegate(QWidget *parent)
    : QStyledItemDelegate(parent)
{
}

void TorrentContentItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto *combobox = static_cast<QComboBox *>(editor);
    // Set combobox index
    const int priority = index.data(TorrentContentModel::UnderlyingDataRole).toInt();
    switch (static_cast<BitTorrent::DownloadPriority>(priority))
    {
    case BitTorrent::DownloadPriority::Ignored:
        combobox->setCurrentIndex(0);
        break;
    case BitTorrent::DownloadPriority::High:
        combobox->setCurrentIndex(2);
        break;
    case BitTorrent::DownloadPriority::Maximum:
        combobox->setCurrentIndex(3);
        break;
    case BitTorrent::DownloadPriority::Mixed:
        combobox->setCurrentIndex(4);
        break;
    default:
        combobox->setCurrentIndex(1);
        break;
    }
}

QWidget *TorrentContentItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.column() != TorrentContentModelItem::COL_PRIO)
        return nullptr;

    auto *editor = new QComboBox(parent);
    editor->setAutoFillBackground(true);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->addItem(tr("Do not download", "Do not download (priority)"));
    editor->addItem(tr("Normal", "Normal (priority)"));
    editor->addItem(tr("High", "High (priority)"));
    editor->addItem(tr("Maximum", "Maximum (priority)"));

    // add Mixed priority item to the new combobox only for those items with Mixed priority
    const auto priority = static_cast<BitTorrent::DownloadPriority>(index.data(TorrentContentModel::UnderlyingDataRole).toInt());
    if (priority == BitTorrent::DownloadPriority::Mixed)
    {
        editor->addItem(tr("Mixed", "Mixed (priorities)"));
    }

    connect(editor, &QComboBox::currentIndexChanged, this, [this, editor]()
    {
        emit const_cast<TorrentContentItemDelegate *>(this)->commitData(editor);
    });

    return editor;
}

QSize TorrentContentItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid())
        return QSize(option.rect.width(), 34);
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(34);
    return size;
}

void TorrentContentItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    const auto *combobox = static_cast<QComboBox *>(editor);

    BitTorrent::DownloadPriority prio = BitTorrent::DownloadPriority::Normal; // NORMAL
    switch (combobox->currentIndex())
    {
    case 0:
        prio = BitTorrent::DownloadPriority::Ignored; // IGNORED
        break;
    case 2:
        prio = BitTorrent::DownloadPriority::High; // HIGH
        break;
    case 3:
        prio = BitTorrent::DownloadPriority::Maximum; // MAX
        break;
    case 4:
        prio = BitTorrent::DownloadPriority::Mixed; // MIXED
        break;
    }

    const int newPriority = static_cast<int>(prio);
    const int previousPriority = index.data(TorrentContentModel::UnderlyingDataRole).toInt();

    if (newPriority != previousPriority)
    {
        model->setData(index, newPriority);
    }
}

void TorrentContentItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

void TorrentContentItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto mode = ThemeTokens::currentMode();
    const auto color = [mode](const ThemeTokens::Token token) { return ThemeTokens::color(token, mode); };
    const auto *view = qobject_cast<const QTreeView *>(parent());

    switch (index.column())
    {
    case TorrentContentModelItem::COL_NAME:
        {
            // Base row painting (selection/hover/rounded rect) without native
            // branch, checkbox or text; we draw those ourselves below.
            QStyleOptionViewItem base {option};
            base.text.clear();
            base.features &= ~(QStyleOptionViewItem::HasDecoration
                | QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDisplay);
            QStyledItemDelegate::paint(painter, base, index);

            const bool isFolder = index.model()->hasChildren(index);
            const bool expanded = (view != nullptr) && view->isExpanded(index);
            int x = option.rect.left() + 6;
            for (QModelIndex ancestor = index.parent(); ancestor.isValid(); ancestor = ancestor.parent())
                x += 20;

            if (isFolder)
            {
                const QIcon chevron = TorrenterIcons::icon(
                    expanded ? u"chevron-down"_s : u"chevron-right"_s, ThemeTokens::Token::Text3, mode);
                chevron.paint(painter, QRect(x, option.rect.top() + 10, 14, 14));
                x += 18;
            }

            // 16 px tri-state checkbox.
            const int checkState = index.data(Qt::CheckStateRole).toInt();
            const QRect checkRect {x, option.rect.top() + 9, 16, 16};
            QPainterPath box;
            box.addRoundedRect(checkRect, 4, 4);
            painter->setPen(QPen(color(ThemeTokens::Token::LineStrong), 1));
            painter->setBrush((checkState == Qt::Checked) ? color(ThemeTokens::Token::Accent)
                : ((checkState == Qt::PartiallyChecked) ? color(ThemeTokens::Token::Pressed) : Qt::transparent));
            painter->drawPath(box);
            if (checkState == Qt::Checked)
            {
                const QIcon tick = TorrenterIcons::icon(u"check"_s, ThemeTokens::Token::OnAccent, mode);
                tick.paint(painter, checkRect.adjusted(2, 2, -2, -2));
            }
            else if (checkState == Qt::PartiallyChecked)
            {
                const QIcon dash = TorrenterIcons::icon(u"minus"_s, ThemeTokens::Token::Text2, mode);
                dash.paint(painter, checkRect.adjusted(2, 2, -2, -2));
            }
            x += 22;

            const QString iconId = isFolder ? (expanded ? u"folder-open"_s : u"folder"_s) : u"file"_s;
            TorrenterIcons::icon(iconId, ThemeTokens::Token::Text3, mode)
                .paint(painter, QRect(x, option.rect.top() + 9, 16, 16));
            x += 21;

            QFont font = option.font;
            font.setPixelSize(12);
            font.setWeight(isFolder ? QFont::Medium : QFont::Normal);
            painter->setFont(font);
            painter->setPen(color(isFolder ? ThemeTokens::Token::Text1 : ThemeTokens::Token::Text2));
            painter->drawText(QRect(x, option.rect.top(), option.rect.right() - x - 6, option.rect.height())
                , Qt::AlignLeft | Qt::AlignVCenter, index.data().toString());
        }
        break;
    case TorrentContentModelItem::COL_PROGRESS:
        {
            const int progress = static_cast<int>(index.data(TorrentContentModel::UnderlyingDataRole).toReal());
            const int priority = index.sibling(index.row(), TorrentContentModelItem::COL_PRIO).data(TorrentContentModel::UnderlyingDataRole).toInt();
            const bool isEnabled = static_cast<BitTorrent::DownloadPriority>(priority) != BitTorrent::DownloadPriority::Ignored;
            QStyleOptionViewItem customOption {option};
            customOption.state.setFlag(QStyle::State_Enabled, isEnabled);
            m_progressBarPainter.paint(painter, customOption, index.data().toString(), progress);
        }
        break;
    case TorrentContentModelItem::COL_PRIO:
        {
            QStyleOptionViewItem base {option};
            base.text.clear();
            base.features &= ~QStyleOptionViewItem::HasDisplay;
            QStyledItemDelegate::paint(painter, base, index);
            const auto priority = static_cast<BitTorrent::DownloadPriority>(
                index.data(TorrentContentModel::UnderlyingDataRole).toInt());
            QString text;
            ThemeTokens::Token token = ThemeTokens::Token::Text2;
            switch (priority)
            {
            case BitTorrent::DownloadPriority::Ignored:
                text = tr("Do not download"); token = ThemeTokens::Token::Text3; break;
            case BitTorrent::DownloadPriority::High:
                text = tr("High"); token = ThemeTokens::Token::AccentText; break;
            case BitTorrent::DownloadPriority::Maximum:
                text = tr("Maximum"); token = ThemeTokens::Token::Inkstalled; break;
            case BitTorrent::DownloadPriority::Mixed:
                text = tr("Mixed"); token = ThemeTokens::Token::Text2; break;
            default:
                text = tr("Normal"); token = ThemeTokens::Token::Text2; break;
            }
            const QRect pill {option.rect.left() + 6, option.rect.top() + (option.rect.height() - 24) / 2
                , option.rect.width() - 18, 24};
            const QColor ink = color(token);
            QPainterPath shape;
            shape.addRoundedRect(pill, 6, 6);
            painter->setPen(QPen(ink, 1));
            painter->setBrush(ink);
            painter->drawPath(shape);
            QFont font = option.font;
            font.setPixelSize(12);
            font.setWeight(QFont::Medium);
            painter->setFont(font);
            painter->setPen(color(ThemeTokens::Token::Raised));
            painter->drawText(pill.adjusted(8, 0, -20, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
            TorrenterIcons::icon(u"chevrons-up-down"_s, ThemeTokens::Token::Text4, mode)
                .paint(painter, QRect(pill.right() - 18, pill.top() + 5, 12, 12));
        }
        break;
    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }
}
