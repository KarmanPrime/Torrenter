/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "addnewtorrentdialog.h"

#include <algorithm>
#include <functional>

#include <QtVersionChecks>
#include <QAction>
#include <QAbstractButton>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QFuture>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/lttypecast.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentcontenthandler.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/torrentfileguard.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "filterpatternformatmenu.h"
#include "lineedit.h"
#include "torrentericons.h"
#include "torrentsharelimitswidget.h"
#include "torrenttagsdialog.h"
#include "widgets/segmentedcontrol.h"
#include "widgets/toggleswitch.h"

#ifdef Q_OS_WIN
#include "windowsthemehelper.h"
#endif

#include "ui_addnewtorrentdialog.h"

namespace
{
#define SETTINGS_KEY(name) u"AddNewTorrentDialog/" name
    const QString KEY_SAVEPATHHISTORY = SETTINGS_KEY(u"SavePathHistory"_s);
    const QString KEY_DOWNLOADPATHHISTORY = SETTINGS_KEY(u"DownloadPathHistory"_s);

    // just a shortcut
    inline SettingsStorage *settings()
    {
        return SettingsStorage::instance();
    }

    // savePath is a folder, not an absolute file path
    int indexOfPath(const FileSystemPathComboEdit *fsPathEdit, const Path &savePath)
    {
        for (qsizetype i = 0; i < fsPathEdit->count(); ++i)
        {
            if (fsPathEdit->item(i) == savePath)
                return i;
        }
        return -1;
    }

    void setPath(FileSystemPathComboEdit *fsPathEdit, const Path &newPath)
    {
        int existingIndex = indexOfPath(fsPathEdit, newPath);
        if (existingIndex < 0)
        {
            // New path, prepend to combo box
            fsPathEdit->insertItem(0, newPath);
            existingIndex = 0;
        }

        fsPathEdit->setCurrentIndex(existingIndex);
    }

    void updatePathHistory(const QString &settingsKey, const Path &path, const qsizetype maxLength)
    {
        // Add last used save path to the front of history

        auto pathList = settings()->loadValue<QStringList>(settingsKey);

        const qsizetype selectedSavePathIndex = pathList.indexOf(path.toString());
        if (selectedSavePathIndex > -1)
            pathList.move(selectedSavePathIndex, 0);
        else
            pathList.prepend(path.toString());

        if (pathList.size() > maxLength)
            pathList.resize(maxLength);

        settings()->storeValue(settingsKey, pathList);
    }
}

class AddNewTorrentDialog::TorrentContentAdaptor final : public BitTorrent::TorrentContentHandler
{
public:
    TorrentContentAdaptor(const BitTorrent::TorrentInfo &torrentInfo, PathList &filePaths
            , QList<BitTorrent::DownloadPriority> &filePriorities, std::function<void ()> onFilePrioritiesChanged);

    bool hasMetadata() const override;
    int filesCount() const override;
    qlonglong fileSize(int index) const override;
    Path filePath(int index) const override;
    PathList filePaths() const;
    QList<BitTorrent::DownloadPriority> filePriorities() const override;
    QList<qreal> filesProgress() const override;
    QFuture<QList<qreal>> fetchAvailableFileFractions() const override;
    Path actualStorageLocation() const override;
    Path actualFilePath(int fileIndex) const override;
    void flushCache() const override;

    void renameFile(int index, const Path &newFilePath) override;
    void applyContentLayout(const BitTorrent::TorrentContentLayout contentLayout);
    void prioritizeFiles(const QList<BitTorrent::DownloadPriority> &priorities) override;

private:
    void doRenameFolder(const Path &oldFolderPath, const Path &newFolderPath) override;
    void doRenameFile(int index, const Path &newFilePath);

    const BitTorrent::TorrentInfo &m_torrentInfo;
    PathList &m_filePaths;
    QList<BitTorrent::DownloadPriority> &m_filePriorities;
    std::function<void ()> m_onFilePrioritiesChanged;
    Path m_originalRootFolder;
    BitTorrent::TorrentContentLayout m_currentContentLayout;
};

struct AddNewTorrentDialog::Context
{
    BitTorrent::TorrentDescriptor torrentDescr;
    BitTorrent::AddTorrentParams torrentParams;
};

