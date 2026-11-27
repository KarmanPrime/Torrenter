/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "statusbar.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPalette>
#include <QPushButton>
#include <QStyle>

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "speedlimitdialog.h"
#include "themetokens.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
    QString cssColor(const QColor &color, const int alpha = -1)
    {
        QColor result = color;
        if (alpha >= 0)
            result.setAlpha(alpha);
        return QStringLiteral("rgba(%1,%2,%3,%4)")
                .arg(result.red()).arg(result.green()).arg(result.blue()).arg(result.alpha());
    }

    void applyTabularFont(QWidget *widget, const int pixelSize, const int weight)
    {
        QFont font = widget->font();
        font.setPixelSize(pixelSize);
        font.setWeight(static_cast<QFont::Weight>(weight));
        font.setFeature("tnum", 1);  // OpenType tabular numbers
        widget->setFont(font);
    }

    QLabel *createStatusLabel(QWidget *parent, const QColor &color, const QString &text = {})
    {
        auto *label = new QLabel(text, parent);
        applyTabularFont(label, 12, QFont::Normal);
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, color);
        label->setPalette(pal);
        // Colors are set in code (palette + generated stylesheet), not in QSS alone.
        label->setStyleSheet(QStringLiteral("QLabel { color: %1; padding: 0; background: transparent; }")
            .arg(cssColor(color)));
        label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        return label;
    }

    QPushButton *createSpeedButton(QWidget *parent, const QColor &color)
    {
        auto *button = new QPushButton(parent);
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        applyTabularFont(button, 12, QFont::Medium);
        QPalette pal = button->palette();
        pal.setColor(QPalette::ButtonText, color);
        button->setPalette(pal);
        button->setStyleSheet(QStringLiteral("QPushButton { color: %1; background: transparent; border: none; padding: 0; min-height: 0; max-height: 22px; }")
            .arg(cssColor(color)));
        return button;
    }
}

ConnectionLed::ConnectionLed(QWidget *parent)
    : QAbstractButton(parent)
{
    setFixedSize(13, 13);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
}

void ConnectionLed::setStatusColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    update();
}

