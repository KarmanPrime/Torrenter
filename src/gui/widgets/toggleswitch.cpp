/*
 * Torrenter ToggleSwitch - keeps full QCheckBox semantics/signals.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "toggleswitch.h"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPropertyAnimation>

#include "../themetokens.h"

namespace
{
    // Painted switch geometry. The focus ring extends 2 px outside the track,
    // so the track starts 3 px inside the widget to keep every edge visible.
    constexpr int trackWidth = 30;
    constexpr int trackHeight = 18;
    constexpr int outerMargin = 3;
    constexpr int labelGap = 8;
    constexpr int labelX = outerMargin + trackWidth + labelGap;
}

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QCheckBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setObjectName(QStringLiteral("toggleSwitch"));
    setMinimumHeight(trackHeight + (2 * outerMargin));
    m_knobPosition = isChecked() ? 1 : 0;

    m_animation = new QPropertyAnimation(this, "knobPosition", this);
    m_animation->setDuration(ThemeTokens::durationSnappy);
    connect(this, &QCheckBox::toggled, this, &ToggleSwitch::syncState);
}

QSize ToggleSwitch::sizeHint() const
{
    const QFontMetrics fm(font());
    const int textWidth = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
    const int height = qMax(trackHeight + (2 * outerMargin), fm.height() + 6);
    return {labelX + textWidth + outerMargin, height};
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return {trackWidth + (2 * outerMargin), trackHeight + (2 * outerMargin)};
}

bool ToggleSwitch::hitButton(const QPoint &pos) const
{
    // The whole row (track + label) acts like the checkbox indicator.
    return rect().contains(pos);
}

void ToggleSwitch::changeEvent(QEvent *event)
{
    QCheckBox::changeEvent(event);
    if ((event->type() == QEvent::PaletteChange) || (event->type() == QEvent::ThemeChange))
        update();
}

void ToggleSwitch::setKnobPosition(qreal position)
{
    if (qFuzzyCompare(m_knobPosition, position))
        return;
    m_knobPosition = position;
    update();
}

void ToggleSwitch::syncState()
{
    m_animation->stop();
    m_animation->setStartValue(m_knobPosition);
    m_animation->setEndValue(isChecked() ? 1.0 : 0.0);
    m_animation->start();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    const ThemeTokens::Mode mode = ThemeTokens::currentMode();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!isEnabled())
        painter.setOpacity(0.4);

    const QRectF track(outerMargin, qMax(0.0, (height() - trackHeight) / 2.0), trackWidth, trackHeight);
    // Spec 6.2: Off = LineStrong #45474E / #C4C7CE, On = Accent #45E3B8 / #0E9A74.
    const QColor trackColor = isChecked()
            ? ((mode == ThemeTokens::Mode::Dark) ? QColor(QStringLiteral("#45E3B8")) : QColor(QStringLiteral("#0E9A74")))
            : ((mode == ThemeTokens::Mode::Dark) ? QColor(QStringLiteral("#45474E")) : QColor(QStringLiteral("#C4C7CE")));
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, 9, 9);

    if (hasFocus())
    {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(ThemeTokens::color(ThemeTokens::Token::Focus, mode), 2));
        painter.drawRoundedRect(track.adjusted(-2, -2, 2, 2), 11, 11);
    }

    const qreal knobX = track.left() + 2 + (m_knobPosition * 14.0);
    // Subtle drop shadow: rgba(0,0,0,0.35) offset 0,1 blur 2.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 89));
    painter.drawEllipse(QRectF(knobX - 1, track.top() + 3, 16, 16));
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(knobX, track.top() + 2, 14, 14));

    if (!text().isEmpty())
    {
        painter.setPen(ThemeTokens::color(ThemeTokens::Token::Text1, mode));
        const QRectF labelRect(labelX, 0, qMax(0, width() - labelX), height());
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
}
