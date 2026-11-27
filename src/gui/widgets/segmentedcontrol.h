/*
 * Torrenter SegmentedControl - a QButtonGroup with a sliding indicator.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QWidget>
#include <QRectF>
#include <QVariant>
#include <QStringList>

class QButtonGroup;
class QHBoxLayout;
class QPropertyAnimation;

class SegmentedControl final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QRectF indicator READ indicator WRITE setIndicator)

public:
    explicit SegmentedControl(QWidget *parent = nullptr);

    void addSegment(const QString &label, const QVariant &data = {});
    int currentIndex() const;
    QVariant segmentData(int index) const;

public slots:
    void setCurrentIndex(int index);

signals:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QRectF indicator() const { return m_indicator; }
    void setIndicator(const QRectF &rect);
    QRectF targetIndicator() const;

    QButtonGroup *m_group = nullptr;
    QHBoxLayout *m_layout = nullptr;
    QPropertyAnimation *m_animation = nullptr;
    QRectF m_indicator;
    QStringList m_labels;
    QList<QVariant> m_data;
    int m_currentIndex = -1;
};