void ConnectionLed::paintEvent(QPaintEvent *)
{
    if (!m_color.isValid())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    // 3 px halo at 15% opacity, then the 7 px dot.
    QColor halo = m_color;
    halo.setAlpha(38);
    painter.setBrush(halo);
    painter.drawEllipse(rect().adjusted(0, 0, -1, -1));
    painter.setBrush(m_color);
    painter.drawEllipse(rect().adjusted(3, 3, -4, -4));
}

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar(parent)
{
    const auto mode = ThemeTokens::currentMode();
    const QColor chrome = ThemeTokens::color(ThemeTokens::Token::Chrome, mode);
    const QColor lineSubtle = ThemeTokens::color(ThemeTokens::Token::LineSubtle, mode);
    const QColor text3 = ThemeTokens::color(ThemeTokens::Token::Text3, mode);

    // QA-v6: 30 px tall, chrome background, hairline top border, no item borders.
    setMinimumHeight(30);
    setMaximumHeight(30);
    setStyleSheet(QStringLiteral("QStatusBar { background: %1; border-top: 1px solid %2; min-height: 30px; max-height: 30px; }"
        " QStatusBar::item { border: none; margin: 0; padding: 0; }")
        .arg(cssColor(chrome), cssColor(lineSubtle)));
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, [this]()
    {
        const auto mode = ThemeTokens::currentMode();
        setStyleSheet(QStringLiteral("QStatusBar { background: %1; border-top: 1px solid %2; min-height: 30px; max-height: 30px; }"
            " QStatusBar::item { border: none; margin: 0; padding: 0; }")
            .arg(cssColor(ThemeTokens::color(ThemeTokens::Token::Chrome, mode))
                , cssColor(ThemeTokens::color(ThemeTokens::Token::LineSubtle, mode))));
    });

    // One stretched permanent widget gives us 14 px side padding and 16 px gaps
    // with the left/right cluster layout fully under our control.
    m_content = new QWidget(this);
    auto *contentLayout = new QHBoxLayout(m_content);
    contentLayout->setContentsMargins(14, 0, 14, 0);
    contentLayout->setSpacing(16);

    m_leftCluster = new QWidget(m_content);
    auto *leftLayout = new QHBoxLayout(m_leftCluster);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(16);

    m_rightCluster = new QWidget(m_content);
    auto *rightLayout = new QHBoxLayout(m_rightCluster);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(16);

    contentLayout->addWidget(m_leftCluster, 0, Qt::AlignLeft);
    contentLayout->addStretch(1);
    contentLayout->addWidget(m_rightCluster, 0, Qt::AlignRight);

    m_connectionLed = new ConnectionLed(m_leftCluster);
    connect(m_connectionLed, &QAbstractButton::clicked, this, &StatusBar::connectionButtonClicked);
    leftLayout->addWidget(m_connectionLed, 0, Qt::AlignVCenter);

    m_connectionLbl = createStatusLabel(m_leftCluster, text3, tr("Connected"));
    leftLayout->addWidget(m_connectionLbl, 0, Qt::AlignVCenter);

    m_DHTLbl = createStatusLabel(m_leftCluster, text3, tr("DHT: %1 nodes").arg(0));
    leftLayout->addWidget(m_DHTLbl, 0, Qt::AlignVCenter);

    m_freeDiskSpaceLbl = createStatusLabel(m_leftCluster, text3, tr("Free space: N/A"));
    leftLayout->addWidget(m_freeDiskSpaceLbl, 0, Qt::AlignVCenter);

    m_lastExternalIPsLbl = createStatusLabel(m_leftCluster, text3, tr("External IP: N/A"));
    leftLayout->addWidget(m_lastExternalIPsLbl, 0, Qt::AlignVCenter);

    // QA-v6 explicit ink values: mint down, violet up; session totals stay text3.
    m_dlSpeedLbl = createSpeedButton(m_rightCluster, QColor(QStringLiteral("#45E3B8")));
    connect(m_dlSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);
    m_dlTotalLbl = createStatusLabel(m_rightCluster, text3);

    m_upSpeedLbl = createSpeedButton(m_rightCluster, QColor(QStringLiteral("#9D8CFF")));
    connect(m_upSpeedLbl, &QAbstractButton::clicked, this, &StatusBar::capSpeed);
    m_upTotalLbl = createStatusLabel(m_rightCluster, text3);

    m_altSpeedsBtn = new QPushButton(m_rightCluster);
    m_altSpeedsBtn->setCursor(Qt::PointingHandCursor);
    m_altSpeedsBtn->setFlat(true);
    m_altSpeedsBtn->setFocusPolicy(Qt::NoFocus);
    m_altSpeedsBtn->setFixedSize(20, 20);
    m_altSpeedsBtn->setIconSize(QSize(14, 14));
    m_altSpeedsBtn->setStyleSheet(QStringLiteral("QPushButton { background: transparent; border: none; padding: 0; }"));
    connect(m_altSpeedsBtn, &QAbstractButton::clicked, this, &StatusBar::alternativeSpeedsButtonClicked);

    rightLayout->addWidget(m_dlSpeedLbl, 0, Qt::AlignVCenter);
    rightLayout->addWidget(m_dlTotalLbl, 0, Qt::AlignVCenter);
    rightLayout->addWidget(m_upSpeedLbl, 0, Qt::AlignVCenter);
    rightLayout->addWidget(m_upTotalLbl, 0, Qt::AlignVCenter);
    rightLayout->addWidget(m_altSpeedsBtn, 0, Qt::AlignVCenter);

    addPermanentWidget(m_content, 1);

    const auto *session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::speedLimitModeChanged, this, &StatusBar::updateAltSpeedsBtn);
    connect(session, &BitTorrent::Session::statsUpdated, this, &StatusBar::refresh);
    connect(session, &BitTorrent::Session::freeDiskSpaceChecked, this, &StatusBar::updateFreeDiskSpaceLabel);
    connect(Preferences::instance(), &Preferences::changed, this, &StatusBar::optionsSaved);

    updateAltSpeedsBtn(session->isAltGlobalSpeedLimitEnabled());
    updateDHTNodesNumber();
    updateExternalAddressesVisibility();
    updateFreeDiskSpaceVisibility();
    updateFreeDiskSpaceLabel(session->freeDiskSpace());
}

void StatusBar::showRestartRequired()
{
    // Restart required notification
    const QString restartText = tr("Torrenter needs to be restarted!");

    const QPixmap pixmap = style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(Utils::Gui::smallIconSize());
    auto *restartIconLbl = new QLabel(this);
    restartIconLbl->setPixmap(pixmap);
    restartIconLbl->setToolTip(restartText);
    insertWidget(0, restartIconLbl);

    auto *restartLbl = new QLabel(this);
    restartLbl->setText(restartText);
    insertWidget(1, restartLbl);
}