AddNewTorrentDialog::AddNewTorrentDialog(const BitTorrent::TorrentDescriptor &torrentDescr
        , const BitTorrent::AddTorrentParams &inParams, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::AddNewTorrentDialog}
    , m_filterLine {new LineEdit(this)}
    , m_storeDialogSize {SETTINGS_KEY(u"DialogSize"_s)}
    , m_storeDefaultCategory {SETTINGS_KEY(u"DefaultCategory"_s)}
    , m_storeRememberLastSavePath {SETTINGS_KEY(u"RememberLastSavePath"_s)}
    , m_storeTreeHeaderState {u"GUI/Qt6/" SETTINGS_KEY(u"TreeHeaderState"_s)}
    , m_storeSplitterState {u"GUI/Qt6/" SETTINGS_KEY(u"SplitterState"_s)}
    , m_storeFilterPatternFormat {u"GUI/" SETTINGS_KEY(u"FilterPatternFormat"_s)}
{
    m_ui->setupUi(this);

    // Phase 4 (mockup 04): token-driven chrome, native frame retained.
    setWindowTitle(tr("Add torrent"));
    setMinimumSize(760, 560);
    resize(940, 680);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("addHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 16, 22, 12);
    headerLayout->setSpacing(12);

    auto *tile = new QLabel(header);
    tile->setObjectName(QStringLiteral("addHeaderTile"));
    tile->setFixedSize(40, 40);
    const bool headerHasMetadata = torrentDescr.info().has_value();
    tile->setPixmap(TorrenterIcons::icon(headerHasMetadata ? QStringLiteral("file-down") : QStringLiteral("magnet")
            , ThemeTokens::Token::AccentText, ThemeTokens::currentMode()).pixmap(QSize(20, 20)));
    headerLayout->addWidget(tile);

    auto *titles = new QVBoxLayout();
    titles->setSpacing(3);
    m_addHeaderName = new QLabel(header);
    m_addHeaderName->setObjectName(QStringLiteral("addHeaderName"));
    m_addHeaderMeta = new QLabel(header);
    m_addHeaderMeta->setObjectName(QStringLiteral("addHeaderMeta"));
    titles->addWidget(m_addHeaderName);
    titles->addWidget(m_addHeaderMeta);
    headerLayout->addLayout(titles, 1);

    auto *closeButton = new QToolButton(header);
    closeButton->setObjectName(QStringLiteral("addHeaderClose"));
    closeButton->setIcon(TorrenterIcons::icon(QStringLiteral("x"), ThemeTokens::Token::Text2
            , ThemeTokens::currentMode()));
    closeButton->setIconSize(QSize(16, 16));
    closeButton->setToolTip(tr("Close"));
    closeButton->setAccessibleName(tr("Close"));
    closeButton->setAccessibleDescription(tr("Cancel the add torrent dialog"));
    connect(closeButton, &QToolButton::clicked, this, &QDialog::reject);
    headerLayout->addWidget(closeButton);
    if (auto *rootLayout = qobject_cast<QVBoxLayout *>(layout()))
        rootLayout->insertWidget(0, header);

    // Four-pixel free-space indicator: the fill width is updated from the
    // real free-space / torrent-size values in updateDiskSpaceLabel().
    if (auto *bar = findChild<QFrame *>(QStringLiteral("freeSpaceBar")))
    {
        auto *fill = new QFrame(bar);
        fill->setObjectName(QStringLiteral("freeSpaceFill"));
        fill->setGeometry(0, 0, 0, 4);
    }

    // "Remember last used save path" lives in the dialog footer (04 mockup).

    const auto setVariant = [](QAbstractButton *button, const QString &variant)
    {
        button->setProperty("variant", variant);
        button->style()->unpolish(button);
        button->style()->polish(button);
    };
    if (QPushButton *ok = m_ui->buttonBox->button(QDialogButtonBox::Ok))
        setVariant(ok, QStringLiteral("primary"));
    if (QPushButton *cancel = m_ui->buttonBox->button(QDialogButtonBox::Cancel))
        setVariant(cancel, QStringLiteral("ghost"));
    setVariant(m_ui->buttonSave, QStringLiteral("ghost"));
    setVariant(m_ui->buttonSelectAll, QStringLiteral("ghost"));
    setVariant(m_ui->buttonSelectNone, QStringLiteral("ghost"));
    m_filterLine->setObjectName(QStringLiteral("contentFilter"));

    // The incomplete-download path is revealed only while the ToggleSwitch is
    // checked. The upstream QGroupBox stays the source of truth for semantics.
    connect(m_ui->useDownloadPathToggle, &QAbstractButton::toggled
            , m_ui->groupBoxDownloadPath, &QGroupBox::setChecked);
    connect(m_ui->groupBoxDownloadPath, &QGroupBox::toggled, this, [this](const bool checked)
    {
        m_ui->groupBoxDownloadPath->setVisible(checked);
        if (m_ui->useDownloadPathToggle->isChecked() != checked)
            m_ui->useDownloadPathToggle->setChecked(checked);
    });
    m_ui->useDownloadPathToggle->setAccessibleName(tr("Use another path for incomplete torrent"));
    m_ui->useDownloadPathToggle->setAccessibleDescription(tr("Show a separate save path for incomplete files"));
    m_ui->groupBoxDownloadPath->setVisible(m_ui->groupBoxDownloadPath->isChecked());

    // Torrent management: the upstream combo remains authoritative; the
    // two-option SegmentedControl is the visible control (04 mockup).
    auto *tmmSegmented = new SegmentedControl(this);
    tmmSegmented->setObjectName(QStringLiteral("tmmSegmented"));
    tmmSegmented->setAccessibleName(tr("Torrent management"));
    tmmSegmented->setAccessibleDescription(tr("Choose manual or automatic torrent management"));
    tmmSegmented->addSegment(tr("Manual"), 0);
    tmmSegmented->addSegment(tr("Automatic"), 1);
    if (auto *tmmRow = findChild<QHBoxLayout *>(QStringLiteral("tmmRow")))
        tmmRow->addWidget(tmmSegmented, 1);
    connect(m_ui->comboTMM, &QComboBox::currentIndexChanged, tmmSegmented, &SegmentedControl::setCurrentIndex);
    connect(tmmSegmented, &SegmentedControl::currentIndexChanged, m_ui->comboTMM, &QComboBox::setCurrentIndex);

    // Content layout: three-option SegmentedControl, synced with the combo.
    auto *layoutSegmented = new SegmentedControl(this);
    layoutSegmented->setObjectName(QStringLiteral("contentLayoutSegmented"));
    layoutSegmented->setAccessibleName(tr("Content layout"));
    layoutSegmented->setAccessibleDescription(tr("Choose how the torrent content is placed on disk"));
    layoutSegmented->addSegment(tr("Original"), 0);
    layoutSegmented->addSegment(tr("Create subfolder"), 1);
    layoutSegmented->addSegment(tr("No subfolder"), 2);
    if (auto *layoutRow = findChild<QHBoxLayout *>(QStringLiteral("contentLayoutRow")))
        layoutRow->addWidget(layoutSegmented, 1);
    connect(m_ui->contentLayoutComboBox, &QComboBox::currentIndexChanged, layoutSegmented, &SegmentedControl::setCurrentIndex);
    connect(layoutSegmented, &SegmentedControl::currentIndexChanged, m_ui->contentLayoutComboBox, &QComboBox::setCurrentIndex);

    // Secondary controls live in a disclosure so the primary rows never clip.
    if (auto *moreButton = findChild<QToolButton *>(QStringLiteral("moreOptionsButton")))
    {
        if (auto *moreFrame = findChild<QFrame *>(QStringLiteral("moreOptionsFrame")))
        {
            connect(moreButton, &QToolButton::toggled, moreFrame, &QFrame::setVisible);
            moreButton->setAccessibleName(tr("More options"));
            moreButton->setAccessibleDescription(tr("Show category defaults, torrent information and save options"));
        }
    }

    m_ui->splitter->setSizes({400, 540});
    m_ui->contentTreeView->setCompactHeaders(true);
    m_ui->tagsLineEdit->setPlaceholderText(tr("Add tags..."));

    m_ui->scrollArea->viewport()->setAutoFillBackground(false);
    m_ui->scrollAreaWidgetContents->setAutoFillBackground(false);

    m_ui->savePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->savePath->setDialogCaption(tr("Choose save path"));
    m_ui->savePath->setMaxVisibleItems(20);

    m_ui->downloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->downloadPath->setDialogCaption(tr("Choose save path"));
    m_ui->downloadPath->setMaxVisibleItems(20);

    m_ui->stopConditionComboBox->setToolTip(
            u"<html><body><p><b>" + tr("None") + u"</b> - " + tr("No stop condition is set.") + u"</p><p><b>"
            + tr("Metadata received") + u"</b> - " + tr("Torrent will stop after metadata is received.")
            + u" <em>" + tr("Torrents that have metadata initially will be added as stopped.") + u"</em></p><p><b>"
            + tr("Files checked") + u"</b> - " + tr("Torrent will stop after files are initially checked.")
            + u" <em>" + tr("This will also download metadata if it wasn't there initially.") + u"</em></p></body></html>");
    m_ui->stopConditionComboBox->addItem(tr("None"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::None));
    m_ui->stopConditionComboBox->addItem(tr("Files checked"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::FilesChecked));

    // Share limits live in the More options disclosure; they map to the
    // same AddTorrentParams fields used by the default-options widget.
    {
        const auto *session = BitTorrent::Session::instance();
        m_ui->torrentShareLimitsWidget->setDefaults(
                inParams.category.isEmpty() ? TorrentShareLimitsWidget::UsedDefaults::Global
                        : TorrentShareLimitsWidget::UsedDefaults::Category
                , session->categoryShareLimits(inParams.category));
        m_ui->torrentShareLimitsWidget->setRatioLimit(inParams.shareLimits.ratioLimit);
        m_ui->torrentShareLimitsWidget->setSeedingTimeLimit(inParams.shareLimits.seedingTimeLimit);
        m_ui->torrentShareLimitsWidget->setInactiveSeedingTimeLimit(inParams.shareLimits.inactiveSeedingTimeLimit);
        m_ui->torrentShareLimitsWidget->setShareLimitsMode(inParams.shareLimits.mode);
        m_ui->torrentShareLimitsWidget->setShareLimitAction(inParams.shareLimits.action);
    }

    m_ui->checkBoxRememberLastSavePath->setChecked(m_storeRememberLastSavePath);
    m_ui->doNotDeleteTorrentCheckBox->setVisible(TorrentFileGuard::autoDeleteMode() != TorrentFileGuard::Never);

    // Torrent content filtering
    m_filterLine->setPlaceholderText(tr("Filter files..."));
    m_filterLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_filterLine->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_filterLine, &QWidget::customContextMenuRequested, this, &AddNewTorrentDialog::showContentFilterContextMenu);
    m_ui->contentFilterLayout->insertWidget(0, m_filterLine);
    const auto *focusSearchHotkey = new QShortcut(QKeySequence::Find, this);
    connect(focusSearchHotkey, &QShortcut::activated, this, [this]()
    {
        m_filterLine->setFocus();
        m_filterLine->selectAll();
    });

    loadState();

    if (const QByteArray state = m_storeTreeHeaderState; !state.isEmpty())
        m_ui->contentTreeView->header()->restoreState(state);
    // Only Name / Size / Priority are shown in the Add torrent dialog; fixed
    // widths keep the headers from truncating to "Downl...".
    QHeaderView *treeHeader = m_ui->contentTreeView->header();
    treeHeader->setStretchLastSection(false);
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Progress);
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Remaining);
    m_ui->contentTreeView->hideColumn(TorrentContentWidget::Availability);
    treeHeader->setSectionResizeMode(TorrentContentWidget::Name, QHeaderView::Stretch);
    treeHeader->setSectionResizeMode(TorrentContentWidget::Size, QHeaderView::Fixed);
    treeHeader->setSectionResizeMode(TorrentContentWidget::Priority, QHeaderView::Fixed);
    treeHeader->resizeSection(TorrentContentWidget::Size, 96);
    treeHeader->resizeSection(TorrentContentWidget::Priority, 105);
    m_ui->contentTreeView->setColumnsVisibilityMode(TorrentContentWidget::ColumnsVisibilityMode::Locked);
    connect(m_ui->contentTreeView, &TorrentContentWidget::stateChanged, this, [this] { updateContentFooter(); });
    m_ui->contentTreeView->setDoubleClickAction(TorrentContentWidget::DoubleClickAction::Rename);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->buttonSave, &QPushButton::clicked, this, &AddNewTorrentDialog::saveTorrentFile);
    connect(m_ui->savePath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onSavePathChanged);
    connect(m_ui->downloadPath, &FileSystemPathEdit::selectedPathChanged, this, &AddNewTorrentDialog::onDownloadPathChanged);
    connect(m_ui->groupBoxDownloadPath, &QGroupBox::toggled, this, &AddNewTorrentDialog::onUseDownloadPathChanged);
    connect(m_ui->comboTMM, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::TMMChanged);
    connect(m_ui->startTorrentCheckBox, &QCheckBox::toggled, this, [this](const bool checked)
    {
        m_ui->stopConditionLabel->setEnabled(checked);
        m_ui->stopConditionComboBox->setEnabled(checked);
    });
    connect(m_ui->contentLayoutComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::contentLayoutChanged);
    connect(m_ui->categoryComboBox, &QComboBox::currentIndexChanged, this, &AddNewTorrentDialog::categoryChanged);
    connect(m_ui->tagsEditButton, &QAbstractButton::clicked, this, [this]
    {
        auto *dlg = new TorrentTagsDialog(m_currentContext->torrentParams.tags, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(dlg, &TorrentTagsDialog::accepted, this, [this, dlg]
        {
            m_currentContext->torrentParams.tags = dlg->tags();
            m_ui->tagsLineEdit->setText(Utils::String::joinIntoString(m_currentContext->torrentParams.tags, u", "_s));
        });
        dlg->open();
    });
    connect(m_filterLine, &LineEdit::textUpdated, this, &AddNewTorrentDialog::setContentFilterPattern);
    connect(m_ui->buttonSelectAll, &QPushButton::clicked, m_ui->contentTreeView, &TorrentContentWidget::checkAll);
    connect(m_ui->buttonSelectNone, &QPushButton::clicked, m_ui->contentTreeView, &TorrentContentWidget::checkNone);
    connect(Preferences::instance(), &Preferences::changed, this, []
    {
        const int length = Preferences::instance()->addNewTorrentDialogSavePathHistoryLength();
        settings()->storeValue(KEY_SAVEPATHHISTORY, settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY).mid(0, length));
    });

    setCurrentContext(std::make_shared<Context>(Context {torrentDescr, inParams}));
}

