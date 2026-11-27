/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
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

#include "progressbarpainter.h"

#include <algorithm>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyleHints>
#include <QStyleOptionProgressBar>
#include <QStyleOptionViewItem>

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
#include <QProxyStyle>
#endif

#include "base/global.h"
#include "gui/uithememanager.h"
#include "themetokens.h"

ProgressBarPainter::ProgressBarPainter(QObject *parent)
    : QObject(parent)
{
#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    auto *fusionStyle = new QProxyStyle(u"fusion"_s);
    fusionStyle->setParent(&m_dummyProgressBar);
    m_dummyProgressBar.setStyle(fusionStyle);
#endif

    applyUITheme();
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &ProgressBarPainter::applyUITheme);
}

namespace
{
    QHash<quint64, qreal> &displayedProgress()
    {
        static QHash<quint64, qreal> values;
        return values;
    }

    QTimer &stripeTimer()
    {
        static QTimer timer;
        return timer;
    }

    qreal &stripeOffset()
    {
        static qreal offset = 0;
        return offset;
    }

    int &stripeFrames()
    {
        static int frames = 0;
        return frames;
    }
}

qreal ProgressBarPainter::lerped(const quint64 key, const qreal target)
{
    if (key == 0)
        return target;
    qreal &value = displayedProgress()[key];
    if (value <= 0)
        value = target;
    else
    {
        value += (target - value) * 0.08;
        if (qAbs(target - value) < 0.05)
            value = target;
    }
    return value;
}

void ProgressBarPainter::noteStriped()
{
    ++stripeFrames();
    auto &timer = stripeTimer();
    if (timer.isActive())
        return;
    timer.setInterval(1000 / 30);
    QObject::connect(&timer, &QTimer::timeout, &timer, []()
    {
        stripeOffset() += 7.0 / 30.0;
        if (stripeOffset() >= 7.0)
            stripeOffset() -= 7.0;
        if (stripeFrames() == 0)
            stripeTimer().stop();
        stripeFrames() = 0;
    });
    timer.start();
}

qreal ProgressBarPainter::stripePhase()
{
    return stripeOffset();
}

void ProgressBarPainter::paint(QPainter *painter, const QStyleOptionViewItem &option, const QString &text, const int progress,
    const QColor &color, const bool striped, const quint64 key) const
{
    const auto mode = ThemeTokens::currentMode();
    const QColor fillColor = color.isValid() ? color : ThemeTokens::color(ThemeTokens::Token::Accent, mode);
    const QColor trackColor = ThemeTokens::color(ThemeTokens::Token::Track, mode);
    const QColor textColor = option.palette.color(QPalette::Text);

    const QRect cell = option.rect;
    const int textWidth = QFontMetrics(option.font).horizontalAdvance(text);
    const int barWidth = std::max(24, cell.width() - textWidth - 10);
    const QRect track(cell.left(), cell.center().y() - 2, barWidth, 4);
    const qreal displayed = lerped(key, std::clamp(progress, 0, 100));
    const QRect fillRect(track.left(), track.top(), qRound(track.width() * displayed / 100.0), track.height());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(trackColor);
    painter->drawRoundedRect(track, 2, 2);
    if (fillRect.width() > 0)
    {
        painter->setBrush(fillColor);
        painter->drawRoundedRect(fillRect, 2, 2);
        if (striped)
        {
            noteStriped();
            painter->save();
            QPainterPath clipPath;
            clipPath.addRoundedRect(fillRect, 2, 2);
            painter->setClipPath(clipPath);
            painter->setPen(QPen(QColor(255, 255, 255, 71), 3, Qt::SolidLine));
            const qreal phase = stripePhase();
            for (int x = fillRect.left() - static_cast<int>(fillRect.height()) - 14;
                 x < fillRect.right() + 14; x += 7)
            {
                const int offset = x + static_cast<int>(phase) % 7;
                painter->drawLine(offset, fillRect.bottom(), offset + fillRect.height(), fillRect.top());
            }
            painter->restore();
        }
    }
    painter->setPen(textColor);
    painter->drawText(QRect(cell.right() - textWidth, cell.top(), textWidth, cell.height())
                      , Qt::AlignRight | Qt::AlignVCenter, text);
    painter->restore();
}

void ProgressBarPainter::applyUITheme()
{
    m_chunkColor = UIThemeManager::instance()->getColor(u"ProgressBar"_s);
}
