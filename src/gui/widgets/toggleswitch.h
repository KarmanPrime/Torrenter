/*
 * Torrenter ToggleSwitch - a QCheckBox drawn as a 30x18 sliding switch.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QCheckBox>
#include <QRect>

class QPropertyAnimation;

class ToggleSwitch final : public QCheckBox
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;
    void changeEvent(QEvent *event) override;

private:
    qreal knobPosition() const { return m_knobPosition; }
    void setKnobPosition(qreal position);

    void syncState();

    QPropertyAnimation *m_animation = nullptr;
    qreal m_knobPosition = 0;
};
