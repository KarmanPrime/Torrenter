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

#include "downloadfromurldialog.h"

#include <algorithm>

#include <QAbstractButton>
#include <QCoreApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "base/net/downloadmanager.h"
#include "torrentericons.h"
#include "ui_downloadfromurldialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"DownloadFromURLDialog/" name

namespace
{
    bool isDownloadable(const QString &str)
    {
        return (Net::DownloadManager::hasSupportedScheme(str)
            || str.startsWith(u"magnet:", Qt::CaseInsensitive)
#ifdef QBT_USES_LIBTORRENT2
            || ((str.size() == 64) && !str.contains(QRegularExpression(u"[^0-9A-Fa-f]"_s))) // v2 hex-encoded SHA-256 info-hash
#endif
            || ((str.size() == 40) && !str.contains(QRegularExpression(u"[^0-9A-Fa-f]"_s))) // v1 hex-encoded SHA-1 info-hash
            || ((str.size() == 32) && !str.contains(QRegularExpression(u"[^2-7A-Za-z]"_s)))); // v1 Base32 encoded SHA-1 info-hash
    }

    struct ParsedLink
    {
        QString name;
        QString type;
        QString icon;
    };

    ParsedLink describe(const QString &link)
    {
        if (link.startsWith(u"magnet:", Qt::CaseInsensitive))
        {
            QString name = QUrlQuery(QUrl(link).query()).queryItemValue(u"dn"_s);
            if (name.isEmpty())
                name = link;
            // QUrlQuery::queryItemValue() already percent-decodes dn; decoding
            // again corrupts names containing literal percent sequences.
            return {name, QCoreApplication::translate("DownloadFromURLDialog", "Magnet"), u"torrent-magnet"_s};
        }
        if (Net::DownloadManager::hasSupportedScheme(link))
        {
            QString name = QUrl(link).fileName();
            if (name.isEmpty())
                name = link;
            return {QUrl::fromPercentEncoding(name.toUtf8()), QCoreApplication::translate("DownloadFromURLDialog", "URL"), u"application-url"_s};
        }
        return {(link.size() > 14) ? (link.first(8) + u"\u2026" + link.last(6)) : link, QCoreApplication::translate("DownloadFromURLDialog", "Info hash"), u"hash"_s};
    }
}

