/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "propertieswidget.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QFuture>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QShortcut>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QTimer>
#include <QUrl>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/types.h"
#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "gui/autoexpandabledialog.h"
#include "gui/filterpatternformatmenu.h"
#include "gui/lineedit.h"
#include "gui/trackerlist/trackerlistwidget.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "gui/utils/keysequence.h"
#include "gui/themetokens.h"
#include "gui/torrentericons.h"
#include "downloadedpiecesbar.h"
#include "gui/torrentcontentmodel.h"
#include "gui/torrentcontentmodelitem.h"
#include "peerlistwidget.h"
#include "pieceavailabilitybar.h"
#include "proptabbar.h"
#include "speedwidget.h"
#include "ui_propertieswidget.h"

PropertiesWidget::PropertiesWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui {new Ui::PropertiesWidget}
    , m_storeFilterPatternFormat {u"GUI/PropertiesWidget/FilterPatternFormat"_s}
{
    m_ui->setupUi(this);
#ifndef Q_OS_MACOS
    setAutoFillBackground(true);
#endif

    m_state = VISIBLE;

    // Torrent content filtering
    m_contentFilterLine = new LineEdit(this);
    m_contentFilterLine->setObjectName(u"contentFilterLine"_s);
    m_contentFilterLine->setPlaceholderText(tr("Filter files..."));
    m_contentFilterLine->setFixedSize(260, 30);
    m_contentFilterLine->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_contentFilterLine, &QWidget::customContextMenuRequested, this, &PropertiesWidget::showContentFilterContextMenu);
    connect(m_contentFilterLine, &LineEdit::textUpdated, this, &PropertiesWidget::setContentFilterPattern);
    // Tool row: filter, Select all, Select none, stretch, Selected X of Y.
    if (auto *toolRow = qobject_cast<QHBoxLayout *>(m_ui->contentFilterLayout))
    {
        while (toolRow->count() > 0)
        {
            QLayoutItem *item = toolRow->takeAt(0);
            delete item;
        }
        const auto ghostButton = [this](QPushButton *button, const QString &text, const QString &iconId)
        {
            button->setText(text);
            button->setObjectName(u"contentGhostButton"_s);
            button->setIcon(TorrenterIcons::icon(iconId, ThemeTokens::Token::Text2, ThemeTokens::currentMode()));
            button->setIconSize(QSize(14, 14));
            button->setFixedHeight(24);
            button->setCursor(Qt::PointingHandCursor);
        };
        ghostButton(m_ui->selectAllButton, tr("Select all"), u"check-check"_s);
        ghostButton(m_ui->selectNoneButton, tr("Select none"), u"square-dashed"_s);
        m_contentSelectedLabel = new QLabel(m_ui->pageContents);
        m_contentSelectedLabel->setObjectName(u"contentSelectedLabel"_s);
        toolRow->setContentsMargins(0, 0, 16, 10);
        toolRow->setSpacing(8);
        toolRow->addWidget(m_contentFilterLine);
        toolRow->addWidget(m_ui->selectAllButton);
        toolRow->addWidget(m_ui->selectNoneButton);
        toolRow->addStretch();
        toolRow->addWidget(m_contentSelectedLabel);
    }
    connect(m_ui->filesList->selectionModel(), &QItemSelectionModel::selectionChanged
            , this, [this]() { updateContentSelectionLabel(); });
    connect(m_ui->filesList->model(), &QAbstractItemModel::dataChanged
            , this, [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles)
    {
        if (roles.isEmpty() || roles.contains(Qt::CheckStateRole))
            updateContentSelectionLabel();
    });

    // HTTP sources empty state.
    m_noHttpSourcesLabel = new QLabel(tr("No HTTP sources"), m_ui->pageSources);
    m_noHttpSourcesLabel->setObjectName(u"httpSourcesEmpty"_s);
    m_noHttpSourcesLabel->setAlignment(Qt::AlignCenter);
    m_ui->verticalLayout_9->addWidget(m_noHttpSourcesLabel);

    m_ui->filesList->setContentDragAllowed(true);
    m_ui->filesList->setDoubleClickAction(TorrentContentWidget::DoubleClickAction::Open);
    m_ui->filesList->setOpenByEnterKey(true);

    // SIGNAL/SLOTS
    connect(m_ui->selectAllButton, &QPushButton::clicked, m_ui->filesList, &TorrentContentWidget::checkAll);
    connect(m_ui->selectNoneButton, &QPushButton::clicked, m_ui->filesList, &TorrentContentWidget::checkNone);
    connect(m_ui->listWebSeeds, &QWidget::customContextMenuRequested, this, &PropertiesWidget::displayWebSeedListMenu);
    connect(m_ui->stackedProperties, &QStackedWidget::currentChanged, this, &PropertiesWidget::loadDynamicData);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentSavePathChanged, this, &PropertiesWidget::updateSavePath);
    connect(m_ui->filesList, &TorrentContentWidget::stateChanged, this, &PropertiesWidget::saveSettings);
    connect(m_ui->filesList, &TorrentContentWidget::stateChanged, this, &PropertiesWidget::updateContentSelectionLabel);

    // Pieces bars are sized in setupGeneralCards().
    // Downloaded pieces progress bar
    m_ui->tempProgressBarArea->setVisible(false);
    m_downloadedPieces = new DownloadedPiecesBar(this);
    m_ui->groupBarLayout->addWidget(m_downloadedPieces, 0, 1);
    m_downloadedPieces->setFixedHeight(10);
    m_downloadedPieces->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Pieces availability bar
    m_ui->tempAvailabilityBarArea->setVisible(false);
    m_piecesAvailability = new PieceAvailabilityBar(this);
    m_ui->groupBarLayout->addWidget(m_piecesAvailability, 1, 1);
    m_piecesAvailability->setFixedHeight(7);
    m_piecesAvailability->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Tracker list
    m_trackerList = new TrackerListWidget(this);
    m_ui->trackerUpButton->setIcon(UIThemeManager::instance()->getIcon(u"go-up"_s));
    m_ui->trackerUpButton->setIconSize(Utils::Gui::smallIconSize());
    m_ui->trackerDownButton->setIcon(UIThemeManager::instance()->getIcon(u"go-down"_s));
    m_ui->trackerDownButton->setIconSize(Utils::Gui::smallIconSize());
    connect(m_ui->trackerUpButton, &QPushButton::clicked, m_trackerList, &TrackerListWidget::decreaseSelectedTrackerTiers);
    connect(m_ui->trackerDownButton, &QPushButton::clicked, m_trackerList, &TrackerListWidget::increaseSelectedTrackerTiers);
    m_ui->hBoxLayoutTrackers->insertWidget(0, m_trackerList);
    // Peers list
    m_peerList = new PeerListWidget(this);
    m_ui->vBoxLayoutPeerPage->addWidget(m_peerList);
    // Tab bar: one segmented control (General / Trackers / Peers / HTTP Sources / Content / Speed).
    m_tabBar = new PropTabBar(nullptr);
    // PropTabBar puts a spacer before Speed; remove it so all six tabs form one track.
    for (int i = m_tabBar->count() - 1; i >= 0; --i)
    {
        QLayoutItem *item = m_tabBar->itemAt(i);
        if (item && item->spacerItem())
        {
            m_tabBar->removeItem(item);
            delete item;
            break;
        }
    }

    const auto mode = ThemeTokens::currentMode();
    const auto tokenCss = [mode](const ThemeTokens::Token token) -> QString
    {
        const QColor c = ThemeTokens::color(token, mode);
        return QStringLiteral("rgba(%1,%2,%3,%4)")
                .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
    };

    for (int i = 0; i < m_tabBar->count(); ++i)
    {
        if (QPushButton *button = qobject_cast<QPushButton *>(m_tabBar->itemAt(i)->widget()))
        {
            button->setObjectName(u"torrenterPropTab"_s);
            button->setProperty("baseText", button->text());
            button->setFixedHeight(26);
            button->setIconSize(QSize(14, 14));
            QFont font = button->font();
            font.setPixelSize(12);
            font.setWeight(QFont::Medium);
            button->setFont(font);
        }
    }

    // Raised track wrapping the six 26 px segments.
    m_tabTrack = new QWidget(this);
    m_tabTrack->setObjectName(u"torrenterPropTabTrack"_s);
    auto *trackLayout = new QHBoxLayout(m_tabTrack);
    trackLayout->setContentsMargins(3, 3, 3, 3);
    trackLayout->setSpacing(2);
    trackLayout->addLayout(m_tabBar);
    trackLayout->addStretch(1);
    m_addPeersButton = new QToolButton(m_tabTrack);
    m_addPeersButton->setObjectName(u"peerAddButton"_s);
    m_addPeersButton->setIcon(TorrenterIcons::icon(u"peers-add"_s, ThemeTokens::Token::Text2, mode));
    m_addPeersButton->setIconSize(QSize(14, 14));
    m_addPeersButton->setText(tr("Add peers"));
    m_addPeersButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_addPeersButton->hide();
    connect(m_addPeersButton, &QToolButton::clicked, m_peerList, &PeerListWidget::addPeers);
    trackLayout->addWidget(m_addPeersButton);
    m_hideButton = new QToolButton(m_tabTrack);
    m_hideButton->setObjectName(u"propHide"_s);
    m_hideButton->setIcon(TorrenterIcons::icon(u"chevron-down"_s, ThemeTokens::Token::Text2, mode));
    m_hideButton->setIconSize(QSize(16, 16));
    m_hideButton->setToolTip(tr("Hide details panel"));
    trackLayout->addWidget(m_hideButton);
    connect(m_hideButton, &QToolButton::clicked, this, &PropertiesWidget::togglePanelVisibility);
    m_tabTrack->setStyleSheet(
        QStringLiteral("QWidget#torrenterPropTabTrack { background: %1; border: 1px solid %2; border-radius: 9px; }"
            " QPushButton#torrenterPropTab { background: transparent; border: none; border-radius: 6px; padding: 0 11px; color: %3; }"
            " QPushButton#torrenterPropTab:hover { background: %4; color: %5; }"
            " QPushButton#torrenterPropTab[active=\"true\"] { background: %6; color: %5; }")
        .arg(tokenCss(ThemeTokens::Token::Raised)
            , tokenCss(ThemeTokens::Token::LineSubtle)
            , tokenCss(ThemeTokens::Token::Text2)
            , tokenCss(ThemeTokens::Token::Hover)
            , tokenCss(ThemeTokens::Token::Text1)
            , tokenCss(ThemeTokens::Token::Pressed)));
    m_ui->verticalLayout->insertWidget(0, m_tabTrack);

    // QA-v6 item 2: active tab icons take the accent tint; others text2.
    static const char *tabIconIds[] = {"help-about", "trackers", "peers", "network-server", "directory", "chart-line"};
    const auto refreshTabStyles = [this]()
    {
        const int current = m_tabBar->currentIndex();
        for (int i = 0; i < m_tabBar->count(); ++i)
        {
            if (QPushButton *button = qobject_cast<QPushButton *>(m_tabBar->itemAt(i)->widget()))
            {
                const bool active = (i == current);
                button->setProperty("active", active);
                if (i < 6)
                {
                    const QString iconId = QString::fromLatin1(tabIconIds[i]);
                    button->setIcon(active ? UIThemeManager::instance()->activeIcon(iconId)
                                           : UIThemeManager::instance()->getIcon(iconId));
                }
                button->style()->unpolish(button);
                button->style()->polish(button);
                button->update();
            }
        }
    };
    refreshTabStyles();
    connect(m_tabBar, &PropTabBar::tabChanged, this, [this, refreshTabStyles](int index)
    {
        m_expandedTabIndex = index;
        refreshTabStyles();
        updateHideButton();
        if (m_addPeersButton)
            m_addPeersButton->setVisible(index == PropTabBar::PeersTab);
    });
    connect(m_tabBar, &PropTabBar::visibilityToggled, this, [this, refreshTabStyles](bool visible)
    {
        refreshTabStyles();
        updateHideButton();
        Q_UNUSED(visible);
    });
    connect(m_tabBar, &PropTabBar::tabChanged, m_ui->stackedProperties, &QStackedWidget::setCurrentIndex);
    connect(m_tabBar, &PropTabBar::tabChanged, this, &PropertiesWidget::saveSettings);
    connect(m_tabBar, &PropTabBar::visibilityToggled, this, &PropertiesWidget::setVisibility);
    connect(m_tabBar, &PropTabBar::visibilityToggled, this, &PropertiesWidget::saveSettings);
    if (auto *detailsSplitter = qobject_cast<QSplitter *>(parentWidget()))
        connect(detailsSplitter, &QSplitter::splitterMoved, this, &PropertiesWidget::onSplitterMoved);

    const auto *editWebSeedsHotkey = new QShortcut(Qt::Key_F2, m_ui->listWebSeeds, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editWebSeedsHotkey, &QShortcut::activated, this, &PropertiesWidget::editWebSeed);
    const auto *deleteWebSeedsHotkey = new QShortcut(Utils::KeySequence::deleteItem(), m_ui->listWebSeeds, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteWebSeedsHotkey, &QShortcut::activated, this, &PropertiesWidget::deleteSelectedUrlSeeds);
    connect(m_ui->listWebSeeds, &QListWidget::doubleClicked, this, &PropertiesWidget::editWebSeed);