AddNewTorrentDialog::~AddNewTorrentDialog()
{
    delete m_ui;
}

bool AddNewTorrentDialog::isDoNotDeleteTorrentChecked() const
{
    return m_ui->doNotDeleteTorrentCheckBox->isChecked();
}

void AddNewTorrentDialog::loadState()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);

    m_ui->splitter->restoreState(m_storeSplitterState);
}

void AddNewTorrentDialog::saveState()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    m_storeDialogSize = size();
    m_storeSplitterState = m_ui->splitter->saveState();
    if (hasMetadata)
        m_storeTreeHeaderState = m_ui->contentTreeView->header()->saveState();
}

void AddNewTorrentDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
#ifdef Q_OS_WIN
    WindowsThemeHelper::apply(this);
#endif
    if (!Preferences::instance()->isAddNewTorrentDialogTopLevel())
        return;

    activateWindow();
    raise();
}

void AddNewTorrentDialog::setCurrentContext(const std::shared_ptr<Context> context)
{
    Q_ASSERT(context);
    if (!context) [[unlikely]]
        return;

    m_currentContext = context;

    const QSignalBlocker comboTMMSignalBlocker {m_ui->comboTMM};
    const QSignalBlocker startTorrentCheckBoxSignalBlocker {m_ui->startTorrentCheckBox};
    const QSignalBlocker contentLayoutComboBoxSignalBlocker {m_ui->contentLayoutComboBox};
    const QSignalBlocker categoryComboBoxSignalBlocker {m_ui->categoryComboBox};

    const BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;
    const auto *session = BitTorrent::Session::instance();

    // TODO: set dialog file properties using m_torrentParams.filePriorities

    m_ui->comboTMM->setCurrentIndex(addTorrentParams.useAutoTMM.value_or(!session->isAutoTMMDisabledByDefault()) ? 1 : 0);
    m_ui->addToQueueTopCheckBox->setChecked(addTorrentParams.addToQueueTop.value_or(session->isAddTorrentToQueueTop()));
    m_ui->startTorrentCheckBox->setChecked(!addTorrentParams.addStopped.value_or(session->isAddTorrentStopped()));
    m_ui->stopConditionLabel->setEnabled(m_ui->startTorrentCheckBox->isChecked());
    m_ui->stopConditionComboBox->setEnabled(m_ui->startTorrentCheckBox->isChecked());
    m_ui->contentLayoutComboBox->setCurrentIndex(
            static_cast<int>(addTorrentParams.contentLayout.value_or(session->torrentContentLayout())));
    m_ui->sequentialCheckBox->setChecked(addTorrentParams.sequential);
    m_ui->firstLastCheckBox->setChecked(addTorrentParams.firstLastPiecePriority);
    m_ui->seedModeCheckBox->setChecked(addTorrentParams.seedMode);
    m_ui->tagsLineEdit->setText(Utils::String::joinIntoString(addTorrentParams.tags, u", "_s));

    // Load categories
    QStringList categories = session->categories();
    std::ranges::sort(categories, Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
    const QString defaultCategory = m_storeDefaultCategory;

    if (!addTorrentParams.category.isEmpty())
        m_ui->categoryComboBox->addItem(addTorrentParams.category);
    if (!defaultCategory.isEmpty())
        m_ui->categoryComboBox->addItem(defaultCategory);
    m_ui->categoryComboBox->addItem(u""_s);

    for (const QString &category : asConst(categories))
    {
        if ((category != defaultCategory) && (category != addTorrentParams.category))
            m_ui->categoryComboBox->addItem(category);
    }

    m_filterLine->blockSignals(true);
    m_filterLine->clear();

    // Default focus
    if (m_ui->comboTMM->currentIndex() == 0) // 0 is Manual mode
        m_ui->savePath->setFocus();
    else
        m_ui->categoryComboBox->setFocus();

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const BitTorrent::InfoHash infoHash = torrentDescr.infoHash();
    const bool hasMetadata = torrentDescr.info().has_value();

    if (hasMetadata)
        m_ui->stopConditionComboBox->removeItem(m_ui->stopConditionComboBox->findData(QVariant::fromValue(BitTorrent::Torrent::StopCondition::MetadataReceived)));
    else
        m_ui->stopConditionComboBox->insertItem(1, tr("Metadata received"), QVariant::fromValue(BitTorrent::Torrent::StopCondition::MetadataReceived));
    const auto stopCondition = addTorrentParams.stopCondition.value_or(session->torrentStopCondition());
    if (hasMetadata && (stopCondition == BitTorrent::Torrent::StopCondition::MetadataReceived))
    {
        m_ui->startTorrentCheckBox->setChecked(false);
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(QVariant::fromValue(BitTorrent::Torrent::StopCondition::None)));
    }
    else
    {
        m_ui->startTorrentCheckBox->setChecked(!addTorrentParams.addStopped.value_or(session->isAddTorrentStopped()));
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(QVariant::fromValue(stopCondition)));
    }

    m_ui->labelInfohash1Data->setText(infoHash.v1().isValid() ? infoHash.v1().toString() : tr("N/A"));
    m_ui->labelInfohash2Data->setText(infoHash.v2().isValid() ? infoHash.v2().toString() : tr("N/A"));

    if (hasMetadata)
    {
        m_ui->lblMetaLoading->setVisible(false);
        m_ui->progMetaLoading->setVisible(false);
        m_ui->buttonSave->setVisible(true);
        setupTreeview();
    }
    else
    {
        // Set dialog title
        const QString torrentName = torrentDescr.name();
        setWindowTitle(torrentName.isEmpty() ? tr("Magnet link") : torrentName);
        m_ui->labelCommentData->setText(tr("Not Available", "This comment is unavailable"));
        m_ui->labelDateData->setText(tr("Not Available", "This date is unavailable"));
        updateDiskSpaceLabel();
        setMetadataProgressIndicator(true, tr("Retrieving metadata..."));
        m_ui->buttonSave->setVisible(false);
    }

    TMMChanged(m_ui->comboTMM->currentIndex());

    if (m_addHeaderName)
    {
        const QString name = torrentDescr.name().isEmpty() ? tr("Magnet link") : torrentDescr.name();
        const QFontMetrics metrics(m_addHeaderName->font());
        m_addHeaderName->setText(metrics.elidedText(name, Qt::ElideRight, 560));
        m_addHeaderName->setAccessibleName(name);
    }
    if (m_addHeaderMeta)
    {
        QStringList meta;
        if (hasMetadata)
        {
            const BitTorrent::InfoHash hash = torrentDescr.infoHash();
            meta << Utils::Misc::friendlyUnit((*torrentDescr.info()).totalSize())
                 << tr("%1 files").arg((*torrentDescr.info()).filesCount())
                 << ((hash.v1().isValid() && hash.v2().isValid()) ? tr("v1 + v2 hybrid")
                        : (hash.v2().isValid() ? tr("v2") : tr("v1")));
        }
        if (torrentDescr.creationDate().isValid())
            meta << torrentDescr.creationDate().toString(Qt::TextDate);
        if (!torrentDescr.creator().isEmpty())
            meta << tr("by %1").arg(torrentDescr.creator());
        m_addHeaderMeta->setText(meta.isEmpty() ? tr("Fetching metadata\u2026")
                : meta.join(QStringLiteral(" \u00b7 ")));
    }

    updateContentFooter();
}