DownloadFromURLDialog::DownloadFromURLDialog(QWidget *parent)
    : QDialog(parent, (Qt::Popup | Qt::FramelessWindowHint))
    , m_ui(new Ui::DownloadFromURLDialog)
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_s))
{
    m_ui->setupUi(this);

    // Phase 4 (mockup 04b): a compact tray anchored to the toolbar Link button.
    setFixedWidth(520);
    resize(520, 430);
    setWindowTitle(tr("Add links"));

    auto *root = qobject_cast<QVBoxLayout *>(layout());
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("linksHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 16, 18, 10);
    headerLayout->setSpacing(10);

    auto *tile = new QLabel(header);
    tile->setObjectName(QStringLiteral("linksHeaderTile"));
    tile->setFixedSize(32, 32);
    tile->setPixmap(TorrenterIcons::icon(u"torrent-magnet"_s, ThemeTokens::Token::AccentText
            , ThemeTokens::currentMode()).pixmap(QSize(18, 18)));
    headerLayout->addWidget(tile);

    auto *titles = new QVBoxLayout();
    titles->setSpacing(2);
    auto *titleLabel = new QLabel(tr("Add links"), header);
    titleLabel->setObjectName(QStringLiteral("linksHeaderTitle"));
    // Reuse the upstream help string; the mockup wording is illustrative.
    auto *helpLabel = new QLabel(m_ui->label_infos->text(), header);
    helpLabel->setObjectName(QStringLiteral("linksHeaderHelp"));
    helpLabel->setWordWrap(true);
    titles->addWidget(titleLabel);
    titles->addWidget(helpLabel);
    headerLayout->addLayout(titles, 1);
    if (root)
        root->insertWidget(0, header);
    m_ui->downloadURL_lbl->setVisible(false);
    m_ui->label_infos->setVisible(false);

    m_ui->textUrls->setFixedHeight(112);
    m_ui->textUrls->setWordWrapMode(QTextOption::NoWrap);
    m_ui->textUrls->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_parsedList = new QListWidget(this);
    m_parsedList->setObjectName(QStringLiteral("parsedLinks"));
    m_parsedList->setUniformItemSizes(true);
    m_parsedList->setSelectionMode(QAbstractItemView::NoSelection);
    m_parsedList->setFocusPolicy(Qt::NoFocus);
    m_parsedList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (root)
    {
        const int at = root->indexOf(m_ui->textUrls);
        root->insertWidget((at >= 0) ? (at + 1) : root->count(), m_parsedList);
    }

    m_linksCount = new QLabel(this);
    m_linksCount->setObjectName(QStringLiteral("linksCount"));
    if (root)
    {
        const int at = root->indexOf(m_ui->buttonBox);
        root->insertWidget((at >= 0) ? at : root->count(), m_linksCount);
    }

    // Ghost Cancel on the LEFT of the primary button. Qt 6 here has no
    // InvertedButtons option, so mirror the button box only.
    m_ui->buttonBox->setLayoutDirection(Qt::RightToLeft);
    for (QAbstractButton *button : m_ui->buttonBox->buttons())
        button->setLayoutDirection(Qt::LeftToRight);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Download"));
    const auto setVariant = [](QAbstractButton *button, const QString &variant)
    {
        button->setProperty("variant", variant);
        button->style()->unpolish(button);
        button->style()->polish(button);
    };
    setVariant(m_ui->buttonBox->button(QDialogButtonBox::Ok), QStringLiteral("primary"));
    setVariant(m_ui->buttonBox->button(QDialogButtonBox::Cancel), QStringLiteral("ghost"));
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &DownloadFromURLDialog::onSubmit);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->textUrls, &QTextEdit::textChanged, this, &DownloadFromURLDialog::updateParsedList);

    // Anchor to the toolbar Link button and keep it visually checked while open.
    if (QWidget *mainWindow = parentWidget())
    {
        const auto buttons = mainWindow->findChildren<QToolButton *>();
        for (QToolButton *button : buttons)
        {
            if (button->defaultAction() && (button->defaultAction()->objectName() == u"actionDownloadFromURL"_s))
            {
                m_anchorButton = button;
                m_anchorWasCheckable = button->isCheckable();
                m_anchorWasChecked = button->isChecked();
                break;
            }
        }
    }
    if (m_anchorButton)
    {
        m_anchorButton->setCheckable(true);
        m_anchorButton->setChecked(true);
        const QPointer<QToolButton> anchor = m_anchorButton;
        const bool wasCheckable = m_anchorWasCheckable;
        const bool wasChecked = m_anchorWasChecked;
        connect(this, &QObject::destroyed, m_anchorButton, [anchor, wasCheckable, wasChecked]
        {
            if (anchor)
            {
                anchor->setChecked(false);
                anchor->setCheckable(wasCheckable);
                anchor->setChecked(wasChecked);
            }
        });
        QTimer::singleShot(0, this, [this] { anchorToLinkButton(); });
    }

    // Paste clipboard if there is an URL in it (upstream behaviour).
    const QString clipboardText = qApp->clipboard()->text();
    const QList<QStringView> clipboardList = QStringView(clipboardText).split(u'\n');

    // show urls in the original order as from the clipboard
    QStringList urls;
    urls.reserve(clipboardList.size());

    for (QStringView url : clipboardList)
    {
        url = url.trimmed();

        if (url.isEmpty())
            continue;

        if (const QString urlString = url.toString(); isDownloadable(urlString))
            urls << urlString;
    }

    if (!urls.isEmpty())
    {
        m_ui->textUrls->setText(urls.join(u'\n') + u"\n");
        m_ui->textUrls->moveCursor(QTextCursor::End);
        m_ui->buttonBox->setFocus();
    }

#ifdef TORRENTER_QA_HOOKS
    // QA-only: deterministic links for scripted screenshots.
    if (const QByteArray qaText = qgetenv("TORRENTER_QA_LINKS_TEXT"); !qaText.isEmpty())
        m_ui->textUrls->setPlainText(QString::fromLocal8Bit(qaText.constData()));
#endif

    updateParsedList();

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

DownloadFromURLDialog::~DownloadFromURLDialog()
{
    restoreAnchorState();
    m_storeDialogSize = size();
    delete m_ui;
}

void DownloadFromURLDialog::restoreAnchorState()
{
    if (!m_anchorButton)
        return;
    m_anchorButton->setChecked(false);
    m_anchorButton->setCheckable(m_anchorWasCheckable);
    m_anchorButton->setChecked(m_anchorWasChecked);
}