#ifdef TORRENTER_QA_HOOKS
    if (qEnvironmentVariableIsSet("TORRENTER_QA_EXPAND_FILES"))
    {
        QTimer::singleShot(6000, this, [this]()
        {
            m_contentFilterLine->clear();
            setContentFilterPattern();
            m_ui->filesList->expandAll();
        });
    }
#endif

    configure();
    setupGeneralCards();
    updateTabCounts();
    connect(Preferences::instance(), &Preferences::changed, this, &PropertiesWidget::configure);
}


void PropertiesWidget::setupGeneralCards()
{
    auto *root = qobject_cast<QVBoxLayout *>(m_ui->verticalLayout_2);
    if (root == nullptr)
        return;

    // 4 px top, 16 px sides/bottom, 12 px gaps; scroll rather than clip.
    root->setContentsMargins(16, 4, 16, 16);
    root->setSpacing(12);
    m_ui->scrollArea->setFrameShape(QFrame::NoFrame);
    m_ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const auto detach = [](QWidget *w)
    {
        if ((w == nullptr) || (w->parentWidget() == nullptr) || (w->parentWidget()->layout() == nullptr))
            return;
        QLayout *layout = w->parentWidget()->layout();
        for (int i = 0; i < layout->count(); ++i)
        {
            QLayoutItem *item = layout->itemAt(i);
            if (item->widget() == w)
            {
                layout->removeItem(item);
                delete item;
                return;
            }
        }
    };
    const auto byName = [this](const QString &name) -> QWidget *
    {
        return findChild<QWidget *>(name);
    };
    const auto enableTabularFigures = [](QWidget *widget)
    {
        auto *label = qobject_cast<QLabel *>(widget);
        if (label == nullptr)
            return;
        QFont font = label->font();
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        font.setFeature(QFont::Tag("tnum"), 1);
#endif
        label->setFont(font);
    };

    // --- Header card -------------------------------------------------------
    auto *barLayout = qobject_cast<QGridLayout *>(m_ui->groupBarLayout);
    QHash<QString, QWidget *> barWidgets;
    if (barLayout != nullptr)
    {
        for (int i = 0; i < barLayout->count(); ++i)
            if (QWidget *w = barLayout->itemAt(i)->widget())
                barWidgets.insert(w->objectName(), w);
        while (barLayout->count() > 0)
        {
            QLayoutItem *item = barLayout->takeAt(0);
            delete item;
        }
    }

    m_downloadedPieces->setObjectName(u"torrenterPiecesBar"_s);
    m_piecesAvailability->setObjectName(u"torrenterPiecesBar"_s);
    if (barLayout != nullptr)
    {
        barLayout->setContentsMargins(0, 0, 0, 0);
        barLayout->setHorizontalSpacing(0);
        barLayout->setVerticalSpacing(6);
        barLayout->addWidget(m_downloadedPieces, 0, 0, 1, 2);
        barLayout->addWidget(m_piecesAvailability, 1, 0, 1, 2);
    }

    auto *header = new QWidget(this);
    header->setObjectName(u"detailsCard"_s);
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    header->setMinimumWidth(0);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 14, 16, 12);
    headerLayout->setSpacing(8);

    auto *top = new QHBoxLayout;
    top->setSpacing(10);
    m_detailName = new QLabel(tr("No torrent selected"), header);
    m_detailName->setObjectName(u"detailName"_s);
    m_detailName->setMinimumWidth(0);
    m_detailName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_detailPill = new QLabel(header);
    m_detailPill->setObjectName(u"detailPill"_s);
    m_detailPill->hide();
    m_detailSummary = new QLabel(header);
    m_detailSummary->setObjectName(u"detailSummary"_s);
    m_detailSummary->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_detailSummary->setMinimumWidth(340);
    m_detailSummary->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    enableTabularFigures(m_detailSummary);
    top->addWidget(m_detailName, 1);
    top->addWidget(m_detailPill);
    top->addStretch();
    headerLayout->addLayout(top);
    auto *summaryRow = new QHBoxLayout;
    summaryRow->addStretch();
    summaryRow->addWidget(m_detailSummary);
    headerLayout->addLayout(summaryRow);
    headerLayout->addWidget(m_ui->groupBar);

    auto *legend = new QHBoxLayout;
    legend->setSpacing(8);
    const auto swatch = [this, header, legend](const QString &objectName, const QString &text)
    {
        auto *s = new QLabel(header);
        s->setObjectName(objectName);
        s->setFixedSize(8, 8);
        legend->addWidget(s);
        auto *l = new QLabel(text, header);
        l->setObjectName(u"detailMeta"_s);
        legend->addWidget(l);
    };
    swatch(u"detailSwatchHave"_s, tr("Have"));
    swatch(u"detailSwatchPartial"_s, tr("Partial"));
    swatch(u"detailSwatchMissing"_s, tr("Missing"));
    legend->addStretch(1);

    // Keep the original labels alive and working; showPieces*() still toggles them.
    auto *hidden = new QWidget(header);
    hidden->setObjectName(u"detailHiddenLabels"_s);
    hidden->hide();
    auto *hiddenLayout = new QHBoxLayout(hidden);
    for (const QString &name : {u"labelDownloadedPieces"_s, u"labelPiecesAvailability"_s})
        if (QWidget *w = barWidgets.value(name))
        {
            detach(w);
            hiddenLayout->addWidget(w);
        }
    if (QWidget *w = barWidgets.value(u"labelProgressVal"_s))
    {
        detach(w);
        w->setObjectName(u"detailMeta"_s);
        legend->addWidget(w);
    }
    auto *availabilityKey = new QLabel(tr("Availability"), header);
    availabilityKey->setObjectName(u"detailMeta"_s);
    legend->addWidget(availabilityKey);
    if (QWidget *w = barWidgets.value(u"labelAverageAvailabilityVal"_s))
    {
        detach(w);
        w->setObjectName(u"detailMeta"_s);
        legend->addWidget(w);
    }
    m_detailPiecesMeta = new QLabel(header);
    m_detailPiecesMeta->setObjectName(u"detailMeta"_s);
    legend->addWidget(m_detailPiecesMeta);
    headerLayout->addLayout(legend);

    m_ui->labelSavePathVal->setWordWrap(false);
    m_ui->lineBelowBars->setFixedHeight(0);
    m_ui->lineBelowBars->setMaximumHeight(0);
    headerLayout->addWidget(m_ui->lineBelowBars);

    // --- Three cards -------------------------------------------------------
    using Field = QPair<QString, QString>;
    const QList<Field> transferFields {
        {u"labelTimeActive"_s, u"labelElapsedVal"_s},
        {u"labelETA"_s, u"labelETAVal"_s},
        {u"labelDownloaded"_s, u"labelDlTotalVal"_s},
        {u"labelUploaded"_s, u"labelUpTotalVal"_s},
        {u"labelWasted"_s, u"labelWastedVal"_s},
        {u"labelRatio"_s, u"labelShareRatioVal"_s}
    };
    const QList<Field> speedFields {
        {u"labelDlSpeed"_s, u"labelDlSpeedVal"_s},
        {u"labelUpSpeed"_s, u"labelUpSpeedVal"_s},
        {u"labelConnections"_s, u"labelConnectionsVal"_s},
        {u"labelSeeds"_s, u"labelSeedsVal"_s},
        {u"labelPeers"_s, u"labelPeersVal"_s},
        {u"labelDlLimit"_s, u"labelDlLimitVal"_s},
        {u"labelUpLimit"_s, u"labelUpLimitVal"_s},
        {u"labelReannounceIn"_s, u"labelReannounceInVal"_s},
        {u"labelLastSeenComplete"_s, u"labelLastSeenCompleteVal"_s},
        {u"labelPopularity"_s, u"labelPopularityVal"_s}
    };
    const QList<Field> infoFields {
        {u"labelSavePath"_s, u"labelSavePathVal"_s},
        {u"labelAddedOn"_s, u"labelAddedOnVal"_s},
        {u"labelCompletedOn"_s, u"labelCompletedOnVal"_s},
        {u"labelCreatedBy"_s, u"labelCreatedByVal"_s},
        {u"labelCreatedOn"_s, u"labelCreatedOnVal"_s},
        {u"labelInfohash1"_s, u"labelInfohash1Val"_s},
        {u"labelInfohash2"_s, u"labelInfohash2Val"_s},
        {u"labelComment"_s, u"labelCommentVal"_s},
        {u"labelTotalSize"_s, u"labelTotalSizeVal"_s},
        {u"labelTotalPieces"_s, u"labelTotalPiecesVal"_s},
        {u"labelPrivate"_s, u"labelPrivateVal"_s}
    };

    const auto buildCard = [this, &detach, &byName, &enableTabularFigures](QGroupBox *card, const QString &title
            , const QString &iconId, const QList<Field> &fields, const bool withCopy)
    {
        card->setObjectName(u"detailsCard"_s);
        card->setTitle({});
        card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        card->setMinimumWidth((title == tr("Information")) ? 300 : 250);
        auto *grid = qobject_cast<QGridLayout *>(card->layout());
        if (grid == nullptr)
            grid = new QGridLayout(card);
        while (grid->count() > 0)
        {
            QLayoutItem *item = grid->takeAt(0);
            delete item;
        }
        grid->setContentsMargins(12, 12, 14, 12);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);
        grid->setColumnMinimumWidth(0, 128);
        grid->setColumnStretch(1, 1);

        auto *icon = new QLabel(card);
        icon->setPixmap(TorrenterIcons::icon(iconId, ThemeTokens::Token::Text2
                , ThemeTokens::currentMode()).pixmap(QSize(14, 14)));
        auto *heading = new QLabel(title, card);
        heading->setObjectName(u"detailHeader"_s);
        heading->setTextFormat(Qt::RichText);
        heading->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *cardHeader = new QHBoxLayout;
        cardHeader->setSpacing(6);
        cardHeader->addWidget(icon);
        cardHeader->addWidget(heading, 1);
        grid->addLayout(cardHeader, 0, 0, 1, 2);

        int row = 1;
        for (const Field &field : fields)
        {
            QWidget *key = byName(field.first);
            QWidget *value = byName(field.second);
            if ((key == nullptr) || (value == nullptr))
                continue;
            detach(key);
            detach(value);
            key->setObjectName(u"detailKey"_s);
            if (auto *keyLabel = qobject_cast<QLabel *>(key))
                keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            value->setObjectName(u"detailValue"_s);
            value->setMinimumWidth(0);
            value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            if (auto *valueLabel = qobject_cast<QLabel *>(value))
            {
                valueLabel->setWordWrap(true);
                enableTabularFigures(valueLabel);
            }
            grid->addWidget(key, row, 0);
            if (withCopy && ((field.first == u"labelInfohash1"_s) || (field.first == u"labelInfohash2"_s)))
            {
                auto *box = new QHBoxLayout;
                box->setSpacing(4);
                box->addWidget(value, 1);
                auto *copy = new QToolButton(card);
                copy->setObjectName(u"detailCopy"_s);
                copy->setIcon(TorrenterIcons::icon(u"edit-copy"_s, ThemeTokens::Token::Text3
                        , ThemeTokens::currentMode()));
                copy->setIconSize(QSize(12, 12));
                copy->setToolTip(tr("Copy to clipboard"));
                auto *copyLabel = qobject_cast<QLabel *>(value);
                connect(copy, &QToolButton::clicked, this, [copyLabel]()
                {
                    if (copyLabel != nullptr)
                        QApplication::clipboard()->setText(copyLabel->text());
                });
                box->addWidget(copy);
                grid->addLayout(box, row, 1);
            }
            else
            {
                grid->addWidget(value, row, 1);
            }
            ++row;
        }
    };

    auto *cards = new QWidget(this);
    cards->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    cards->setMinimumWidth(0);
    auto *cardGrid = new QGridLayout(cards);
    cardGrid->setContentsMargins(0, 0, 0, 0);
    cardGrid->setHorizontalSpacing(12);
    cardGrid->setColumnStretch(0, 4);
    cardGrid->setColumnStretch(1, 4);
    cardGrid->setColumnStretch(2, 5);

    buildCard(m_ui->groupTransferBox, tr("Transfer"), u"chart-line"_s, transferFields, false);
    auto *speedCard = new QGroupBox(this);
    buildCard(speedCard, tr("Speed &amp; peers"), u"gauge"_s, speedFields, false);
    buildCard(m_ui->groupInfosBox, tr("Information"), u"help-about"_s, infoFields, true);
    cardGrid->addWidget(m_ui->groupTransferBox, 0, 0);
    cardGrid->addWidget(speedCard, 0, 1);
    cardGrid->addWidget(m_ui->groupInfosBox, 0, 2);

    while (root->count() > 0)
    {
        QLayoutItem *item = root->takeAt(0);
        delete item;
    }
    root->addWidget(header);
    root->addWidget(cards);
}

