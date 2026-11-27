/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "peerlistwidget.h"

#include <algorithm>

#include <QApplication>
#include <QClipboard>
#include <QFuture>
#include <QHeaderView>
#include <QHostAddress>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QWheelEvent>

#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/geoipmanager.h"
#include "base/net/reverseresolution.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "gui/themetokens.h"
#include "gui/uithememanager.h"
#include "gui/utils/keysequence.h"
#include "peerlistsortmodel.h"
#include "peersadditiondialog.h"
#include "propertieswidget.h"

struct PeerEndpoint
{
    BitTorrent::PeerAddress address;
    QString connectionType; // matches return type of `PeerInfo::connectionType()`

    friend bool operator==(const PeerEndpoint &left, const PeerEndpoint &right) = default;
};

std::size_t qHash(const PeerEndpoint &peerEndpoint, const std::size_t seed = 0)
{
    return qHashMulti(seed, peerEndpoint.address, peerEndpoint.connectionType);
}

namespace
{
    void setModelData(QStandardItemModel *model, const int row, const int column, const QString &displayData
            , const QVariant &underlyingData, const Qt::Alignment textAlignmentData = {}, const QString &toolTip = {})
    {
        const QMap<int, QVariant> data = {
            {Qt::DisplayRole, displayData},
            {PeerListSortModel::UnderlyingDataRole, underlyingData},
            {Qt::TextAlignmentRole, QVariant {textAlignmentData}},
            {Qt::ToolTipRole, toolTip}};

        model->setItemData(model->index(row, column), data);
    }
}

namespace
{
    class PeerListItemDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            if (!index.isValid())
                return QSize(option.rect.width(), 34);
            QSize size = QStyledItemDelegate::sizeHint(option, index);
            size.setHeight(34);
            return size;
        }

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            const auto mode = ThemeTokens::currentMode();
            const auto color = [mode](const ThemeTokens::Token token) { return ThemeTokens::color(token, mode); };
            const auto drawBackground = [painter, option, color]()
            {
                QPainterPath path;
                path.addRoundedRect(option.rect.adjusted(2, 1, -2, -1), 6, 6);
                const bool selected = option.state.testFlag(QStyle::State_Selected);
                const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
                painter->setPen(Qt::NoPen);
                painter->setBrush(selected ? color(ThemeTokens::Token::Selected)
                    : (hovered ? color(ThemeTokens::Token::Hover) : Qt::transparent));
                painter->drawPath(path);
            };

            switch (index.column())
            {
            case PeerListWidget::COUNTRY:
                drawBackground();
                {
                    QStyleOptionViewItem opt {option};
                    opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
                    opt.text.clear();
                    opt.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
                    QStyledItemDelegate::paint(painter, opt, index);
                    const QIcon flag = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
                    if (!flag.isNull())
                    {
                        const QRect r {option.rect.left() + 6, option.rect.top() + 11, 16, 12};
                        QPainterPath shape;
                        shape.addRoundedRect(r, 2, 2);
                        painter->save();
                        painter->setClipPath(shape);
                        painter->drawPixmap(r, flag.pixmap(16, 12));
                        painter->restore();
                        painter->setPen(QPen(QColor(255, 255, 255, 20), 1));
                        painter->setBrush(Qt::NoBrush);
                        painter->drawPath(shape);
                    }
                }
                break;
            case PeerListWidget::IP:
                drawBackground();
                {
                    QFont font = option.font;
                    font.setPixelSize(12);
                    font.setStyleHint(QFont::Monospace);
                    painter->setFont(font);
                    const QString ip = index.data().toString();
                    const QString port = index.siblingAtColumn(PeerListWidget::PORT).data().toString();
                    const QFontMetrics metrics {font};
                    const int x = option.rect.left() + 8;
                    const int y = option.rect.center().y() + metrics.height() / 2 - 2;
                    painter->setPen(color(ThemeTokens::Token::Text1));
                    painter->drawText(x, y, ip);
                    painter->setPen(color(ThemeTokens::Token::Text3));
                    painter->drawText(x + metrics.horizontalAdvance(ip), y, u":"_s + port);
                }
                break;
            case PeerListWidget::FLAGS:
                drawBackground();
                {
                    const QString text = index.data().toString();
                    const QRect chip {option.rect.left() + 4, option.rect.top() + 7, option.rect.width() - 8, 20};
                    QPainterPath path;
                    path.addRoundedRect(chip, 5, 5);
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(color(ThemeTokens::Token::Pressed));
                    painter->drawPath(path);
                    QFont font = option.font;
                    font.setPixelSize(11);
                    font.setWeight(QFont::DemiBold);
                    painter->setFont(font);
                    painter->setPen(color(ThemeTokens::Token::Text2));
                    painter->drawText(chip, Qt::AlignCenter, text);
                }
                break;
            case PeerListWidget::DOWN_SPEED:
                drawBackground();
                {
                    const bool active = index.data(PeerListSortModel::UnderlyingDataRole).toLongLong() > 0;
                    painter->setPen(color(active ? ThemeTokens::Token::Inkdown : ThemeTokens::Token::Text3));
                    painter->drawText(option.rect.adjusted(0, 0, 6, 0), Qt::AlignRight | Qt::AlignVCenter, index.data().toString());
                }
                break;
            default:
                drawBackground();
                {
                    QStyleOptionViewItem opt {option};
                    opt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver);
                    QStyledItemDelegate::paint(painter, opt, index);
                }
                break;
            }
        }
    };
}