void AddNewTorrentDialog::updateCurrentContext()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;

    addTorrentParams.seedMode = m_ui->seedModeCheckBox->isChecked();

    // Category
    addTorrentParams.category = m_ui->categoryComboBox->currentText();
    if (m_ui->defaultCategoryCheckbox->isChecked())
        m_storeDefaultCategory = addTorrentParams.category;

    m_storeRememberLastSavePath = m_ui->checkBoxRememberLastSavePath->isChecked();

    addTorrentParams.addToQueueTop = m_ui->addToQueueTopCheckBox->isChecked();
    addTorrentParams.addStopped = !m_ui->startTorrentCheckBox->isChecked();
    addTorrentParams.stopCondition = m_ui->stopConditionComboBox->currentData().value<BitTorrent::Torrent::StopCondition>();
    addTorrentParams.contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());

    addTorrentParams.sequential = m_ui->sequentialCheckBox->isChecked();
    addTorrentParams.firstLastPiecePriority = m_ui->firstLastCheckBox->isChecked();

    addTorrentParams.shareLimits.ratioLimit = m_ui->torrentShareLimitsWidget->ratioLimit().value();
    addTorrentParams.shareLimits.seedingTimeLimit = m_ui->torrentShareLimitsWidget->seedingTimeLimit().value();
    addTorrentParams.shareLimits.inactiveSeedingTimeLimit = m_ui->torrentShareLimitsWidget->inactiveSeedingTimeLimit().value();
    addTorrentParams.shareLimits.mode = m_ui->torrentShareLimitsWidget->shareLimitsMode().value();
    addTorrentParams.shareLimits.action = m_ui->torrentShareLimitsWidget->shareLimitAction().value();

    const bool useAutoTMM = (m_ui->comboTMM->currentIndex() == 1); // 1 is Automatic mode. Handle all non 1 values as manual mode.
    addTorrentParams.useAutoTMM = useAutoTMM;
    if (!useAutoTMM)
    {
        const int savePathHistoryLength = Preferences::instance()->addNewTorrentDialogSavePathHistoryLength();
        const Path savePath = m_ui->savePath->selectedPath();
        addTorrentParams.savePath = savePath;
        updatePathHistory(KEY_SAVEPATHHISTORY, savePath, savePathHistoryLength);

        addTorrentParams.useDownloadPath = m_ui->groupBoxDownloadPath->isChecked();
        if (addTorrentParams.useDownloadPath)
        {
            const Path downloadPath = m_ui->downloadPath->selectedPath();
            addTorrentParams.downloadPath = downloadPath;
            updatePathHistory(KEY_DOWNLOADPATHHISTORY, downloadPath, savePathHistoryLength);
        }
        else
        {
            addTorrentParams.downloadPath = Path();
        }
    }
    else
    {
        addTorrentParams.savePath = Path();
        addTorrentParams.downloadPath = Path();
        addTorrentParams.useDownloadPath = std::nullopt;
    }
}