void PropertiesWidget::updateContentSelectionLabel()
{
    if (m_contentSelectedLabel == nullptr)
        return;
    const auto *model = m_ui->filesList->model();
    if ((model == nullptr) || (model->rowCount() == 0))
    {
        m_contentSelectedLabel->setText({});
        return;
    }
    qlonglong total = 0;
    qlonglong selected = 0;
    // Count leaf files only: folder sizes are the sum of their children.
    const auto accumulate = [&model, &total, &selected](const auto &self, const QModelIndex &parent) -> void
    {
        for (int row = 0; row < model->rowCount(parent); ++row)
        {
            const QModelIndex nameIndex = model->index(row, TorrentContentModelItem::COL_NAME, parent);
            const qlonglong size = model->index(row, TorrentContentModelItem::COL_SIZE, parent)
                .data(TorrentContentModel::UnderlyingDataRole).toLongLong();
            if (model->rowCount(nameIndex) > 0)
                self(self, nameIndex);
            else
            {
                total += size;
                if (nameIndex.data(Qt::CheckStateRole).toInt() == Qt::Checked)
                    selected += size;
            }
        }
    };
    accumulate(accumulate, QModelIndex());
    m_contentSelectedLabel->setText(tr("Selected %1 of %2")
        .arg(Utils::Misc::friendlyUnit(selected), Utils::Misc::friendlyUnit(total)));
}