PeerListWidget::PeerListWidget(PropertiesWidget *parent)
    : QTreeView(parent)
    , m_properties(parent)
{
    // Load settings
    const bool columnLoaded = loadSettings();
    // Visual settings
    setItemDelegate(new PeerListItemDelegate(this));
    // List Model
    m_listModel = new QStandardItemModel(0, PeerListColumns::COL_COUNT, this);
    m_listModel->setHeaderData(PeerListColumns::COUNTRY, Qt::Horizontal, tr("Country/Region")); // Country flag column
    m_listModel->setHeaderData(PeerListColumns::IP, Qt::Horizontal, tr("IP/Address"));
    m_listModel->setHeaderData(PeerListColumns::PORT, Qt::Horizontal, tr("Port"));
    m_listModel->setHeaderData(PeerListColumns::FLAGS, Qt::Horizontal, tr("Flags"));
    m_listModel->setHeaderData(PeerListColumns::CONNECTION, Qt::Horizontal, tr("Connection"));
    m_listModel->setHeaderData(PeerListColumns::CLIENT, Qt::Horizontal, tr("Client", "i.e.: Client application"));
    m_listModel->setHeaderData(PeerListColumns::PEERID_CLIENT, Qt::Horizontal, tr("Peer ID Client", "i.e.: Client resolved from Peer ID"));
    m_listModel->setHeaderData(PeerListColumns::PROGRESS, Qt::Horizontal, tr("Progress", "i.e: % downloaded"));
    m_listModel->setHeaderData(PeerListColumns::DOWN_SPEED, Qt::Horizontal, tr("Down Speed", "i.e: Download speed"));
    m_listModel->setHeaderData(PeerListColumns::UP_SPEED, Qt::Horizontal, tr("Up Speed", "i.e: Upload speed"));
    m_listModel->setHeaderData(PeerListColumns::TOT_DOWN, Qt::Horizontal, tr("Downloaded", "i.e: total data downloaded"));
    m_listModel->setHeaderData(PeerListColumns::TOT_UP, Qt::Horizontal, tr("Uploaded", "i.e: total data uploaded"));
    m_listModel->setHeaderData(PeerListColumns::RELEVANCE, Qt::Horizontal, tr("Relevance", "i.e: How relevant this peer is to us. How many pieces it has that we don't."));
    m_listModel->setHeaderData(PeerListColumns::CONTRIBUTION, Qt::Horizontal, tr("Contribution", "i.e: How much of this peer's current progress was provided by us"));
    m_listModel->setHeaderData(PeerListColumns::DOWNLOADING_PIECE, Qt::Horizontal, tr("Files", "i.e. files that are being downloaded right now"));
    // Set header text alignment
    m_listModel->setHeaderData(PeerListColumns::PORT, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::PROGRESS, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::DOWN_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::UP_SPEED, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::TOT_DOWN, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::TOT_UP, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::RELEVANCE, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_listModel->setHeaderData(PeerListColumns::CONTRIBUTION, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    // Proxy model to support sorting without actually altering the underlying model
    m_proxyModel = new PeerListSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_listModel);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    setModel(m_proxyModel);
    // §5.4 columns: address, flag, client, progress, down/up, flags, conn.
    for (const int hidden : {PeerListWidget::PORT, PeerListWidget::PEERID_CLIENT,
             PeerListWidget::TOT_DOWN, PeerListWidget::TOT_UP, PeerListWidget::RELEVANCE,
             PeerListWidget::DOWNLOADING_PIECE, PeerListWidget::CONTRIBUTION, PeerListWidget::IP_HIDDEN})
        setColumnHidden(hidden, true);
    header()->setSectionResizeMode(PeerListWidget::IP, QHeaderView::Stretch);
    for (const auto &columnWidth : {QPair<int, int> {PeerListWidget::COUNTRY, 56},
             {PeerListWidget::FLAGS, 80}, {PeerListWidget::CLIENT, 180},
             {PeerListWidget::PROGRESS, 90}, {PeerListWidget::DOWN_SPEED, 110},
             {PeerListWidget::UP_SPEED, 100}, {PeerListWidget::CONNECTION, 70}})
    {
        header()->setSectionResizeMode(columnWidth.first, QHeaderView::Fixed);
        setColumnWidth(columnWidth.first, columnWidth.second);
    }
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setAllColumnsShowFocus(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    header()->setFirstSectionMovable(true);
    header()->setStretchLastSection(false);
    header()->setTextElideMode(Qt::ElideRight);

    hideColumn(PeerListColumns::IP_HIDDEN);
    hideColumn(PeerListColumns::COL_COUNT);

    // Default hidden columns
    if (!columnLoaded)
    {
        hideColumn(PeerListColumns::PEERID_CLIENT);
    }

    m_resolveCountries = Preferences::instance()->resolvePeerCountries();
    if (!m_resolveCountries)
        hideColumn(PeerListColumns::COUNTRY);
    // Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if (!isColumnHidden(i))
        {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        setColumnHidden(PeerListColumns::IP, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    }
    // Context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &PeerListWidget::showPeerListMenu);
    // Enable sorting
    setSortingEnabled(true);
    // IP to Hostname resolver
    updatePeerHostNameResolutionState();
    // SIGNAL/SLOT
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &PeerListWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionClicked, this, &PeerListWidget::handleSortColumnChanged);
    connect(header(), &QHeaderView::sectionMoved, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &PeerListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &PeerListWidget::saveSettings);
    handleSortColumnChanged(header()->sortIndicatorSection());
    const auto *copyHotkey = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copyHotkey, &QShortcut::activated, this, &PeerListWidget::copySelectedPeers);
    const auto *deleteHotkey = new QShortcut(Utils::KeySequence::deleteItem(), this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &PeerListWidget::banSelectedPeers);
}

PeerListWidget::~PeerListWidget()
{
    saveSettings();
}

void PeerListWidget::displayColumnHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));
    menu->setToolTipsVisible(true);

    for (int i = 0; i < PeerListColumns::IP_HIDDEN; ++i)
    {
        if ((i == PeerListColumns::COUNTRY) && !Preferences::instance()->resolvePeerCountries())
            continue;

        const auto columnName = m_listModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        QAction *action = menu->addAction(columnName, this, [this, i](const bool checked)
        {
            if (!checked && (visibleColumnsCount() <= 1))
                return;

            setColumnHidden(i, !checked);

            if (checked && (columnWidth(i) <= 5))
                resizeColumnToContents(i);

            saveSettings();
        });
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(i));
    }

    menu->addSeparator();
    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = header()->count(); i < count; ++i)
        {
            if (!isColumnHidden(i))
                resizeColumnToContents(i);
        }
        saveSettings();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void PeerListWidget::updatePeerHostNameResolutionState()
{
    const bool resolveHostNames = Preferences::instance()->resolvePeerHostNames();
    if (resolveHostNames == m_resolveHostNames)
        return;

    m_resolveHostNames = resolveHostNames;
    if (m_resolveHostNames)
    {
        connect(Net::ReverseResolution::instance(), &Net::ReverseResolution::ipResolved
                , this, &PeerListWidget::handleResolved);
        loadPeers(m_properties->getCurrentTorrent());
    }
    else
    {
        disconnect(Net::ReverseResolution::instance(), &Net::ReverseResolution::ipResolved
                , this, &PeerListWidget::handleResolved);
    }
}

