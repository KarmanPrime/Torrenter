/* Torrenter transfer-list header (HANDOFF 5.1).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "transferlistheaderview.h"

#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QRect>

#include "torrentericons.h"
#include "transferlistmodel.h"

using namespace Qt::Literals::StringLiterals;

TransferListHeaderView::TransferListHeaderView(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
    setFixedHeight(34);
}

    void TransferListHeaderView::paintSection(QPainter *painter, const QRect &rect, const int logicalIndex) const
    {
        if (!rect.isValid())
            return;

        const auto mode = ThemeTokens::currentMode();
        painter->save();

        // Flat header surface: no vertical separators are drawn.
        painter->fillRect(rect, ThemeTokens::color(ThemeTokens::Token::Surface, mode));

        // Design 01 sorts by Name. If no indicator was ever set (default
        // profile), still present Name as the sorted column.
        const int indicator = sortIndicatorSection();
        const bool hasIndicator = (indicator >= 0) && (indicator < count()) && !isSectionHidden(indicator);
        const int sortedSection = hasIndicator ? indicator : static_cast<int>(TransferListModel::TR_NAME);
        const Qt::SortOrder order = hasIndicator ? sortIndicatorOrder() : Qt::AscendingOrder;
        const bool sorted = (sortedSection == logicalIndex);
        const bool chooserSlot = (logicalIndex == lastVisibleSection());

        QFont font;
        font.setFamily(QStringLiteral("Inter"));
        font.setPixelSize(12);
        font.setWeight(QFont::Medium);  // 500
        painter->setFont(font);
        painter->setPen(ThemeTokens::color(sorted
            ? ThemeTokens::Token::Text1 : ThemeTokens::Token::Text3, mode));

        // Reserve the chooser slot on the right and 1px for the underline.
        QRect textRect = rect.adjusted(8, 0, chooserSlot ? -36 : -8, -1);
        const int chevronWidth = sorted ? 12 : 0;
        const int gap = sorted ? 5 : 0;
        const QFontMetrics metrics {font};
        const QString label = model()->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString();
        const QString text = metrics.elidedText(label, Qt::ElideRight,
            qMax(0, textRect.width() - chevronWidth - gap));

        const Qt::Alignment alignment = (isNumeric(logicalIndex) ? Qt::AlignRight : Qt::AlignLeft)
            | Qt::AlignVCenter;
        if (!sorted)
        {
            painter->drawText(textRect, alignment, text);
        }
        else
        {
            QRect labelRect = textRect;
            labelRect.setWidth(qMax(0, textRect.width() - chevronWidth - gap));
            painter->drawText(labelRect, alignment, text);

            const int chevronX = isNumeric(logicalIndex)
                ? textRect.right() - chevronWidth
                : textRect.left() + metrics.horizontalAdvance(text) + gap;
            const QRect chevronRect {chevronX, textRect.center().y() - 6, chevronWidth, 12};
            const QString glyph = (order == Qt::AscendingOrder)
                ? QStringLiteral("go-up") : QStringLiteral("go-down");
            TorrenterIcons::icon(glyph, ThemeTokens::Token::Text1, mode).paint(painter, chevronRect);
        }

        painter->fillRect(QRect(rect.left(), rect.bottom() - 1, rect.width(), 1),
            ThemeTokens::color(ThemeTokens::Token::LineSubtle, mode));
        painter->restore();
    }

    bool TransferListHeaderView::isNumeric(const int column)
    {
        switch (column)
        {
        case TransferListModel::TR_SIZE:
        case TransferListModel::TR_SEEDS:
        case TransferListModel::TR_PEERS:
        case TransferListModel::TR_DLSPEED:
        case TransferListModel::TR_UPSPEED:
        case TransferListModel::TR_ETA:
        case TransferListModel::TR_RATIO:
            return true;
        default:
            return false;
        }
    }

    int TransferListHeaderView::lastVisibleSection() const
    {
        int bestLogical = -1;
        int bestVisual = -1;
        for (int i = 0; i < count(); ++i)
        {
            if (isSectionHidden(i))
                continue;
            const int visual = visualIndex(i);
            if (visual > bestVisual)
            {
                bestVisual = visual;
                bestLogical = i;
            }
        }
        return bestLogical;
    }