void PropertiesWidget::updateGeneralHeader()
{
    if ((m_detailName == nullptr) || (m_torrent == nullptr))
        return;

    m_detailName->setText(m_torrent->name());
    const QString status = m_torrent->isFinished() ? tr("Seeding")
        : (m_torrent->isStopped() ? tr("Stopped") : tr("Downloading"));
    m_detailPill->setText(status);
    m_detailPill->show();
    const QString downloaded = m_ui->labelDlTotalVal->text().section(u" ("_s, 0, 0);
    const QString eta = m_ui->labelETAVal->text();
    m_detailSummary->setText(tr("%1 of %2 · %3 left")
        .arg(downloaded, m_ui->labelTotalSizeVal->text(), eta.isEmpty() ? tr("—") : eta));
    m_detailPiecesMeta->setText(tr("· Pieces %1").arg(m_ui->labelTotalPiecesVal->text()));
}

void PropertiesWidget::updateTabCounts()
{
    if (m_tabBar == nullptr)
        return;
    const int trackerCount = m_trackerList->model() ? m_trackerList->model()->rowCount() : 0;
    const int peerCount = m_peerList->model() ? m_peerList->model()->rowCount() : 0;
    for (int i = 0; i < m_tabBar->count(); ++i)
    {
        QPushButton *button = qobject_cast<QPushButton *>(m_tabBar->itemAt(i)->widget());
        if (button == nullptr)
            continue;
        const QString base = button->property("baseText").toString();
        if ((i == PropTabBar::TrackersTab) || (i == PropTabBar::PeersTab))
        {
            const int count = (i == PropTabBar::TrackersTab) ? trackerCount : peerCount;
            button->setText(QStringLiteral("%1  %2").arg(base).arg(count));
            QFont font = button->font();
            font.setPixelSize(11);
            button->setFont(font);
        }
        else if (!base.isEmpty())
        {
            button->setText(base);
        }
    }
}