void PeerListWidget::updatePeerCountryResolutionState()
{
    const bool resolveCountries = Preferences::instance()->resolvePeerCountries();
    if (resolveCountries == m_resolveCountries)
        return;

    m_resolveCountries = resolveCountries;
    if (m_resolveCountries)
    {
        loadPeers(m_properties->getCurrentTorrent());
        showColumn(PeerListColumns::COUNTRY);
        if (columnWidth(PeerListColumns::COUNTRY) <= 0)
            resizeColumnToContents(PeerListColumns::COUNTRY);
    }
    else
    {
        hideColumn(PeerListColumns::COUNTRY);
    }
}

void PeerListWidget::addPeers()
{
    BitTorrent::Torrent *torrent = m_properties->getCurrentTorrent();
    if (torrent == nullptr)
        return;
    const QList<BitTorrent::PeerAddress> peersList = PeersAdditionDialog::askForPeers(this);
    const int peerCount = std::ranges::count_if(peersList, [torrent](const BitTorrent::PeerAddress &peer)
    {
        return torrent->connectPeer(peer);
    });
    if (peerCount < peersList.length())
        QMessageBox::information(this, tr("Adding peers"), tr("Some peers cannot be added. Check the Log for details."));
    else if (peerCount > 0)
        QMessageBox::information(this, tr("Adding peers"), tr("Peers are added to this torrent."));
}