void StatusBar::updateConnectionStatus()
{
    const BitTorrent::Session *session = BitTorrent::Session::instance();
    const BitTorrent::SessionStatus &sessionStatus = session->status();
    const auto mode = ThemeTokens::currentMode();

    if (!session->isListening())
    {
        m_connectionLed->setStatusColor(QColor(QStringLiteral("#FF6B6B")));
        m_connectionLbl->setText(tr("Disconnected"));
        m_connectionLbl->setToolTip(u"<b>%1</b><br>%2"_s.arg(tr("Connection Status:")
            , tr("Offline. This usually means that Torrenter failed to listen on the selected port for incoming connections.")));
    }
    else if (sessionStatus.hasIncomingConnections)
    {
        m_connectionLed->setStatusColor(QColor(QStringLiteral("#45E3B8")));
        m_connectionLbl->setText(tr("Connected"));
        m_connectionLbl->setToolTip(u"<b>%1</b><br>%2"_s.arg(tr("Connection Status:"), tr("Online")));
    }
    else
    {
        m_connectionLed->setStatusColor(ThemeTokens::color(ThemeTokens::Token::StStalled, mode));
        m_connectionLbl->setText(tr("Firewalled"));
        m_connectionLbl->setToolTip(u"<b>%1</b><br><i>%2</i>"_s.arg(tr("Connection Status:")
            , tr("No direct connections. This may indicate network configuration problems.")));
    }
}

void StatusBar::updateDHTNodesNumber()
{
    const bool visible = BitTorrent::Session::instance()->isDHTEnabled();
    m_DHTLbl->setVisible(visible);
    if (visible)
        m_DHTLbl->setText(tr("DHT: %1 nodes").arg(BitTorrent::Session::instance()->status().dhtNodes));
}

void StatusBar::updateFreeDiskSpaceLabel(const qint64 value)
{
    m_freeDiskSpaceLbl->setText(tr("Free space: ") + Utils::Misc::friendlyUnit(value));
}

void StatusBar::updateFreeDiskSpaceVisibility()
{
    m_freeDiskSpaceLbl->setVisible(Preferences::instance()->isStatusbarFreeDiskSpaceDisplayed());
}

void StatusBar::updateExternalAddressesLabel()
{
    const QString lastExternalIPv4Address = BitTorrent::Session::instance()->lastExternalIPv4Address();
    const QString lastExternalIPv6Address = BitTorrent::Session::instance()->lastExternalIPv6Address();
    QString addressText = tr("External IP: N/A");

    const bool hasIPv4Address = !lastExternalIPv4Address.isEmpty();
    const bool hasIPv6Address = !lastExternalIPv6Address.isEmpty();

    if (hasIPv4Address && hasIPv6Address)
        addressText = tr("External IPs: %1, %2").arg(lastExternalIPv4Address, lastExternalIPv6Address);
    else if (hasIPv4Address || hasIPv6Address)
        addressText = tr("External IP: %1%2").arg(lastExternalIPv4Address, lastExternalIPv6Address);

    m_lastExternalIPsLbl->setText(addressText);
}

void StatusBar::updateExternalAddressesVisibility()
{
    m_lastExternalIPsLbl->setVisible(Preferences::instance()->isStatusbarExternalIPDisplayed());
}

void StatusBar::updateSpeedLabels()
{
    const BitTorrent::SessionStatus &sessionStatus = BitTorrent::Session::instance()->status();

    QString dlSpeed = Utils::Misc::friendlyUnit(sessionStatus.payloadDownloadRate, true);
    const int dlSpeedLimit = BitTorrent::Session::instance()->downloadSpeedLimit();
    if (dlSpeedLimit > 0)
        dlSpeed += u" [" + Utils::Misc::friendlyUnit(dlSpeedLimit, true) + u']';
    m_dlSpeedLbl->setText(tr("↓ %1").arg(dlSpeed));
    m_dlTotalLbl->setText(Utils::Misc::friendlyUnit(sessionStatus.totalPayloadDownload));

    QString upSpeed = Utils::Misc::friendlyUnit(sessionStatus.payloadUploadRate, true);
    const int upSpeedLimit = BitTorrent::Session::instance()->uploadSpeedLimit();
    if (upSpeedLimit > 0)
        upSpeed += u" [" + Utils::Misc::friendlyUnit(upSpeedLimit, true) + u']';
    m_upSpeedLbl->setText(tr("↑ %1").arg(upSpeed));
    m_upTotalLbl->setText(Utils::Misc::friendlyUnit(sessionStatus.totalPayloadUpload));
}

void StatusBar::refresh()
{
    updateConnectionStatus();
    updateDHTNodesNumber();
    updateExternalAddressesLabel();
    updateSpeedLabels();
}

void StatusBar::updateAltSpeedsBtn(const bool alternative)
{
    m_altSpeedsBtn->setIcon(UIThemeManager::instance()->getIcon(alternative ? u"slow"_s : u"slow_off"_s));
    m_altSpeedsBtn->setToolTip(alternative
        ? tr("Click to switch to regular speed limits")
        : tr("Click to switch to alternative speed limits"));
    m_altSpeedsBtn->setDown(alternative);
    refresh();
}

void StatusBar::capSpeed()
{
    auto *dialog = new SpeedLimitDialog {parentWidget()};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void StatusBar::optionsSaved()
{
    updateFreeDiskSpaceVisibility();
    updateExternalAddressesVisibility();
}