PropertiesWidget::~PropertiesWidget()
{
    delete m_tabBar;
    delete m_ui;
}

void PropertiesWidget::setCurrentTab(const int index)
{
    // Force a fresh selection so the segmented buttons repaint even when the
    // saved tab index already matches.
    if (m_tabBar != nullptr)
    {
        m_tabBar->setCurrentIndex(-1);
        m_tabBar->setCurrentIndex(index);
    }
    m_ui->stackedProperties->setCurrentIndex(index);
}

void PropertiesWidget::showPiecesAvailability(bool show)
{
    m_ui->labelPiecesAvailability->setVisible(show);
    m_piecesAvailability->setVisible(show);
    m_ui->labelAverageAvailabilityVal->setVisible(show);
    if (show || !m_downloadedPieces->isVisible())
        m_ui->lineBelowBars->setVisible(show);
}

void PropertiesWidget::showPiecesDownloaded(bool show)
{
    m_ui->labelDownloadedPieces->setVisible(show);
    m_downloadedPieces->setVisible(show);
    m_ui->labelProgressVal->setVisible(show);
    if (show || !m_piecesAvailability->isVisible())
        m_ui->lineBelowBars->setVisible(show);
}

void PropertiesWidget::animateSplitterSizes(const QList<int> &target)
{
    auto *splitter = static_cast<QSplitter *>(parentWidget());
    if (!splitter || (splitter->sizes().size() != target.size()))
    {
        m_ignoreSplitterMoved = true;
        splitter->setSizes(target);
        m_ignoreSplitterMoved = false;
        return;
    }
    m_slideStartSizes = splitter->sizes();
    m_slideTargetSizes = target;
    m_slideElapsed = 0;
    if (!m_slideAnimTimer.isActive())
    {
        m_slideAnimTimer.setInterval(16);
        connect(&m_slideAnimTimer, &QTimer::timeout, this, [this]()
        {
            m_slideElapsed += m_slideAnimTimer.interval();
            const qreal t = qMin<qreal>(1.0, static_cast<qreal>(m_slideElapsed) / 300.0);
            const qreal eased = 1.0 - std::pow(1.0 - t, 3.0);
            QList<int> sizes;
            for (int i = 0; i < m_slideStartSizes.size(); ++i)
                sizes.append(qRound(m_slideStartSizes.at(i) + (m_slideTargetSizes.at(i) - m_slideStartSizes.at(i)) * eased));
            if (auto *splitter = static_cast<QSplitter *>(parentWidget()))
            {
                m_ignoreSplitterMoved = true;
                splitter->setSizes(sizes);
                m_ignoreSplitterMoved = false;
            }
            if (t >= 1.0)
            {
                m_slideAnimTimer.stop();
                m_animatingToCollapse = false;
            }
        });
        m_slideAnimTimer.start();
    }
}

void PropertiesWidget::updateHideButton()
{
    if (!m_hideButton)
        return;
    const bool collapsed = (m_state == REDUCED);
    m_hideButton->setIcon(TorrenterIcons::icon(collapsed ? u"chevron-up"_s : u"chevron-down"_s
        , ThemeTokens::Token::Text2, ThemeTokens::currentMode()));
    m_hideButton->setToolTip(collapsed ? tr("Show details panel") : tr("Hide details panel"));
}

void PropertiesWidget::togglePanelVisibility()
{
    if (m_state == REDUCED)
    {
        const int index = qBound(0, m_expandedTabIndex, m_tabBar->count() - 1);
        m_tabBar->setCurrentIndex(index);
    }
    else
    {
        m_expandedTabIndex = m_tabBar->currentIndex();
        if (m_expandedTabIndex < 0)
            m_expandedTabIndex = 0;
        m_tabBar->setCurrentIndex(m_expandedTabIndex);
    }
    updateHideButton();
    saveSettings();
}

void PropertiesWidget::onSplitterMoved()
{
    if (m_ignoreSplitterMoved || m_slideAnimTimer.isActive() || m_animatingToCollapse)
        return;

    auto *hSplitter = qobject_cast<QSplitter *>(parentWidget());
    if (!hSplitter || (hSplitter->sizes().size() != 2))
        return;

    const int tabBarHeight = m_tabTrack->height();
    if ((m_state == REDUCED) && (hSplitter->sizes().last() > (tabBarHeight + 16)))
    {
        // A drag from the collapsed state is an explicit expand.
        m_ignoreSplitterMoved = true;
        if (m_handleWidth != -1)
            hSplitter->setHandleWidth(m_handleWidth);
        m_ui->stackedProperties->setVisible(true);
        hSplitter->handle(1)->setDisabled(false);
        hSplitter->handle(1)->setVisible(true);
        setMaximumSize(maximumSize().width(), QWIDGETSIZE_MAX);
        m_state = VISIBLE;
        if (m_tabBar->currentIndex() < 0)
            m_tabBar->setCurrentIndex(m_expandedTabIndex);
        m_slideSizes = hSplitter->sizes();
        m_ignoreSplitterMoved = false;
        updateHideButton();
        saveSettings();
        loadDynamicData();
        return;
    }

    if (m_state == VISIBLE)
    {
        m_slideSizes = hSplitter->sizes();
        saveSettings();
    }
}