void PeerListWidget::showPeerListMenu()
{
    BitTorrent::Torrent *torrent = m_properties->getCurrentTorrent();
    if (!torrent) return;

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);

    QAction *addNewPeer = menu->addAction(UIThemeManager::instance()->getIcon(u"peers-add"_s), tr("Add peers...")
        , this, &PeerListWidget::addPeers);
    QAction *copyPeers = menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy IP:port")
        , this, &PeerListWidget::copySelectedPeers);
    menu->addSeparator();
    QAction *banPeers = menu->addAction(UIThemeManager::instance()->getIcon(u"peers-remove"_s), tr("Ban peer permanently")
        , this, &PeerListWidget::banSelectedPeers);

    // disable actions
    const auto disableAction = [](QAction *action, const QString &tooltip)
    {
        action->setEnabled(false);
        action->setToolTip(tooltip);
    };

    if (torrent->isPrivate())
        disableAction(addNewPeer, tr("Cannot add peers to a private torrent"));
    else if (torrent->isChecking())
        disableAction(addNewPeer, tr("Cannot add peers when the torrent is checking"));
    else if (torrent->isQueued())
        disableAction(addNewPeer, tr("Cannot add peers when the torrent is queued"));

    if (selectionModel()->selectedRows().isEmpty())
    {
        const QString tooltip = tr("No peer was selected");
        disableAction(copyPeers, tooltip);
        disableAction(banPeers, tooltip);
    }

    menu->popup(QCursor::pos());
}

void PeerListWidget::banSelectedPeers()
{
    // Store selected rows first as selected peers may disconnect
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();

    QList<QString> selectedIPs;
    selectedIPs.reserve(selectedIndexes.size());

    for (const QModelIndex &index : selectedIndexes)
    {
        const int row = m_proxyModel->mapToSource(index).row();
        const QString ip = m_listModel->item(row, PeerListColumns::IP_HIDDEN)->text();
        selectedIPs += ip;
    }

    // Confirm before banning peer
    const QMessageBox::StandardButton btn = QMessageBox::question(this, tr("Ban peer permanently")
        , tr("Are you sure you want to permanently ban the selected peers?"));
    if (btn != QMessageBox::Yes) return;

    for (const QString &ip : selectedIPs)
    {
        BitTorrent::Session::instance()->banIP(ip);
        LogMsg(tr("Peer \"%1\" is manually banned").arg(ip));
    }
    // Refresh list
    loadPeers(m_properties->getCurrentTorrent());
}

void PeerListWidget::copySelectedPeers()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    QStringList selectedPeers;

    for (const QModelIndex &index : selectedIndexes)
    {
        const int row = m_proxyModel->mapToSource(index).row();
        const QString ip = m_listModel->item(row, PeerListColumns::IP_HIDDEN)->text();
        const QString port = m_listModel->item(row, PeerListColumns::PORT)->text();

        if (!ip.contains(u'.'))  // IPv6
            selectedPeers << (u'[' + ip + u"]:" + port);
        else  // IPv4
            selectedPeers << (ip + u':' + port);
    }

    QApplication::clipboard()->setText(selectedPeers.join(u'\n'));
}

