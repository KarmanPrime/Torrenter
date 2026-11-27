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
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSvgRenderer>
#include <QStackedLayout>
#include <QVBoxLayout>

#include "themetokens.h"
#include "torrentericons.h"
#include "uithememanager.h"

using namespace Qt::Literals::StringLiterals;

EmptyIllustration::EmptyIllustration(QWidget *parent)
    : QWidget(parent)
{
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, [this]()
    {
        m_cachedPixmap = QPixmap();
        m_cachedSize = QSize();
        m_cachedDpr = 0;
        m_cachedMode = -1;
        update();
    });
}

QSize EmptyIllustration::sizeHint() const
{
    return {132, 112};
}

void EmptyIllustration::paintEvent(QPaintEvent *)
{
    const qreal dpr = devicePixelRatioF();
    const auto mode = ThemeTokens::currentMode();
    const QSize logicalSize = size();

    if (m_cachedPixmap.isNull() || m_cachedSize != logicalSize
        || !qFuzzyCompare(m_cachedDpr, dpr) || m_cachedMode != static_cast<int>(mode))
    {
        QImage image(QSize(logicalSize.width() * dpr, logicalSize.height() * dpr)
            , QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter imagePainter(&image);
        imagePainter.scale(dpr, dpr);
        imagePainter.setRenderHint(QPainter::Antialiasing);

        // 112x76 dashed rounded card, radius 12, 1.5px border, 10px inset.
        const QRectF card {10, 18, 112, 76};
        QPen dash {ThemeTokens::color(ThemeTokens::Token::LineStrong, mode)};
        dash.setWidthF(1.5);
        dash.setStyle(Qt::DashLine);
        imagePainter.setPen(dash);
        imagePainter.setBrush(Qt::NoBrush);
        imagePainter.drawRoundedRect(card, 12, 12);

        // The brand icon IS the 56px tile: render the SVG into the tile rect,
        // inset 0, centered and overlapping the card's top edge.
        const QRectF tile {38, 2, 56, 56};
        const QString iconPath = (mode == ThemeTokens::Mode::Dark)
            ? u":/icons/torrenter-icon.svg"_s
            : u":/icons/torrenter-icon-light.svg"_s;
        QSvgRenderer renderer(iconPath);
        if (renderer.isValid())
            renderer.render(&imagePainter, tile);

        // Two centered progress tracks inside the card: 32x4 mint over
        // an 88x4 empty track, both radius 2.
        imagePainter.setPen(Qt::NoPen);
        imagePainter.setBrush(ThemeTokens::color(ThemeTokens::Token::Accent, mode));
        imagePainter.drawRoundedRect(QRectF(50, 68, 32, 4), 2, 2);
        imagePainter.setBrush(ThemeTokens::color(ThemeTokens::Token::Track, mode));
        imagePainter.drawRoundedRect(QRectF(22, 80, 88, 4), 2, 2);

        imagePainter.end();
        m_cachedPixmap = QPixmap::fromImage(image);
        m_cachedPixmap.setDevicePixelRatio(dpr);
        m_cachedSize = logicalSize;
        m_cachedDpr = dpr;
        m_cachedMode = static_cast<int>(mode);
    }

    QPainter painter {this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, m_cachedPixmap);
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