void PropertiesWidget::setVisibility(const bool visible)
{
    auto *hSplitter = static_cast<QSplitter *>(parentWidget());
    if (!hSplitter || (hSplitter->sizes().size() != 2))
        return;

    if (!visible && (m_state == VISIBLE))
    {
        const int tabBarHeight = m_tabTrack->height();
        const int collapsedHandleWidth = 4;
        m_ui->stackedProperties->setVisible(false);
        m_animatingToCollapse = true;
        m_slideSizes = hSplitter->sizes();
        if (m_handleWidth == -1)
            m_handleWidth = hSplitter->handleWidth();
        hSplitter->handle(1)->setDisabled(false);
        hSplitter->handle(1)->setVisible(true);
        hSplitter->setHandleWidth(collapsedHandleWidth);
        const QList<int> sizes {(hSplitter->height() - tabBarHeight - collapsedHandleWidth), tabBarHeight};
        animateSplitterSizes(sizes);
        setMaximumSize(maximumSize().width(), QWIDGETSIZE_MAX);
        m_state = REDUCED;
        updateHideButton();
        return;
    }

    if (visible && (m_state == REDUCED))
    {
        m_ui->stackedProperties->setVisible(true);
        if (m_handleWidth != -1)
            hSplitter->setHandleWidth(m_handleWidth);
        hSplitter->handle(1)->setDisabled(false);
        hSplitter->handle(1)->setVisible(true);
        if ((m_slideSizes.size() != 2) || (m_slideSizes.last() <= m_tabTrack->height()))
            m_slideSizes = {qMax(120, hSplitter->height() - 260), 260};
        animateSplitterSizes(m_slideSizes);
        m_state = VISIBLE;
        setMaximumSize(maximumSize().width(), QWIDGETSIZE_MAX);
        updateHideButton();
        loadDynamicData();
    }
}

void PropertiesWidget::clear()
{
    qDebug("Clearing torrent properties");
    m_ui->labelSavePathVal->clear();
    m_ui->labelCreatedOnVal->clear();
    m_ui->labelTotalPiecesVal->clear();
    m_ui->labelPrivateVal->clear();
    m_ui->labelInfohash1Val->clear();
    m_ui->labelInfohash2Val->clear();
    m_ui->labelCommentVal->clear();
    m_ui->labelProgressVal->clear();
    m_ui->labelAverageAvailabilityVal->clear();
    m_ui->labelWastedVal->clear();
    m_ui->labelUpTotalVal->clear();
    m_ui->labelDlTotalVal->clear();
    m_ui->labelUpLimitVal->clear();
    m_ui->labelDlLimitVal->clear();
    m_ui->labelElapsedVal->clear();
    m_ui->labelConnectionsVal->clear();
    m_ui->labelReannounceInVal->clear();
    m_ui->labelShareRatioVal->clear();
    m_ui->labelPopularityVal->clear();
    m_ui->listWebSeeds->clear();
    m_ui->labelETAVal->clear();
    m_ui->labelSeedsVal->clear();
    m_ui->labelPeersVal->clear();
    m_ui->labelDlSpeedVal->clear();
    m_ui->labelUpSpeedVal->clear();
    m_ui->labelTotalSizeVal->clear();
    m_ui->labelCompletedOnVal->clear();
    m_ui->labelLastSeenCompleteVal->clear();
    m_ui->labelCreatedByVal->clear();
    m_ui->labelAddedOnVal->clear();
    m_downloadedPieces->clear();
    m_piecesAvailability->clear();
    m_peerList->clear();
    m_contentFilterLine->clear();
}

BitTorrent::Torrent *PropertiesWidget::getCurrentTorrent() const
{
    return m_torrent;
}

TrackerListWidget *PropertiesWidget::getTrackerList() const
{
    return m_trackerList;
}

PeerListWidget *PropertiesWidget::getPeerList() const
{
    return m_peerList;
}

QTreeView *PropertiesWidget::getFilesList() const
{
    return m_ui->filesList;
}

PropTabBar *PropertiesWidget::tabBar() const
{
    return m_tabBar;
}

LineEdit *PropertiesWidget::contentFilterLine() const
{
    return m_contentFilterLine;
}

void PropertiesWidget::updateSavePath(BitTorrent::Torrent *const torrent)
{
    if (torrent == m_torrent)
        m_ui->labelSavePathVal->setText(m_torrent->savePath().toString());
}

void PropertiesWidget::showContentFilterContextMenu()
{
    QMenu *menu = m_contentFilterLine->createStandardContextMenu();

    auto *formatMenu = new FilterPatternFormatMenu(m_storeFilterPatternFormat.get(FilterPatternFormat::Wildcards), menu);
    connect(formatMenu, &FilterPatternFormatMenu::patternFormatChanged, this, [this](const FilterPatternFormat format)
    {
        m_storeFilterPatternFormat = format;
        setContentFilterPattern();
    });

    menu->addSeparator();
    menu->addMenu(formatMenu);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(QCursor::pos());
}

void PropertiesWidget::setContentFilterPattern()
{
    m_ui->filesList->setFilterPattern(m_contentFilterLine->text(), m_storeFilterPatternFormat.get(FilterPatternFormat::Wildcards));
}