void PeerListWidget::clear()
{
    m_peerItems.clear();
    m_I2PPeerItems.clear();
    m_itemsByIP.clear();
    const int nbrows = m_listModel->rowCount();
    if (nbrows > 0)
        m_listModel->removeRows(0, nbrows);
}

bool PeerListWidget::loadSettings()
{
    return header()->restoreState(Preferences::instance()->getPeerListState());
}

void PeerListWidget::saveSettings() const
{
    Preferences::instance()->setPeerListState(header()->saveState());
}

void PeerListWidget::loadPeers(const BitTorrent::Torrent *torrent)
{
    if (!torrent)
        return;

    torrent->fetchPeerInfo().then(this, [this, torrent = QPointer(torrent)](const QList<BitTorrent::PeerInfo> &peers)
    {
        if (const BitTorrent::Torrent *currentTorrent = m_properties->getCurrentTorrent();
            !currentTorrent || (currentTorrent != torrent))
        {
            return;
        }

        // Remove I2P peers since they will be completely reloaded.
        for (const QStandardItem *item : asConst(m_I2PPeerItems))
            m_listModel->removeRow(item->row());
        m_I2PPeerItems.clear();

        QSet<PeerEndpoint> existingPeers;
        existingPeers.reserve(m_peerItems.size());
        for (auto i = m_peerItems.cbegin(); i != m_peerItems.cend(); ++i)
            existingPeers.insert(i.key());

        const Preferences *pref = Preferences::instance();
        const bool hideZeroValues = (pref->getHideZeroValues() && (pref->getHideZeroComboValues() == 0));
        for (const BitTorrent::PeerInfo &peer : peers)
        {
            const PeerEndpoint peerEndpoint {peer.address(), peer.connectionType()};

            auto itemIter = m_peerItems.find(peerEndpoint);
            const bool isNewPeer = (itemIter == m_peerItems.end());
            const int row = isNewPeer ? m_listModel->rowCount() : (*itemIter)->row();
            if (isNewPeer)
            {
                m_listModel->insertRow(row);

                const bool useI2PSocket = peer.useI2PSocket();

                const QString peerIPString = useI2PSocket ? peer.I2PAddress() : peerEndpoint.address.ip.toString();
                const QVariant peerIPSortData = useI2PSocket ? QVariant(peerIPString) : QVariant::fromValue(peerEndpoint.address);
                setModelData(m_listModel, row, PeerListColumns::IP, peerIPString, peerIPSortData, {}, peerIPString);

                const QString peerIPHiddenString = useI2PSocket ? QString() : peerEndpoint.address.ip.toString();
                setModelData(m_listModel, row, PeerListColumns::IP_HIDDEN, peerIPHiddenString, peerIPHiddenString);

                const QString peerPortString = useI2PSocket ? tr("N/A") : QString::number(peer.address().port);
                setModelData(m_listModel, row, PeerListColumns::PORT, peerPortString, peer.address().port, (Qt::AlignRight | Qt::AlignVCenter));

                if (useI2PSocket)
                {
                    m_I2PPeerItems.append(m_listModel->item(row, PeerListColumns::IP));
                }
                else
                {
                    itemIter = m_peerItems.insert(peerEndpoint, m_listModel->item(row, PeerListColumns::IP));
                    m_itemsByIP[peerEndpoint.address.ip].insert(itemIter.value());
                }
            }
            else
            {
                existingPeers.remove(peerEndpoint);
            }

            updatePeer(row, torrent, peer, hideZeroValues);
        }

        // Remove peers that are gone
        for (const PeerEndpoint &peerEndpoint : asConst(existingPeers))
        {
            QStandardItem *item = m_peerItems.take(peerEndpoint);

            const auto items = m_itemsByIP.find(peerEndpoint.address.ip);
            Q_ASSERT(items != m_itemsByIP.end());
            if (items == m_itemsByIP.end()) [[unlikely]]
                continue;

            items->remove(item);
            if (items->isEmpty())
                m_itemsByIP.erase(items);

            m_listModel->removeRow(item->row());
        }

#ifdef TORRENTER_QA_HOOKS
        // QA-only deterministic peer rows for scripted screenshots.
        if (qEnvironmentVariableIsSet("TORRENTER_QA_FAKE_PEERS") && (m_listModel->rowCount() < 6))
        {
            struct FakePeer
            {
                const char *ip;
                int port;
                const char *client;
                const char *progress;
                const char *down;
                const char *up;
                const char *flags;
                const char *connection;
            };
            static const FakePeer fakePeers[] = {
                {"185.21.216.140", 51413, "qBittorrent/5.1.2", "96.1%", "1.8 MiB/s", "312 KiB/s", "DE", "TCP"},
                {"177.92.14.37", 6881, "Transmission 4.1.0", "100%", "842 KiB/s", "0 B/s", "BR", "uTP"},
                {"73.218.104.9", 51413, "libtorrent/2.0.9", "74.3%", "412 KiB/s", "18 KiB/s", "US", "TCP"},
                {"51.158.72.44", 6881, "qBittorrent/4.6.4", "58.0%", "208 KiB/s", "0 B/s", "FR", "uTP"},
                {"126.203.8.19", 6881, "BitComet/2.1", "31.6%", "96 KiB/s", "4 KiB/s", "JP", "TCP"},
                {"84.29.167.203", 51413, "Deluge 2.1.1", "12.4%", "0 B/s", "0 B/s", "NL", "TCP"}
            };
            for (const FakePeer &peer : fakePeers)
            {
                const int row = m_listModel->rowCount();
                m_listModel->insertRow(row);
                setModelData(m_listModel, row, PeerListColumns::IP, QString::fromLatin1(peer.ip), QString::fromLatin1(peer.ip));
                setModelData(m_listModel, row, PeerListColumns::PORT, QString::number(peer.port), peer.port, Qt::AlignRight | Qt::AlignVCenter);
                setModelData(m_listModel, row, PeerListColumns::CONNECTION, QString::fromLatin1(peer.connection), QString::fromLatin1(peer.connection));
                setModelData(m_listModel, row, PeerListColumns::CLIENT, QString::fromLatin1(peer.client), QString::fromLatin1(peer.client));
                setModelData(m_listModel, row, PeerListColumns::PROGRESS, QString::fromLatin1(peer.progress), QString::fromLatin1(peer.progress), Qt::AlignRight | Qt::AlignVCenter);
                setModelData(m_listModel, row, PeerListColumns::DOWN_SPEED, QString::fromLatin1(peer.down), 1, Qt::AlignRight | Qt::AlignVCenter);
                setModelData(m_listModel, row, PeerListColumns::UP_SPEED, QString::fromLatin1(peer.up), 1, Qt::AlignRight | Qt::AlignVCenter);
                setModelData(m_listModel, row, PeerListColumns::FLAGS, QString::fromLatin1(peer.flags), QString::fromLatin1(peer.flags));
                const QIcon flag = UIThemeManager::instance()->getFlagIcon(QString::fromLatin1(peer.flags));
                if (!flag.isNull())
                    m_listModel->setData(m_listModel->index(row, PeerListColumns::COUNTRY), flag, Qt::DecorationRole);
            }
        }
#endif
    });
}

