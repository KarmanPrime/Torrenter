/*
 * Torrenter SegmentedControl.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "segmentedcontrol.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>

#include "../themetokens.h"

SegmentedControl::SegmentedControl(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("segmentedControl"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(3, 2, 3, 2);
    m_layout->setSpacing(2);
    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    m_animation = new QPropertyAnimation(this, "indicator", this);
    m_animation->setDuration(ThemeTokens::durationSnappy);
}

void SegmentedControl::addSegment(const QString &label, const QVariant &data)
{
    const int index = m_labels.size();
    auto *button = new QPushButton(label, this);
    button->setObjectName(QStringLiteral("segment"));
    button->setCheckable(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFlat(true);
    button->setAccessibleName(label);
    button->setAccessibleDescription(tr("Select %1").arg(label));
    m_group->addButton(button, index);
    m_layout->addWidget(button, 1);
    m_labels.append(label);
    m_data.append(data);
    connect(button, &QAbstractButton::clicked, this, [this, index] { setCurrentIndex(index); });
    if (index == 0)
        setCurrentIndex(0);
}

int SegmentedControl::currentIndex() const
{
    return m_currentIndex;
}

QVariant SegmentedControl::segmentData(int index) const
{
    return ((index >= 0) && (index < m_data.size())) ? m_data.at(index) : QVariant {};
}

void SegmentedControl::setCurrentIndex(int index)
{
    if ((index < 0) || (index >= m_labels.size()) || (index == m_currentIndex))
        return;

    const bool first = (m_currentIndex < 0);
    m_currentIndex = index;
    if (auto *button = m_group->button(index))
        button->setChecked(true);

    const QRectF target = targetIndicator();
    if (first)
    {
        m_animation->stop();
        setIndicator(target);
    }
    else
    {
        m_animation->stop();
        m_animation->setStartValue(m_indicator);
        m_animation->setEndValue(target);
        m_animation->start();
    }
    emit currentIndexChanged(index);
}

QRectF SegmentedControl::targetIndicator() const
{
    auto *button = m_group->button(m_currentIndex);
    if (!button)
        return {};
    const QRect rect = button->geometry().adjusted(0, 0, 0, 0);
    return QRectF(rect);
}

void SegmentedControl::setIndicator(const QRectF &rect)
{
    if (m_indicator == rect)
        return;
    m_indicator = rect;
    update();
}

void SegmentedControl::keyPressEvent(QKeyEvent *event)
{
    if (m_labels.isEmpty())
    {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key())
    {
    case Qt::Key_Left:
    case Qt::Key_Up:
        setCurrentIndex((m_currentIndex - 1 + m_labels.size()) % m_labels.size());
        event->accept();
        break;
    case Qt::Key_Right:
    case Qt::Key_Down:
        setCurrentIndex((m_currentIndex + 1) % m_labels.size());
        event->accept();
        break;
    case Qt::Key_Space:
    case Qt::Key_Enter:
    case Qt::Key_Return:
        setCurrentIndex(m_currentIndex);
        event->accept();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void SegmentedControl::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if ((event->type() == QEvent::PaletteChange) || (event->type() == QEvent::ThemeChange))
        update();
}

void SegmentedControl::paintEvent(QPaintEvent *)
{
    const ThemeTokens::Mode mode = ThemeTokens::currentMode();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!isEnabled())
        painter.setOpacity(0.4);

    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(ThemeTokens::color(ThemeTokens::Token::LineSubtle, mode), 1));
    painter.setBrush(ThemeTokens::color(ThemeTokens::Token::Raised, mode));
    painter.drawRoundedRect(track, 9, 9);

    if (m_indicator.isValid() && !m_indicator.isEmpty())
    {
        painter.setPen(Qt::NoPen);
        // Spec 6.2: active segment = #2C2E34 dark / #FFFFFF light.
        painter.setBrush((mode == ThemeTokens::Mode::Dark)
                ? QColor(QStringLiteral("#2C2E34")) : QColor(QStringLiteral("#FFFFFF")));
        painter.drawRoundedRect(m_indicator, 7, 7);
    }

    if (hasFocus())
    {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(ThemeTokens::color(ThemeTokens::Token::Focus, mode), 2));
        painter.drawRoundedRect(track.adjusted(-2, -2, 2, 2), 11, 11);
    }
}