void PropertiesWidget::loadTorrentInfos(BitTorrent::Torrent *const torrent)
{
    clear();
    m_torrent = torrent;
    m_downloadedPieces->setTorrent(m_torrent);
    m_piecesAvailability->setTorrent(m_torrent);
    m_trackerList->setTorrent(m_torrent);
    m_ui->filesList->setContentHandler(m_torrent);
    if (!m_torrent)
        return;

    // Save path
    updateSavePath(m_torrent);
    // Info hashes
    m_ui->labelInfohash1Val->setText(m_torrent->infoHash().v1().isValid() ? m_torrent->infoHash().v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Val->setText(m_torrent->infoHash().v2().isValid() ? m_torrent->infoHash().v2().toString() : tr("N/A"));
    // URL seeds
    loadUrlSeeds();
    if (m_torrent->hasMetadata())
    {
        // Creation date
        m_ui->labelCreatedOnVal->setText(QLocale().toString(m_torrent->creationDate(), QLocale::ShortFormat));

        m_ui->labelTotalSizeVal->setText(Utils::Misc::friendlyUnit(m_torrent->totalSize()));

        // Comment
        m_ui->labelCommentVal->setText(Utils::Misc::parseHtmlLinks(m_torrent->comment().toHtmlEscaped()));

        m_ui->labelCreatedByVal->setText(m_torrent->creator());

        m_ui->labelPrivateVal->setText(m_torrent->isPrivate() ? tr("Yes") : tr("No"));
    }
    else
    {
        m_ui->labelPrivateVal->setText(tr("N/A"));
    }

    // Load dynamic data
    loadDynamicData();
}

void PropertiesWidget::readSettings()
{
    const Preferences *const pref = Preferences::instance();
    // Restore splitter sizes
    QStringList sizesStr = pref->getPropSplitterSizes().split(u',');
    if (sizesStr.size() == 2)
    {
        m_slideSizes << sizesStr.first().toInt();
        m_slideSizes << sizesStr.last().toInt();
        auto *hSplitter = static_cast<QSplitter *>(parentWidget());
        hSplitter->setSizes(m_slideSizes);
    }
    const int currentTab = pref->getPropCurTab();
    const bool visible = pref->getPropVisible();
    m_expandedTabIndex = currentTab;
    m_ui->filesList->header()->restoreState(pref->getPropFileListState());
    m_tabBar->setCurrentIndex(currentTab);
    if (!visible)
    {
        setVisibility(false);
        saveSettings();
    }
    updateHideButton();
}

void PropertiesWidget::saveSettings()
{
    Preferences *const pref = Preferences::instance();
    pref->setPropVisible(m_state == VISIBLE);
    // Splitter sizes
    auto *hSplitter = static_cast<QSplitter *>(parentWidget());
    QList<int> sizes;
    if (m_state == VISIBLE)
        sizes = hSplitter->sizes();
    else
        sizes = m_slideSizes;

    if (sizes.size() == 2)
        pref->setPropSplitterSizes(QString::number(sizes.first()) + u',' + QString::number(sizes.last()));
    pref->setPropFileListState(m_ui->filesList->header()->saveState());
    // Remember current tab
    pref->setPropCurTab(m_tabBar->currentIndex());
}

void PropertiesWidget::reloadPreferences()
{
    // Take program preferences into consideration
    m_peerList->updatePeerHostNameResolutionState();
    m_peerList->updatePeerCountryResolutionState();
}

void PropertiesWidget::loadDynamicData()
{
    // Refresh only if the torrent handle is valid and visible
    if (!m_torrent || (m_state != VISIBLE)) return;

    updateTabCounts();

    // Transfer infos
    switch (m_ui->stackedProperties->currentIndex())
    {
    case PropTabBar::MainTab:
        {
            m_ui->labelWastedVal->setText(Utils::Misc::friendlyUnit(m_torrent->wastedSize()));

            m_ui->labelUpTotalVal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalUpload())
                , Utils::Misc::friendlyUnit(m_torrent->totalPayloadUpload())));

            m_ui->labelDlTotalVal->setText(tr("%1 (%2 this session)").arg(Utils::Misc::friendlyUnit(m_torrent->totalDownload())
                , Utils::Misc::friendlyUnit(m_torrent->totalPayloadDownload())));

            m_ui->labelUpLimitVal->setText(m_torrent->uploadLimit() <= 0 ? C_INFINITY : Utils::Misc::friendlyUnit(m_torrent->uploadLimit(), true));

            m_ui->labelDlLimitVal->setText(m_torrent->downloadLimit() <= 0 ? C_INFINITY : Utils::Misc::friendlyUnit(m_torrent->downloadLimit(), true));

            QString elapsedString;
            if (m_torrent->isFinished())
            {
                elapsedString = tr("%1 (seeded for %2)", "e.g. 4m39s (seeded for 3m10s)")
                        .arg(Utils::Misc::userFriendlyDuration(m_torrent->activeTime())
                                , Utils::Misc::userFriendlyDuration(m_torrent->finishedTime()));
            }
            else
            {
                elapsedString = Utils::Misc::userFriendlyDuration(m_torrent->activeTime());
            }
            m_ui->labelElapsedVal->setText(elapsedString);

            m_ui->labelConnectionsVal->setText(tr("%1 (%2 max)", "%1 and %2 are numbers, e.g. 3 (10 max)")
                                           .arg(m_torrent->connectionsCount())
                                           .arg(m_torrent->connectionsLimit() < 0 ? C_INFINITY : QString::number(m_torrent->connectionsLimit())));

            m_ui->labelETAVal->setText(Utils::Misc::userFriendlyDuration(m_torrent->eta(), MAX_ETA, Utils::Misc::TimeResolution::Seconds));

            // Update next announce time
            m_ui->labelReannounceInVal->setText(Utils::Misc::userFriendlyDuration(m_torrent->nextAnnounce()));

            // Update ratio info
            const qreal ratio = m_torrent->realRatio();
            m_ui->labelShareRatioVal->setText(ratio >= BitTorrent::Torrent::MAX_RATIO ? C_INFINITY : Utils::String::fromDouble(ratio, 2));

            const qreal popularity = m_torrent->popularity();
            m_ui->labelPopularityVal->setText(popularity >= BitTorrent::Torrent::MAX_RATIO ? C_INFINITY : Utils::String::fromDouble(popularity, 2));

            m_ui->labelSeedsVal->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                .arg(QString::number(m_torrent->seedsCount())
                    , QString::number(m_torrent->totalSeedsCount())));

            m_ui->labelPeersVal->setText(tr("%1 (%2 total)", "%1 and %2 are numbers, e.g. 3 (10 total)")
                .arg(QString::number(m_torrent->leechsCount())
                    , QString::number(m_torrent->totalLeechersCount())));

            const qlonglong dlDuration = m_torrent->activeTime() - m_torrent->finishedTime();
            const QString dlAvg = Utils::Misc::friendlyUnit((m_torrent->totalDownload() / ((dlDuration == 0) ? -1 : dlDuration)), true);
            m_ui->labelDlSpeedVal->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                .arg(Utils::Misc::friendlyUnit(m_torrent->downloadPayloadRate(), true), dlAvg));

            const qlonglong ulDuration = m_torrent->activeTime();
            const QString ulAvg = Utils::Misc::friendlyUnit((m_torrent->totalUpload() / ((ulDuration == 0) ? -1 : ulDuration)), true);
            m_ui->labelUpSpeedVal->setText(tr("%1 (%2 avg.)", "%1 and %2 are speed rates, e.g. 200KiB/s (100KiB/s avg.)")
                .arg(Utils::Misc::friendlyUnit(m_torrent->uploadPayloadRate(), true), ulAvg));

            m_ui->labelLastSeenCompleteVal->setText(m_torrent->lastSeenComplete().isValid() ? QLocale().toString(m_torrent->lastSeenComplete(), QLocale::ShortFormat) : tr("Never"));

            m_ui->labelCompletedOnVal->setText(m_torrent->completedTime().isValid() ? QLocale().toString(m_torrent->completedTime(), QLocale::ShortFormat) : QString {});

            m_ui->labelAddedOnVal->setText(QLocale().toString(m_torrent->addedTime(), QLocale::ShortFormat));

            if (m_torrent->hasMetadata())
            {
                m_ui->labelTotalPiecesVal->setText(tr("%1 x %2 (have %3)", "(torrent pieces) eg 152 x 4MB (have 25)").arg(m_torrent->piecesCount()).arg(Utils::Misc::friendlyUnit(m_torrent->pieceLength())).arg(m_torrent->piecesHave()));

                if (!m_torrent->isFinished() && !m_torrent->isStopped() && !m_torrent->isQueued() && !m_torrent->isChecking())
                {
                    // Pieces availability
                    showPiecesAvailability(true);

                    m_torrent->fetchPieceAvailability().then(this
                            , [this, torrent = QPointer(m_torrent)](const QList<int> &pieceAvailability)
                    {
                        if (m_torrent && (m_torrent == torrent))
                            m_piecesAvailability->setAvailability(pieceAvailability);
                    });

                    m_ui->labelAverageAvailabilityVal->setText(Utils::String::fromDouble(m_torrent->distributedCopies(), 3));
                }
                else
                {
                    showPiecesAvailability(false);
                }

                // Progress
                qreal progress = m_torrent->progress() * 100.;
                m_ui->labelProgressVal->setText(Utils::String::fromDouble(progress, 1) + u'%');
                updateGeneralHeader();

                m_torrent->fetchDownloadingPieces().then(this
                        , [this, torrent = QPointer(m_torrent)](const QBitArray &downloadingPieces)
                {
                    if (m_torrent && (m_torrent == torrent))
                        m_downloadedPieces->setProgress(m_torrent->pieces(), downloadingPieces);
                });
            }
            else
            {
                showPiecesAvailability(false);
            }
        }
        break;
    case PropTabBar::PeersTab:
        // Load peers
        m_peerList->loadPeers(m_torrent);
        break;
    case PropTabBar::FilesTab:
        m_ui->filesList->refresh();
        updateContentSelectionLabel();
        break;
    default:;
    }
}