void PeerListWidget::updatePeer(const int row, const BitTorrent::Torrent *torrent, const BitTorrent::PeerInfo &peer, const bool hideZeroValues)
{
    const Qt::Alignment intDataTextAlignment = Qt::AlignRight | Qt::AlignVCenter;

    const QString client = peer.client().toHtmlEscaped();
    setModelData(m_listModel, row, PeerListColumns::CLIENT, client, client, {}, client);

    const QString peerIdClient = peer.peerIdClient().toHtmlEscaped();
    setModelData(m_listModel, row, PeerListColumns::PEERID_CLIENT, peerIdClient, peerIdClient);

    const QString downSpeed = (hideZeroValues && (peer.payloadDownSpeed() <= 0))
            ? QString() : Utils::Misc::friendlyUnit(peer.payloadDownSpeed(), true);
    setModelData(m_listModel, row, PeerListColumns::DOWN_SPEED, downSpeed, peer.payloadDownSpeed(), intDataTextAlignment);

    const QString upSpeed = (hideZeroValues && (peer.payloadUpSpeed() <= 0))
            ? QString() : Utils::Misc::friendlyUnit(peer.payloadUpSpeed(), true);
    setModelData(m_listModel, row, PeerListColumns::UP_SPEED, upSpeed, peer.payloadUpSpeed(), intDataTextAlignment);

    const QString totalDown = (hideZeroValues && (peer.totalDownload() <= 0))
            ? QString() : Utils::Misc::friendlyUnit(peer.totalDownload());
    setModelData(m_listModel, row, PeerListColumns::TOT_DOWN, totalDown, peer.totalDownload(), intDataTextAlignment);

    const QString totalUp = (hideZeroValues && (peer.totalUpload() <= 0))
            ? QString() : Utils::Misc::friendlyUnit(peer.totalUpload());
    setModelData(m_listModel, row, PeerListColumns::TOT_UP, totalUp, peer.totalUpload(), intDataTextAlignment);

    setModelData(m_listModel, row, PeerListColumns::CONNECTION, peer.connectionType(), peer.connectionType());
    setModelData(m_listModel, row, PeerListColumns::FLAGS, peer.flags(), peer.flags(), {}, peer.flagsDescription());
    setModelData(m_listModel, row, PeerListColumns::PROGRESS, (Utils::String::fromDouble(peer.progress() * 100, 1) + u'%')
            , peer.progress(), intDataTextAlignment);
    setModelData(m_listModel, row, PeerListColumns::RELEVANCE, (Utils::String::fromDouble(peer.relevance() * 100, 1) + u'%')
            , peer.relevance(), intDataTextAlignment);

    const qlonglong totalUpload = peer.totalUpload();
    qreal contribution = 0;

    if (totalUpload > 0)
    {
        const qlonglong totalSize = (torrent->totalSize() <= 0) ? totalUpload : torrent->totalSize();
        const qreal progressBytes = peer.progress() * totalSize;
        contribution = static_cast<qreal>(totalUpload) / ((progressBytes <= 0) ? totalSize : progressBytes);
    }
    setModelData(m_listModel, row, PeerListColumns::CONTRIBUTION, (Utils::String::fromDouble((contribution * 100), 1) + u'%')
            , contribution, intDataTextAlignment);

    const PathList filePaths = torrent->info().filesForPiece(peer.downloadingPieceIndex());
    QStringList downloadingFiles;
    downloadingFiles.reserve(filePaths.size());
    for (const Path &filePath : filePaths)
        downloadingFiles.append(filePath.toString());

    const QString downloadingFilesDisplayValue = downloadingFiles.join(u';');
    setModelData(m_listModel, row, PeerListColumns::DOWNLOADING_PIECE, downloadingFilesDisplayValue
            , downloadingFilesDisplayValue, {}, downloadingFiles.join(u'\n'));

    if (!peer.useI2PSocket() && m_resolveHostNames)
    {
        const QHostAddress ipAddr = peer.address().ip;
        const QString hostName = Net::ReverseResolution::instance()->resolve(ipAddr);
        if (!hostName.isEmpty())
            setModelData(m_listModel, row, PeerListColumns::IP, hostName, hostName, {}, ipAddr.toString());
    }

    if (m_resolveCountries)
    {
        const QIcon icon = UIThemeManager::instance()->getFlagIcon(peer.country());
        if (!icon.isNull())
        {
            m_listModel->setData(m_listModel->index(row, PeerListColumns::COUNTRY), icon, Qt::DecorationRole);
            const QString countryName = Net::GeoIPManager::CountryName(peer.country());
            m_listModel->setData(m_listModel->index(row, PeerListColumns::COUNTRY), countryName, Qt::ToolTipRole);
        }
    }
}

int PeerListWidget::visibleColumnsCount() const
{
    int count = 0;
    for (int i = 0, iMax = header()->count(); i < iMax; ++i)
    {
        if (!isColumnHidden(i))
            ++count;
    }

    return count;
}

void PeerListWidget::handleResolved(const QHostAddress &ip, const QString &hostname) const
{
    if (hostname.isEmpty())
        return;

    const QSet<QStandardItem *> items = m_itemsByIP.value(ip);
    for (QStandardItem *item : items)
        item->setData(hostname, Qt::DisplayRole);
}

void PeerListWidget::handleSortColumnChanged(const int col)
{
    if (col == PeerListColumns::COUNTRY)
        m_proxyModel->setSortRole(Qt::ToolTipRole);
    else
        m_proxyModel->setSortRole(PeerListSortModel::UnderlyingDataRole);
}

void PeerListWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + scroll = horizontal scroll
        event->accept();
        QWheelEvent scrollHEvent {event->position(), event->globalPosition()
            , event->pixelDelta(), event->angleDelta().transposed(), event->buttons()
            , event->modifiers(), event->phase(), event->inverted(), event->source()};
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
