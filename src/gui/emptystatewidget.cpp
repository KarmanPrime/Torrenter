/* Designed empty state and drop highlight for the transfer list.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "emptystatewidget.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

#include "themetokens.h"
#include "torrentericons.h"

using namespace Qt::Literals::StringLiterals;

QSize EmptyIllustration::sizeHint() const
{
    // Geometry mirrors mockup 07 exactly: a 132x112 box holds the 112x80
    // dashed outline, the 56px tile straddling its top edge and two tracks.
    return {132, 112};
}

void EmptyIllustration::paintEvent(QPaintEvent *)
{
    const auto mode = ThemeTokens::currentMode();
    QPainter painter {this};
    painter.setRenderHint(QPainter::Antialiasing);

    // 112x80 dashed rounded outline (1.5px lineStrong, radius 14).
    QPen dash {ThemeTokens::color(ThemeTokens::Token::LineStrong, mode)};
    dash.setWidthF(1.5);
    dash.setStyle(Qt::DashLine);
    painter.setPen(dash);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(10, 18, 112, 80), 14, 14);

    // 56px app-icon tile sitting on top of the outline.
    const QRectF tile {38, 0, 56, 56};
    painter.setPen(QPen(ThemeTokens::color(ThemeTokens::Token::Line, mode), 1));
    painter.setBrush(ThemeTokens::color(ThemeTokens::Token::Raised, mode));
    painter.drawRoundedRect(tile, 15, 15);
    const QIcon appIcon = TorrenterIcons::icon(u"torrenter-glyph"_s, ThemeTokens::Token::Accent, mode);
    appIcon.paint(&painter, tile.adjusted(12, 12, 12, 12).toRect());

    // Two little progress tracks under the tile: the top one is 55% accent.
    painter.setPen(Qt::NoPen);
    const QRectF firstTrack {46, 66, 40, 4};
    painter.setBrush(ThemeTokens::color(ThemeTokens::Token::Track, mode));
    painter.drawRoundedRect(firstTrack, 2, 2);
    painter.setBrush(ThemeTokens::color(ThemeTokens::Token::Accent, mode));
    painter.drawRoundedRect(QRectF(46, 66, 22, 4), 2, 2);  // 55% of 40px
    const QRectF secondTrack {34, 78, 64, 4};
    painter.setBrush(ThemeTokens::color(ThemeTokens::Token::Track, mode));
    painter.drawRoundedRect(secondTrack, 2, 2);
}

void DropHighlightOverlay::paintEvent(QPaintEvent *)
{
    const auto mode = ThemeTokens::currentMode();
    QPainter painter {this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF area = QRectF(rect()).adjusted(12, 12, -12, -12);
    QColor wash = ThemeTokens::color(ThemeTokens::Token::AccentSoft, mode);
    wash.setAlpha(40);
    painter.setPen(Qt::NoPen);
    painter.setBrush(wash);
    painter.drawRoundedRect(area, 12, 12);

    QPen ring {ThemeTokens::color(ThemeTokens::Token::Accent, mode)};
    ring.setWidthF(2);
    ring.setStyle(Qt::DashLine);
    painter.setPen(ring);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(area, 12, 12);
}

QWidget *EmptyStateWidget::makeButton(const QString &text, const QString &shortcut
    , const QString &objectName, QPushButton **buttonOut)
{
    auto *button = new QPushButton;
    button->setObjectName(objectName);
    if (objectName == u"emptyPrimary"_s)
        button->setProperty("variant", u"primary"_s);
    button->setMinimumHeight(32);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(button);
    layout->setContentsMargins(14, 0, 10, 0);
    layout->setSpacing(8);

    auto *label = new QLabel(text, button);
    label->setObjectName(objectName + u"Label"_s);
    layout->addWidget(label);
    layout->addStretch();

    auto *chip = new QLabel(shortcut, button);
    chip->setObjectName((objectName == u"emptyPrimary"_s) ? u"kbdChipOnAccent"_s : u"kbdChip"_s);
    chip->setAlignment(Qt::AlignCenter);
    layout->addWidget(chip);

    if (buttonOut)
        *buttonOut = button;
    return button;
}

QWidget *EmptyStateWidget::createDefaultPage()
{
    auto *page = new QWidget;
    auto *column = new QVBoxLayout(page);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(12);
    column->setAlignment(Qt::AlignCenter);

    auto *illustration = new EmptyIllustration;
    illustration->setFixedSize(132, 112);
    illustration->setAttribute(Qt::WA_TransparentForMouseEvents);
    column->addWidget(illustration, 0, Qt::AlignHCenter);
    column->addSpacing(10);  // 22px gap to the title, per mockup 07

    auto *title = new QLabel(tr("Nothing downloading yet"));
    title->setObjectName(u"emptyStateTitle"_s);
    title->setAlignment(Qt::AlignCenter);
    column->addWidget(title);

    auto *body = new QLabel(tr("Drop a .torrent file anywhere in this window,\nor paste a magnet link with Ctrl V."));
    body->setObjectName(u"emptyStateBody"_s);
    body->setAlignment(Qt::AlignHCenter);
    body->setTextInteractionFlags(Qt::NoTextInteraction);
    column->addWidget(body);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    QPushButton *openButton = nullptr;
    QPushButton *linksButton = nullptr;
    actions->addWidget(makeButton(tr("Open torrent file"), tr("Ctrl O"), u"emptyPrimary"_s, &openButton));
    actions->addWidget(makeButton(tr("Add links"), tr("Ctrl Shift O"), u"emptySecondary"_s, &linksButton));
    auto *actionsHolder = new QWidget;
    actionsHolder->setFixedWidth(420);
    actionsHolder->setLayout(actions);
    column->addWidget(actionsHolder, 0, Qt::AlignHCenter);

    connect(openButton, &QPushButton::clicked, this, &EmptyStateWidget::openFileRequested);
    connect(linksButton, &QPushButton::clicked, this, &EmptyStateWidget::addLinksRequested);

    auto *helpRow = new QHBoxLayout;
    helpRow->setSpacing(6);
    auto *searchIcon = new QLabel;
    searchIcon->setPixmap(TorrenterIcons::icon(u"edit-find"_s, ThemeTokens::Token::Text3, ThemeTokens::currentMode())
        .pixmap(14, 14));
    searchIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    helpRow->addWidget(searchIcon, 0, Qt::AlignRight);
    auto *help = new QLabel(tr("Looking for something? Try Search in the View menu."));
    help->setObjectName(u"emptyStateHelp"_s);
    helpRow->addWidget(help, 0, Qt::AlignLeft);
    helpRow->setAlignment(Qt::AlignCenter);
    column->addLayout(helpRow);

    return page;
}

QWidget *EmptyStateWidget::createFilterPage()
{
    auto *page = new QWidget;
    auto *column = new QVBoxLayout(page);
    column->setAlignment(Qt::AlignCenter);
    column->setSpacing(12);

    auto *label = new QLabel(tr("No torrents match this filter"));
    label->setObjectName(u"emptyStateTitle"_s);
    label->setAlignment(Qt::AlignCenter);
    column->addWidget(label);

    auto *clear = new QPushButton(tr("Clear filter"));
    clear->setProperty("variant", u"ghost"_s);
    connect(clear, &QPushButton::clicked, this, &EmptyStateWidget::clearFilterRequested);
    column->addWidget(clear, 0, Qt::AlignHCenter);
    return page;
}

EmptyStateWidget::EmptyStateWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(u"emptyStateOverlay"_s);
    m_pages = new QStackedLayout(this);
    m_pages->addWidget(createDefaultPage());
    m_pages->addWidget(createFilterPage());
    m_pages->setCurrentIndex(0);
}

void EmptyStateWidget::setFilterMode(const bool filterMode)
{
    m_pages->setCurrentIndex(filterMode ? 1 : 0);
}