void DownloadFromURLDialog::done(int result)
{
    restoreAnchorState();
    QDialog::done(result);
}

void DownloadFromURLDialog::anchorToLinkButton()
{
    if (!m_anchorButton)
        return;

    QPoint pos = m_anchorButton->mapToGlobal(QPoint(0, m_anchorButton->height()));
    pos.ry() += 8;  // 8 px below the toolbar Link button
    if (const QScreen *scr = QWidget::screen())
    {
        const QRect area = scr->availableGeometry();
        pos.setX(std::clamp(pos.x(), area.left(), std::max(area.left(), area.right() - width())));
        pos.setY(std::clamp(pos.y(), area.top(), std::max(area.top(), area.bottom() - height())));
    }
    move(pos);
}

void DownloadFromURLDialog::updateParsedList()
{
    if (!m_parsedList)
        return;

    m_parsedList->clear();
    QSet<QString> seen;
    int count = 0;
    const QStringList lines = m_ui->textUrls->toPlainText().split(u'\n');
    for (const QString &line : lines)
    {
        const QString link = line.trimmed();
        if (link.isEmpty() || !isDownloadable(link) || seen.contains(link))
            continue;
        seen.insert(link);

        const ParsedLink parsed = describe(link);
        auto *item = new QListWidgetItem(m_parsedList);
        item->setSizeHint(QSize(480, 30));

        auto *row = new QWidget(m_parsedList);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(6, 0, 6, 0);
        rowLayout->setSpacing(10);

        auto *iconLabel = new QLabel(row);
        iconLabel->setFixedSize(16, 16);
        iconLabel->setPixmap(TorrenterIcons::icon(parsed.icon, ThemeTokens::Token::AccentText
                , ThemeTokens::currentMode()).pixmap(QSize(16, 16)));
        auto *nameLabel = new QLabel(parsed.name, row);
        nameLabel->setObjectName(QStringLiteral("parsedName"));
        nameLabel->setMinimumWidth(0);
        nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        nameLabel->setToolTip(parsed.name);
        auto *typeLabel = new QLabel(parsed.type, row);
        typeLabel->setObjectName(QStringLiteral("parsedType"));
        rowLayout->addWidget(iconLabel);
        rowLayout->addWidget(nameLabel, 1);
        rowLayout->addWidget(typeLabel);
        m_parsedList->setItemWidget(item, row);
        ++count;
    }

    m_linksCount->setText(tr("%1 links found").arg(count));
    // Translation-safe dynamic primary label; "Download" when nothing parsed.
    if (auto *ok = m_ui->buttonBox->button(QDialogButtonBox::Ok))
    {
        ok->setText((count == 1) ? tr("Add 1 torrent")
                                 : ((count > 1) ? tr("Add %1 torrents").arg(count) : tr("Download")));
    }
    m_parsedList->setFixedHeight((std::min(count, 8) * 30) + 6);
    const int contentHeight = 276 + (std::min(count, 8) * 30);
    // Grow only when the current/saved height cannot fit the parsed rows.
    // Never force a fixed height here: it would override the saved dialog size.
    const QSize savedSize = m_storeDialogSize;
    if (!savedSize.isValid() || (height() < contentHeight))
        resize(width(), contentHeight);
}

void DownloadFromURLDialog::onSubmit()
{
    const QString plainText = m_ui->textUrls->toPlainText();
    const QList<QStringView> urls = QStringView(plainText).split(u'\n');

    // process urls in the original order as from the widget
    QSet<QStringView> uniqueURLs;
    QStringList urlList;
    urlList.reserve(urls.size());

    for (QStringView url : urls)
    {
        url = url.trimmed();

        if (url.isEmpty())
            continue;

        if (!uniqueURLs.contains(url))
        {
            uniqueURLs.insert(url);
            urlList << url.toString();
        }
    }

    if (urlList.isEmpty())
    {
        QMessageBox::warning(this, tr("No URL entered"), tr("Please type at least one URL."));
        return;
    }

    emit urlsReadyToBeDownloaded(urlList);
    accept();
}

void DownloadFromURLDialog::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() == Qt::ControlModifier) && (event->key() == Qt::Key_Return))
    {
        onSubmit();
        return;
    }

    QDialog::keyPressEvent(event);
}