void AddNewTorrentDialog::updateDiskSpaceLabel()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    // Determine torrent size
    qlonglong torrentSize = 0;
    if (hasMetadata)
    {
        const auto torrentInfo = *torrentDescr.info();
        const QList<BitTorrent::DownloadPriority> &priorities = m_contentAdaptor->filePriorities();
        Q_ASSERT(priorities.size() == torrentInfo.filesCount());
        for (qsizetype i = 0; i < priorities.size(); ++i)
        {
            if (priorities[i] > BitTorrent::DownloadPriority::Ignored)
                torrentSize += torrentInfo.fileSize(i);
        }
    }

    const qlonglong freeBytes = Utils::Fs::freeDiskSpaceOnPath(m_ui->savePath->selectedPath());
    const QString freeSpace = Utils::Misc::friendlyUnit(freeBytes);
    const QString sizeString = (torrentSize > 0)
            ? Utils::Misc::friendlyUnit(torrentSize)
            : tr("Not available", "This size is unavailable.");
    m_ui->labelSizeData->setText(tr("%1 (Free space on disk: %2)").arg(sizeString, freeSpace));

    if (auto *help = findChild<QLabel *>(QStringLiteral("freeSpaceHelp")))
        help->setText(tr("Needs %1 \u00b7 %2 free").arg(sizeString, freeSpace));
    if (auto *bar = findChild<QFrame *>(QStringLiteral("freeSpaceBar")))
    {
        if (auto *fill = bar->findChild<QFrame *>(QStringLiteral("freeSpaceFill")))
        {
            const qreal ratio = ((freeBytes > 0) && (torrentSize > 0))
                    ? std::min(1.0, static_cast<qreal>(torrentSize) / static_cast<qreal>(freeBytes)) : 0;
            const int fillWidth = static_cast<int>(bar->width() * ratio);
            // Always show a >= 4 px accent sliver for non-empty torrents.
            fill->setGeometry(0, 0, (torrentSize > 0) ? std::max(4, fillWidth) : 0, 4);
        }
    }

    updateContentFooter();
}

void AddNewTorrentDialog::updateContentFooter()
{
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const BitTorrent::InfoHash infoHash = torrentDescr.infoHash();
    const QString shortHash = infoHash.v1().isValid() ? infoHash.v1().toString().left(8)
            : (infoHash.v2().isValid() ? infoHash.v2().toString().left(8) : QString {});
    if (auto *hashLabel = findChild<QLabel *>(QStringLiteral("hashInfoLabel")))
        hashLabel->setText(tr("Info hash %1").arg(shortHash));

    auto *selectionLabel = findChild<QLabel *>(QStringLiteral("selectionInfoLabel"));
    if (!selectionLabel)
        return;

    if (!torrentDescr.info())
    {
        selectionLabel->setText(tr("Selected 0 of 0"));
        return;
    }

    const auto &torrentInfo = *torrentDescr.info();
    const QList<BitTorrent::DownloadPriority> priorities = m_contentAdaptor
            ? m_contentAdaptor->filePriorities() : QList<BitTorrent::DownloadPriority> {};
    qlonglong selectedSize = 0;
    qlonglong totalSize = 0;
    for (qsizetype i = 0; i < torrentInfo.filesCount(); ++i)
    {
        const qlonglong fileSize = torrentInfo.fileSize(i);
        totalSize += fileSize;
        if ((i < priorities.size()) && (priorities[i] > BitTorrent::DownloadPriority::Ignored))
            selectedSize += fileSize;
    }
    selectionLabel->setText(tr("Selected %1 of %2")
            .arg(Utils::Misc::friendlyUnit(selectedSize), Utils::Misc::friendlyUnit(totalSize)));
}

