/* Designed empty state and drop highlight for the transfer list.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QStackedLayout;

// The little dashed illustration above the empty-state title: a 112x80
// dashed outline, the 56px app-icon tile and two progress tracks
// (design mockup 07-empty-state.png).
class EmptyIllustration final : public QWidget
{
public:
    using QWidget::QWidget;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// Transparent-for-mouse overlay that paints the accent drop highlight over
// the list area while a .torrent file / magnet is dragged over the window.
class DropHighlightOverlay final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// Full-viewport overlay. Page 0 is the designed "nothing downloading yet"
// state, page 1 is the compact "no torrents match this filter" state.
class EmptyStateWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EmptyStateWidget)

public:
    explicit EmptyStateWidget(QWidget *parent = nullptr);

    void setFilterMode(bool filterMode);

signals:
    void openFileRequested();
    void addLinksRequested();
    void clearFilterRequested();

private:
    QWidget *createDefaultPage();
    QWidget *createFilterPage();
    static QWidget *makeButton(const QString &text, const QString &shortcut
        , const QString &objectName, QPushButton **buttonOut);

    QStackedLayout *m_pages = nullptr;
};
