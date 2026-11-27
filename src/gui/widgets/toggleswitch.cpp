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

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QCheckBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setObjectName(QStringLiteral("toggleSwitch"));
    m_knobPosition = isChecked() ? 1 : 0;

    m_animation = new QPropertyAnimation(this, "knobPosition", this);
    m_animation->setDuration(ThemeTokens::durationSnappy);
    connect(this, &QCheckBox::toggled, this, &ToggleSwitch::syncState);
}

QSize ToggleSwitch::sizeHint() const
{
    const QFontMetrics fm(font());
    const int textWidth = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
    const int height = qMax(22, fm.height() + 4);
    return {38 + textWidth + 2, height};
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return {30, 18};
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

    const QRectF track(0, qMax(0.0, (height() - 18) / 2.0), 30, 18);
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
        const QRectF labelRect(38, 0, width() - 38, height());
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
}