void PropertiesWidget::loadUrlSeeds()
{
    if (!m_torrent)
        return;

    m_torrent->fetchURLSeeds().then(this, [this, torrent = QPointer(m_torrent)](const QList<QUrl> &urlSeeds)
    {
        if (!m_torrent || (m_torrent != torrent))
            return;

        m_ui->listWebSeeds->clear();
        qDebug("Loading web seeds");
        // Add url seeds
        for (const QUrl &urlSeed : urlSeeds)
        {
            qDebug("Loading web seed: %s", qUtf8Printable(urlSeed.toString()));
            new QListWidgetItem(urlSeed.toString(), m_ui->listWebSeeds);
        }
        if (m_noHttpSourcesLabel)
            m_noHttpSourcesLabel->setVisible(m_ui->listWebSeeds->count() == 0);
    });
}

void PropertiesWidget::displayWebSeedListMenu()
{
    if (!m_torrent) return;

    const QModelIndexList rows = m_ui->listWebSeeds->selectionModel()->selectedRows();

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("Add web seed..."), this, &PropertiesWidget::askWebSeed);

    if (!rows.isEmpty())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Remove web seed")
            , this, &PropertiesWidget::deleteSelectedUrlSeeds);
        menu->addSeparator();
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy web seed URL")
            , this, &PropertiesWidget::copySelectedWebSeedsToClipboard);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Edit web seed URL...")
            , this, &PropertiesWidget::editWebSeed);
    }

    menu->popup(QCursor::pos());
}

void PropertiesWidget::configure()
{
    const auto *preferences = Preferences::instance();

    // Torrent Content Widget
    m_ui->filesList->setContentDragEnabled(preferences->isTorrentContentDragEnabled());

    // Speed widget
    if (preferences->isSpeedWidgetEnabled())
    {
        if (!qobject_cast<SpeedWidget *>(m_speedWidget))
        {
            if (m_speedWidget)
            {
                m_ui->speedLayout->removeWidget(m_speedWidget);
                delete m_speedWidget;
            }

            m_speedWidget = new SpeedWidget(this);
            m_ui->speedLayout->addWidget(m_speedWidget);
        }
    }
    else
    {
        if (!qobject_cast<QLabel *>(m_speedWidget))
        {
            if (m_speedWidget)
            {
                m_ui->speedLayout->removeWidget(m_speedWidget);
                delete m_speedWidget;
            }

            const auto displayText = u"<center><b>%1</b><p><a href=\"#\">%2</a></p></center>"_s
                .arg(tr("Speed graphs are disabled"), tr("You can enable it in Advanced Options"));
            auto *label = new QLabel(displayText, this);
            label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            connect(label, &QLabel::linkActivated, this, &PropertiesWidget::openAdvancedSettingsLinkActivated);
            m_speedWidget = label;
            m_ui->speedLayout->addWidget(m_speedWidget);
        }
    }
    updateHideButton();
}

void PropertiesWidget::askWebSeed()
{
    bool ok = false;
    // Ask user for a new url seed
    const QString urlSeed = AutoExpandableDialog::getText(this, tr("Add web seed", "Add HTTP source"),
                                                           tr("Add web seed:"), QLineEdit::Normal,
                                                           u"http://www."_s, &ok);
    if (!ok) return;
    qDebug("Adding %s web seed", qUtf8Printable(urlSeed));
    if (!m_ui->listWebSeeds->findItems(urlSeed, Qt::MatchFixedString).empty())
    {
        QMessageBox::warning(this, u"Torrenter"_s, tr("This web seed is already in the list."), QMessageBox::Ok);
        return;
    }
    if (m_torrent)
        m_torrent->addUrlSeeds({urlSeed});
    // Refresh the seeds list
    loadUrlSeeds();
}

void PropertiesWidget::deleteSelectedUrlSeeds()
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.isEmpty()) return;

    QList<QUrl> urlSeeds;
    urlSeeds.reserve(selectedItems.size());

    for (const QListWidgetItem *item : selectedItems)
        urlSeeds << item->text();

    m_torrent->removeUrlSeeds(urlSeeds);
    // Refresh list
    loadUrlSeeds();
}

void PropertiesWidget::copySelectedWebSeedsToClipboard() const
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.isEmpty()) return;

    QStringList urlsToCopy;
    for (const QListWidgetItem *item : selectedItems)
        urlsToCopy << item->text();

    QApplication::clipboard()->setText(urlsToCopy.join(u'\n'));
}

void PropertiesWidget::editWebSeed()
{
    const QList<QListWidgetItem *> selectedItems = m_ui->listWebSeeds->selectedItems();
    if (selectedItems.size() != 1) return;

    const QListWidgetItem *selectedItem = selectedItems.last();
    const QString oldSeed = selectedItem->text();
    bool result;
    const QString newSeed = AutoExpandableDialog::getText(this, tr("Web seed editing"),
                                                           tr("Web seed URL:"), QLineEdit::Normal,
                                                           oldSeed, &result);
    if (!result) return;

    if (!m_ui->listWebSeeds->findItems(newSeed, Qt::MatchFixedString).empty())
    {
        QMessageBox::warning(this, u"Torrenter"_s,
                             tr("This web seed is already in the list."),
                             QMessageBox::Ok);
        return;
    }

    m_torrent->removeUrlSeeds({oldSeed});
    m_torrent->addUrlSeeds({newSeed});
    loadUrlSeeds();
}
