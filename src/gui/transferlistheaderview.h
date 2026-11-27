#pragma once

#include <QHeaderView>

#include "themetokens.h"

class QPainter;
class QRect;

// Design (HANDOFF 5.1): a 34px surface header with no vertical grid lines.
// Labels are 12px/500 in text3; the sorted column uses text1 and a 12px
// chevron immediately after its label. The last 28px are reserved for the
// column chooser; TransferListWidget installs the actual QToolButton there.
class TransferListHeaderView final : public QHeaderView
{
public:
    explicit TransferListHeaderView(QWidget *parent = nullptr);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
    static bool isNumeric(int column);
    int lastVisibleSection() const;
};
