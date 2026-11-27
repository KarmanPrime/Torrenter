/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "trackerlistitemdelegate.h"

#include <QModelIndex>
#include <QPainter>
#include <QPainterPath>
#include <QFont>

#include "gui/themetokens.h"
#include "trackerlistmodel.h"
#include "trackerlistwidget.h"

TrackerListItemDelegate::TrackerListItemDelegate(TrackerListWidget *view)
    : QStyledItemDelegate(view)
    , m_view {view}
{
    Q_ASSERT(m_view);
}

void TrackerListItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() != TrackerListModel::COL_STATUS)
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem base {option};
    base.text.clear();
    base.features &= ~QStyleOptionViewItem::HasDisplay;
    QStyledItemDelegate::paint(painter, base, index);

    const QString status = index.data().toString();
    const auto mode = ThemeTokens::currentMode();
    ThemeTokens::Token token = ThemeTokens::Token::Text3;
    if (status == tr("Working"))
        token = ThemeTokens::Token::StDown;
    else if (status == tr("Updating..."))
        token = ThemeTokens::Token::StCheck;
    else if (status == tr("Not working"))
        token = ThemeTokens::Token::StError;
    else if (status == tr("Not contacted yet"))
        token = ThemeTokens::Token::StPaused;

    const QColor ink = ThemeTokens::color(token, mode);
    const QRect pill {option.rect.left() + 6, option.rect.top() + 7, option.rect.width() - 14, 20};
    QPainterPath path;
    path.addRoundedRect(pill, 5, 5);
    painter->setPen(QPen(ink, 1));
    painter->setBrush(QColor(ink.red(), ink.green(), ink.blue(), 20));
    painter->drawPath(path);
    QFont font = option.font;
    font.setPixelSize(11);
    font.setWeight(QFont::DemiBold);
    painter->setFont(font);
    painter->setPen(ink);
    painter->drawText(pill, Qt::AlignCenter, status);
}

void TrackerListItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    if (index.parent().isValid() || !m_view->isExpanded(index.siblingAtColumn(0)))
        return;

    switch (index.column())
    {
    case TrackerListModel::COL_PEERS:
    case TrackerListModel::COL_SEEDS:
    case TrackerListModel::COL_LEECHES:
    case TrackerListModel::COL_TIMES_DOWNLOADED:
    case TrackerListModel::COL_MSG:
    case TrackerListModel::COL_NEXT_ANNOUNCE:
    case TrackerListModel::COL_MIN_ANNOUNCE:
        option->text.clear();
        break;
    default:
        break;
    }
}
