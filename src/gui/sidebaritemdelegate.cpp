#include "sidebaritemdelegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QListView>
#include <QLocale>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QRegularExpression>
#include <QStyleHints>

#include "base/logger.h"
#include "base/torrentfilter.h"
#include "themetokens.h"
#include "transferlistfilters/statusfilterwidget.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    const QRegularExpression COUNT_PATTERN {u"\\s*\\((\\d+)\\)$"_s};


}

namespace
{
    bool isStatusSection(const QWidget *widget)
    {
        for (const QWidget *w = widget; w; w = w->parentWidget())
        {
            if (qobject_cast<const StatusFilterWidget *>(w))
                return true;
            if (w->property("torrenterDefaultActive").toBool())
                return true;
        }
        return false;
    }

    const QListView *parentListView(const QWidget *widget)
    {
        for (const QWidget *w = widget; w; w = w->parentWidget())
        {
            if (const auto *view = qobject_cast<const QListView *>(w))
                return view;
        }
        return nullptr;
    }

    bool isErroredStatusRow(const QWidget *widget, const int row)
    {
        if (row != TorrentFilter::Errored)
            return false;

        for (const QWidget *w = widget; w; w = w->parentWidget())
        {
            if (qobject_cast<const StatusFilterWidget *>(w))
                return true;
        }
        return false;
    }
}

QString SidebarItemDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    QString text = QStyledItemDelegate::displayText(value, locale);
    const QRegularExpressionMatch match = COUNT_PATTERN.match(text);
    if (match.hasMatch())
        text.truncate(match.capturedStart());
    return text;
}

QSize SidebarItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Indented children of Stalled are 28 tall (design 01-main-dark); other
    // sidebar rows keep the 30 px comfortable height.
    size.setHeight(index.data(Qt::UserRole).toBool() ? 28 : ThemeTokens::sidebarRowHeight);
    return size;
}

void SidebarItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool inStatus = isStatusSection(option.widget);
    const bool stalledChild = inStatus && index.data(Qt::UserRole).toBool();
    const QListView *view = parentListView(option.widget);
    const bool hasSelection = view && view->selectionModel() && view->selectionModel()->hasSelection();
    const bool isCurrent = view && (view->currentIndex() == index);
    const bool isAll = (displayText(index.data(Qt::DisplayRole), QLocale()) == u"All"_s);
    const bool active = inStatus && (selected || isCurrent || (!hasSelection && isAll));

    const auto mode = ThemeTokens::currentMode();

    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_Selected;
    opt.palette.setColor(QPalette::Base, Qt::transparent);
    opt.palette.setColor(QPalette::AlternateBase, Qt::transparent);
    opt.palette.setColor(QPalette::Highlight, Qt::transparent);
    if (stalledChild)
    {
        // Indented child: no icon, 12.5px type, base delegate draws the text
        // into a rect shifted right by 36px.
        opt.text = displayText(index.data(Qt::DisplayRole), QLocale());
        opt.features &= ~QStyleOptionViewItem::HasDecoration;
        opt.rect = option.rect.adjusted(26, 0, -10, 0);
        const qreal dpiY = painter->device() ? painter->device()->logicalDpiY() : 96.0;
        opt.font.setPointSizeF(12.5 * 72.0 / dpiY);
    }
    // Paint the active-row wash BEFORE the base delegate so the icon and
    // label survive; painting it afterwards overwrote the item decoration.
    QColor labelColor;
    if (active)
    {
        const QColor accent = ThemeTokens::color(ThemeTokens::Token::Accent, mode);
        const QColor bg = option.palette.color(QPalette::Base);
        const QColor fill(qRound(bg.red() * 0.88 + accent.red() * 0.12),
                          qRound(bg.green() * 0.88 + accent.green() * 0.12),
                          qRound(bg.blue() * 0.88 + accent.blue() * 0.12));
        painter->fillRect(option.rect, fill);
        painter->fillRect(QRect(option.rect.left(), option.rect.top(), 2, option.rect.height()), accent);
        labelColor = ThemeTokens::color(ThemeTokens::Token::Text1, mode);
        opt.palette.setColor(QPalette::Text, labelColor);
    }
    else if (selected)
    {
        labelColor = ThemeTokens::color(ThemeTokens::Token::Text2, mode);
        opt.palette.setColor(QPalette::Text, labelColor);
    }

    QStyledItemDelegate::paint(painter, opt, index);

    const QRegularExpressionMatch match = COUNT_PATTERN.match(index.data(Qt::DisplayRole).toString());
    if (!match.hasMatch())
        return;

    const QString countText = match.captured(1);
    const int count = countText.toInt();
    const bool erroredRow = isErroredStatusRow(option.widget, index.row());

    painter->save();

    if (erroredRow && (count > 0))
    {
        // Errored filter count is a red pill badge, so it reads as a warning
        // instead of plain sidebar text (QA-v7 item 3).
        const QColor badgeColor = ThemeTokens::color(ThemeTokens::Token::StError, mode);
        QColor badgeFill {badgeColor};
        badgeFill.setAlphaF(0.14);
        const QColor badgeTextColor = ThemeTokens::colorById(u"TransferList.Error"_s, mode);

        QFont badgeFont = option.font;
        badgeFont.setWeight(QFont::DemiBold);
        const qreal dpiY = painter->device() ? painter->device()->logicalDpiY() : 96.0;
        badgeFont.setPointSizeF(11.5 * 72.0 / dpiY);
        painter->setFont(badgeFont);

        const QFontMetrics metrics {badgeFont};
        const int pillWidth = qMax(20, metrics.horizontalAdvance(countText) + 12);
        const QRect contentRect = option.rect.adjusted(8, 0, -10, 0);
        const QRect pillRect(contentRect.right() - pillWidth + 1
                             , contentRect.center().y() - 9, pillWidth, 18);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(badgeFill);
        painter->drawRoundedRect(QRectF(pillRect), 9.0, 9.0);
        painter->setPen(badgeTextColor);
        painter->drawText(pillRect, Qt::AlignCenter, countText);
    }
    else
    {
        QColor countColor = labelColor.isValid()
                ? labelColor : ThemeTokens::color(ThemeTokens::Token::Text3, mode);
        if (erroredRow)
            countColor = ThemeTokens::color(ThemeTokens::Token::Text3, mode);

        painter->setPen(countColor);
        const QRect countRect = option.rect.adjusted(8, 0, -10, 0);
        painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
    }

    painter->restore();
}
