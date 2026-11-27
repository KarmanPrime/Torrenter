/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024-2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "deletionconfirmationdialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QStyle>
#include <QVBoxLayout>

#include "torrentericons.h"
#include "themetokens.h"
#include "widgets/toggleswitch.h"
#include "windowsthemehelper.h"

#include "base/bittorrent/session.h"
#include "base/preferences.h"
#include "uithememanager.h"
#include "utils.h"

using namespace Qt::Literals::StringLiterals;

DeletionConfirmationDialog::DeletionConfirmationDialog(QWidget *parent, const int torrentsCount, const QString &name, const bool defaultDeleteFiles)
    : QDialog(parent)
    , m_ui {new Ui::DeletionConfirmationDialog}
{
    m_ui->setupUi(this);

    // Phase B / §5.10: 440 px dialog, rounded tile and inset error card.
    setFixedWidth(440);
    winId();
#ifdef Q_OS_WIN
    WindowsThemeHelper::apply(this);
#endif
    m_ui->labelWarning->setObjectName(QStringLiteral("deleteTile"));
    m_ui->labelWarning->setFixedSize(36, 36);
    m_ui->labelWarning->setAlignment(Qt::AlignCenter);
    if (auto *top = findChild<QHBoxLayout *>(QStringLiteral("horizontalLayout")))
    {
        top->removeWidget(m_ui->label);
        auto *texts = new QVBoxLayout;
        texts->setSpacing(4);
        texts->addWidget(m_ui->label);
        auto *help = new QLabel(tr("They'll disappear from the list. Downloaded data stays on disk unless you choose otherwise."), this);
        help->setObjectName(QStringLiteral("deleteHelp"));
        help->setWordWrap(true);
        texts->addWidget(help);
        top->addLayout(texts, 1);
    }
    if (auto *row = findChild<QHBoxLayout *>(QStringLiteral("horizontalLayout_2")))
    {
        row->removeWidget(m_ui->checkRemoveContent);
        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("deleteFilesCard"));
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(10);
        auto *deleteLabel = new QLabel(tr("Also delete files"), card);
        deleteLabel->setObjectName(QStringLiteral("deleteFilesLabel"));
        m_ui->checkRemoveContent->setText({});
        cardLayout->addWidget(deleteLabel, 1);
        cardLayout->addWidget(m_ui->checkRemoveContent);
        row->insertWidget(0, card);
    }

    if (torrentsCount == 1)
        m_ui->label->setText(tr("Are you sure you want to remove '%1' from the transfer list?", "Are you sure you want to remove 'ubuntu-linux-iso' from the transfer list?").arg(name.toHtmlEscaped()));
    else
        m_ui->label->setText(tr("Are you sure you want to remove these %1 torrents from the transfer list?", "Are you sure you want to remove these 5 torrents from the transfer list?").arg(QString::number(torrentsCount)));

    // Icons
    m_ui->labelWarning->setPixmap(TorrenterIcons::icon(u"list-remove"_s, ThemeTokens::Token::StError
            , ThemeTokens::currentMode()).pixmap(QSize(20, 20)));
    m_ui->rememberBtn->setIcon(UIThemeManager::instance()->getIcon(u"object-locked"_s));
    m_ui->rememberBtn->setIconSize(Utils::Gui::mediumIconSize());
    connect(m_ui->rememberBtn, &QCheckBox::clicked, this, [this]
    {
        Preferences::instance()->setRemoveTorrentContent(m_ui->checkRemoveContent->isChecked());
        m_ui->rememberBtn->setEnabled(false);
    });

    const bool removeTorrentContent = defaultDeleteFiles || Preferences::instance()->removeTorrentContent();
    const auto applyDeleteState = [this]()
    {
        const bool del = m_ui->checkRemoveContent->isChecked();
        auto *ok = m_ui->buttonBox->button(QDialogButtonBox::Ok);
        ok->setText(del ? tr("Remove torrent and content") : tr("Remove torrent"));
        ok->setProperty("variant", del ? QStringLiteral("danger") : QStringLiteral("primary"));
        ok->style()->unpolish(ok);
        ok->style()->polish(ok);
        m_ui->rememberBtn->setEnabled(del != Preferences::instance()->removeTorrentContent());
    };
    connect(m_ui->checkRemoveContent, &QCheckBox::toggled, this, applyDeleteState);
    m_ui->checkRemoveContent->setChecked(removeTorrentContent);
    applyDeleteState();
#ifdef TORRENTER_QA_HOOKS
    // QA-only: deterministic destructive state for scripted screenshots.
    if (qEnvironmentVariableIsSet("TORRENTER_QA_DELETE_FILES") && !removeTorrentContent)
        m_ui->checkRemoveContent->setChecked(true);
#endif
    auto *okButton = m_ui->buttonBox->button(QDialogButtonBox::Ok);
    auto *cancelButton = m_ui->buttonBox->button(QDialogButtonBox::Cancel);
    okButton->setAutoDefault(false);
    cancelButton->setAutoDefault(false);
    cancelButton->setProperty("variant", QStringLiteral("ghost"));
    cancelButton->style()->unpolish(cancelButton);
    cancelButton->style()->polish(cancelButton);
    cancelButton->setFocus();

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

DeletionConfirmationDialog::~DeletionConfirmationDialog()
{
    delete m_ui;
}

bool DeletionConfirmationDialog::isRemoveContentSelected() const
{
    return m_ui->checkRemoveContent->isChecked();
}