void AddNewTorrentDialog::onSavePathChanged([[maybe_unused]] const Path &newPath)
{
    // Remember index
    m_savePathIndex = m_ui->savePath->currentIndex();
    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::onDownloadPathChanged([[maybe_unused]] const Path &newPath)
{
    // Remember index
    const int currentPathIndex = m_ui->downloadPath->currentIndex();
    if (currentPathIndex >= 0)
        m_downloadPathIndex = m_ui->downloadPath->currentIndex();
}

void AddNewTorrentDialog::onUseDownloadPathChanged(const bool checked)
{
    m_useDownloadPath = checked;
    m_ui->downloadPath->setCurrentIndex(checked ? m_downloadPathIndex : -1);
}

void AddNewTorrentDialog::categoryChanged([[maybe_unused]] const int index)
{
    if (m_ui->comboTMM->currentIndex() == 1)
    {
        const auto *btSession = BitTorrent::Session::instance();
        const QString categoryName = m_ui->categoryComboBox->currentText();

        const Path savePath = btSession->categorySavePath(categoryName);
        m_ui->savePath->setSelectedPath(savePath);

        const Path downloadPath = btSession->categoryDownloadPath(categoryName);
        m_ui->downloadPath->setSelectedPath(downloadPath);

        m_ui->groupBoxDownloadPath->setChecked(!m_ui->downloadPath->selectedPath().isEmpty());

        updateDiskSpaceLabel();
    }
}

void AddNewTorrentDialog::contentLayoutChanged()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    if (!hasMetadata)
        return;

    const auto contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());
    m_contentAdaptor->applyContentLayout(contentLayout);
}

void AddNewTorrentDialog::saveTorrentFile()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    Q_ASSERT(hasMetadata);
    if (!hasMetadata) [[unlikely]]
        return;

    const auto torrentInfo = *torrentDescr.info();

    const QString filter {tr("Torrent file (*%1)").arg(TORRENT_FILE_EXTENSION)};

    Path path {QFileDialog::getSaveFileName(this, tr("Save as torrent file")
            , QDir::home().absoluteFilePath(torrentInfo.name() + TORRENT_FILE_EXTENSION)
            , filter)};
    if (path.isEmpty())
        return;

    if (!path.hasExtension(TORRENT_FILE_EXTENSION))
        path += TORRENT_FILE_EXTENSION;

    if (const auto result = torrentDescr.saveToFile(path); !result)
    {
        QMessageBox::critical(this, tr("I/O Error")
                , tr("Couldn't export torrent metadata file '%1'. Reason: %2.").arg(path.toString(), result.error()));
    }
}

void AddNewTorrentDialog::showContentFilterContextMenu()
{
    QMenu *menu = m_filterLine->createStandardContextMenu();

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

void AddNewTorrentDialog::setContentFilterPattern()
{
    m_ui->contentTreeView->setFilterPattern(m_filterLine->text(), m_storeFilterPatternFormat.get(FilterPatternFormat::Wildcards));
}

void AddNewTorrentDialog::populateSavePaths()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;
    const auto *btSession = BitTorrent::Session::instance();

    m_ui->savePath->blockSignals(true);
    m_ui->savePath->clear();
    const auto savePathHistory = settings()->loadValue<QStringList>(KEY_SAVEPATHHISTORY);
    if (savePathHistory.size() > 0)
    {
        for (const QString &path : savePathHistory)
            m_ui->savePath->addItem(Path(path));
    }
    else
    {
        m_ui->savePath->addItem(btSession->savePath());
    }

    if (m_savePathIndex >= 0)
    {
        m_ui->savePath->setCurrentIndex(std::min(m_savePathIndex, (m_ui->savePath->count() - 1)));
    }
    else
    {
        if (!addTorrentParams.savePath.isEmpty())
            setPath(m_ui->savePath, addTorrentParams.savePath);
        else if (!m_storeRememberLastSavePath)
            setPath(m_ui->savePath, btSession->savePath());
        else
            m_ui->savePath->setCurrentIndex(0);

        m_savePathIndex = m_ui->savePath->currentIndex();
    }

    m_ui->savePath->blockSignals(false);

    m_ui->downloadPath->blockSignals(true);
    m_ui->downloadPath->clear();
    const auto downloadPathHistory = settings()->loadValue<QStringList>(KEY_DOWNLOADPATHHISTORY);
    if (downloadPathHistory.size() > 0)
    {
        for (const QString &path : downloadPathHistory)
            m_ui->downloadPath->addItem(Path(path));
    }
    else
    {
        m_ui->downloadPath->addItem(btSession->downloadPath());
    }

    if (m_downloadPathIndex >= 0)
    {
        m_ui->downloadPath->setCurrentIndex(m_useDownloadPath ? std::min(m_downloadPathIndex, (m_ui->downloadPath->count() - 1)) : -1);
        m_ui->groupBoxDownloadPath->setChecked(m_useDownloadPath);
    }
    else
    {
        const bool useDownloadPath = addTorrentParams.useDownloadPath.value_or(btSession->isDownloadPathEnabled());
        m_ui->groupBoxDownloadPath->setChecked(useDownloadPath);

        if (!addTorrentParams.downloadPath.isEmpty())
            setPath(m_ui->downloadPath, addTorrentParams.downloadPath);
        else if (!m_storeRememberLastSavePath)
            setPath(m_ui->downloadPath, btSession->downloadPath());
        else
            m_ui->downloadPath->setCurrentIndex(0);

        m_downloadPathIndex = m_ui->downloadPath->currentIndex();
        if (!useDownloadPath)
            m_ui->downloadPath->setCurrentIndex(-1);
    }

    m_ui->downloadPath->blockSignals(false);
    m_ui->groupBoxDownloadPath->blockSignals(false);
}

void AddNewTorrentDialog::accept()
{
    // Check "Save at" path selected is valid (only in Manual mode)
    if ((m_ui->comboTMM->currentIndex() == 0) && (!m_ui->savePath->selectedPath().isValid()))
    {
        QMessageBox::warning(this, tr("Invalid save path"),
                             tr("The \"Save at\" path contains invalid characters."));
        return;
    }

    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    updateCurrentContext();
    emit torrentAccepted(m_currentContext->torrentDescr, m_currentContext->torrentParams);

    Preferences::instance()->setAddNewTorrentDialogEnabled(!m_ui->checkBoxNeverShow->isChecked());

    QDialog::accept();
}

void AddNewTorrentDialog::reject()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    emit torrentRejected(m_currentContext->torrentDescr);

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();
    if (!hasMetadata)
    {
        setMetadataProgressIndicator(false);
        BitTorrent::Session::instance()->cancelDownloadMetadata(torrentDescr.infoHash().toTorrentID());
    }

    QDialog::reject();
}

void AddNewTorrentDialog::done(const int result)
{
    saveState();
    QDialog::done(result);
}

void AddNewTorrentDialog::updateMetadata(const BitTorrent::TorrentInfo &metadata)
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    Q_ASSERT(metadata.isValid());
    if (!metadata.isValid()) [[unlikely]]
        return;

    BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    if (torrentDescr.info())
        return;

    Q_ASSERT(metadata.matchesInfoHash(torrentDescr.infoHash()));
    if (!metadata.matchesInfoHash(torrentDescr.infoHash())) [[unlikely]]
        return;

    torrentDescr.setTorrentInfo(metadata);
    setMetadataProgressIndicator(true, tr("Parsing metadata..."));

    // Update UI
    setupTreeview();
    setMetadataProgressIndicator(false, tr("Metadata retrieval complete"));

    if (const auto stopCondition = m_ui->stopConditionComboBox->currentData().value<BitTorrent::Torrent::StopCondition>()
            ; stopCondition == BitTorrent::Torrent::StopCondition::MetadataReceived)
    {
        m_ui->startTorrentCheckBox->setChecked(false);

        const auto index = m_ui->stopConditionComboBox->currentIndex();
        m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(
                QVariant::fromValue(BitTorrent::Torrent::StopCondition::None)));
        m_ui->stopConditionComboBox->removeItem(index);
    }

    m_ui->buttonSave->setVisible(true);
    if (torrentDescr.infoHash().v2().isValid())
    {
        m_ui->buttonSave->setEnabled(false);
        m_ui->buttonSave->setToolTip(tr("Cannot create v2 torrent until its data is fully downloaded."));
    }
}

void AddNewTorrentDialog::setMetadataProgressIndicator(bool visibleIndicator, const QString &labelText)
{
    // Always show info label when waiting for metadata
    m_ui->lblMetaLoading->setVisible(true);
    m_ui->lblMetaLoading->setText(labelText);
    m_ui->progMetaLoading->setVisible(visibleIndicator);
}

void AddNewTorrentDialog::setupTreeview()
{
    Q_ASSERT(m_currentContext);
    if (!m_currentContext) [[unlikely]]
        return;

    const BitTorrent::TorrentDescriptor &torrentDescr = m_currentContext->torrentDescr;
    const bool hasMetadata = torrentDescr.info().has_value();

    Q_ASSERT(hasMetadata);
    if (!hasMetadata) [[unlikely]]
        return;

    // Set dialog title
    setWindowTitle(torrentDescr.name());

    // Set torrent information
    m_ui->labelCommentData->setText(Utils::Misc::parseHtmlLinks(torrentDescr.comment().toHtmlEscaped()));
    m_ui->labelDateData->setText(!torrentDescr.creationDate().isNull() ? QLocale().toString(torrentDescr.creationDate(), QLocale::ShortFormat) : tr("Not available"));

    const auto &torrentInfo = *torrentDescr.info();

    BitTorrent::AddTorrentParams &addTorrentParams = m_currentContext->torrentParams;
    if (addTorrentParams.filePaths.isEmpty())
        addTorrentParams.filePaths = torrentInfo.filePaths();

    if (addTorrentParams.filePriorities.isEmpty())
    {
        lt::add_torrent_params p = torrentDescr.ltAddTorrentParams();
        if (p.file_priorities.empty())
        {
            // Use qBittorrent default priority rather than libtorrent's (4)
            addTorrentParams.filePriorities.resize(torrentInfo.filesCount(), BitTorrent::DownloadPriority::Normal);
        }
        else
        {
            addTorrentParams.filePriorities.resize(torrentInfo.filesCount(), BitTorrent::DownloadPriority::Ignored);
            const auto nativeIndexes = torrentInfo.nativeIndexes();
            for (qsizetype i = 0; i < nativeIndexes.size(); ++i)
            {
                const auto ordIndex = static_cast<std::size_t>(BitTorrent::LT::toUnderlyingType(nativeIndexes[i]));
                if (ordIndex < p.file_priorities.size())
                    addTorrentParams.filePriorities[i] = BitTorrent::LT::fromNative(p.file_priorities[ordIndex]);
            }
        }
    }

    m_contentAdaptor = std::make_unique<TorrentContentAdaptor>(torrentInfo, addTorrentParams.filePaths
            , addTorrentParams.filePriorities, [this] { updateDiskSpaceLabel(); });

    const auto contentLayout = static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex());
    m_contentAdaptor->applyContentLayout(contentLayout);

    if (BitTorrent::Session::instance()->isExcludedFileNamesEnabled())
    {
        // Check file name blacklist for torrents that are manually added
        QList<BitTorrent::DownloadPriority> priorities = m_contentAdaptor->filePriorities();
        BitTorrent::Session::instance()->applyFilenameFilter(m_contentAdaptor->filePaths(), priorities);
        m_contentAdaptor->prioritizeFiles(priorities);
    }

    m_ui->contentTreeView->setContentHandler(m_contentAdaptor.get());

    m_filterLine->blockSignals(false);

    updateDiskSpaceLabel();
}

void AddNewTorrentDialog::TMMChanged(int index)
{
    if (index != 1)
    { // 0 is Manual mode and 1 is Automatic mode. Handle all non 1 values as manual mode.
        populateSavePaths();
        m_ui->groupBoxSavePath->setEnabled(true);
    }
    else
    {
        const auto *session = BitTorrent::Session::instance();

        m_ui->groupBoxSavePath->setEnabled(false);

        m_ui->savePath->blockSignals(true);
        m_ui->savePath->clear();
        const Path savePath = session->categorySavePath(m_ui->categoryComboBox->currentText());
        m_ui->savePath->addItem(savePath);

        m_ui->downloadPath->blockSignals(true);
        m_ui->downloadPath->clear();
        const Path downloadPath = session->categoryDownloadPath(m_ui->categoryComboBox->currentText());
        m_ui->downloadPath->addItem(downloadPath);

        m_ui->groupBoxDownloadPath->blockSignals(true);
        m_ui->groupBoxDownloadPath->setChecked(!downloadPath.isEmpty());
    }

    updateDiskSpaceLabel();
}

AddNewTorrentDialog::TorrentContentAdaptor::TorrentContentAdaptor(const BitTorrent::TorrentInfo &torrentInfo
        , PathList &filePaths, QList<BitTorrent::DownloadPriority> &filePriorities, std::function<void ()> onFilePrioritiesChanged)
    : m_torrentInfo {torrentInfo}
    , m_filePaths {filePaths}
    , m_filePriorities {filePriorities}
    , m_onFilePrioritiesChanged {std::move(onFilePrioritiesChanged)}
{
    Q_ASSERT(filePaths.isEmpty() || (filePaths.size() == m_torrentInfo.filesCount()));

    m_originalRootFolder = Path::findRootFolder(m_torrentInfo.filePaths());
    m_currentContentLayout = (m_originalRootFolder.isEmpty()
            ? BitTorrent::TorrentContentLayout::NoSubfolder
            : BitTorrent::TorrentContentLayout::Subfolder);

    if (const int fileCount = filesCount(); !m_filePriorities.isEmpty() && (fileCount >= 0))
        m_filePriorities.resize(fileCount, BitTorrent::DownloadPriority::Normal);
}

bool AddNewTorrentDialog::TorrentContentAdaptor::hasMetadata() const
{
    return m_torrentInfo.isValid();
}

int AddNewTorrentDialog::TorrentContentAdaptor::filesCount() const
{
    return m_torrentInfo.filesCount();
}

qlonglong AddNewTorrentDialog::TorrentContentAdaptor::fileSize(const int index) const
{
    Q_ASSERT((index >= 0) && (index < filesCount()));
    return m_torrentInfo.fileSize(index);
}

Path AddNewTorrentDialog::TorrentContentAdaptor::filePath(const int index) const
{
    Q_ASSERT((index >= 0) && (index < filesCount()));
    return (m_filePaths.isEmpty() ? m_torrentInfo.filePath(index) : m_filePaths.at(index));
}

PathList AddNewTorrentDialog::TorrentContentAdaptor::filePaths() const
{
    return (m_filePaths.isEmpty() ? m_torrentInfo.filePaths() : m_filePaths);
}

QList<BitTorrent::DownloadPriority> AddNewTorrentDialog::TorrentContentAdaptor::filePriorities() const
{
    return m_filePriorities.isEmpty()
            ? QList<BitTorrent::DownloadPriority>(filesCount(), BitTorrent::DownloadPriority::Normal)
            : m_filePriorities;
}

QList<qreal> AddNewTorrentDialog::TorrentContentAdaptor::filesProgress() const
{
    return QList<qreal>(filesCount(), 0);
}

QFuture<QList<qreal> > AddNewTorrentDialog::TorrentContentAdaptor::fetchAvailableFileFractions() const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
    return QtFuture::makeReadyValueFuture(QList<qreal>(filesCount(), 0));
#else
    return QtFuture::makeReadyFuture(QList<qreal>(filesCount(), 0));
#endif
}

Path AddNewTorrentDialog::TorrentContentAdaptor::actualStorageLocation() const
{
    return {};
}

Path AddNewTorrentDialog::TorrentContentAdaptor::actualFilePath([[maybe_unused]] const int fileIndex) const
{
    return {};
}

void AddNewTorrentDialog::TorrentContentAdaptor::flushCache() const
{
}

void AddNewTorrentDialog::TorrentContentAdaptor::renameFile(const int index, const Path &newFilePath)
{
    Q_ASSERT((index >= 0) && (index < filesCount()));
    if (!((index >= 0) && (index < filesCount()))) [[unlikely]]
        return;

    const Path oldFilePath = filePath(index);
    // We should compare path in a case sensitive manner even on case insensitive
    // platforms since it can be renamed by only changing case of some character(s)
    if (oldFilePath.data() == newFilePath.data())
        return;

    doRenameFile(index, newFilePath);

    emit fileRenamed(index, oldFilePath);
}

void AddNewTorrentDialog::TorrentContentAdaptor::applyContentLayout(const BitTorrent::TorrentContentLayout contentLayout)
{
    Q_ASSERT(hasMetadata());
    Q_ASSERT(!m_filePaths.isEmpty());

    const auto originalContentLayout = (m_originalRootFolder.isEmpty()
            ? BitTorrent::TorrentContentLayout::NoSubfolder
            : BitTorrent::TorrentContentLayout::Subfolder);
    const auto newContentLayout = ((contentLayout == BitTorrent::TorrentContentLayout::Original)
            ? originalContentLayout : contentLayout);
    if (newContentLayout != m_currentContentLayout)
    {
        const PathList oldFilePaths = m_filePaths;

        if (newContentLayout == BitTorrent::TorrentContentLayout::NoSubfolder)
        {
            Path::stripRootFolder(m_filePaths);
        }
        else
        {
            const auto rootFolder = ((originalContentLayout == BitTorrent::TorrentContentLayout::Subfolder)
                    ? m_originalRootFolder : m_filePaths.at(0).removedExtension());
            Path::addRootFolder(m_filePaths, rootFolder);
        }

        m_currentContentLayout = newContentLayout;

        const qsizetype filesCount = m_filePaths.size();
        for (int i = 0; i < filesCount; ++i)
            emit fileRenamed(i, oldFilePaths.at(i));
    }
}

void AddNewTorrentDialog::TorrentContentAdaptor::prioritizeFiles(const QList<BitTorrent::DownloadPriority> &priorities)
{
    Q_ASSERT(priorities.size() == filesCount());
    m_filePriorities = priorities;
    if (m_onFilePrioritiesChanged)
        m_onFilePrioritiesChanged();
}

void AddNewTorrentDialog::TorrentContentAdaptor::doRenameFile(const int index, const Path &newFilePath)
{
    if (m_filePaths.isEmpty())
        m_filePaths = m_torrentInfo.filePaths();

    m_filePaths[index] = newFilePath;
}

void AddNewTorrentDialog::TorrentContentAdaptor::doRenameFolder(const Path &oldFolderPath, const Path &newFolderPath)
{
    QHash<int, Path> renamedFiles;
    renamedFiles.reserve(filesCount());
    for (int i = 0; i < filesCount(); ++i)
    {
        const Path path = filePath(i);
        if (path.hasAncestor(oldFolderPath))
        {
            const Path newFilePath = newFolderPath / oldFolderPath.relativePathOf(path);
            doRenameFile(i, newFilePath);
            renamedFiles.insert(i, path);
        }
    }

    emit folderRenamed(newFolderPath, oldFolderPath, renamedFiles);
}
