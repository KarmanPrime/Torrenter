/*
 * Torrenter Options dialog - structural layout helper (Phase A / mockup 05).
 * See optionslayouthelper.h.
 */
#include "optionslayouthelper.h"

#include <algorithm>

#include <QAbstractButton>
#include <QBoxLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QLayoutItem>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "widgets/segmentedcontrol.h"
#include "widgets/toggleswitch.h"

namespace
{
    int widgetDepth(const QWidget *widget, const QWidget *stopAt)
    {
        int depth = 0;
        for (const QWidget *w = widget->parentWidget(); w && (w != stopAt); w = w->parentWidget())
            ++depth;
        return depth;
    }

    // Cards use real layouts and grow to fit their contents. Never impose a
    // fixed/minimum row height: nested groups and multi-line fields need more
    // than a single row and would otherwise paint over each other.
    void relaxWidget(QWidget *w)
    {
        if (w == nullptr)
            return;
        QSizePolicy policy = w->sizePolicy();
        policy.setVerticalPolicy(qobject_cast<QGroupBox *>(w) ? QSizePolicy::Preferred
                                                             : QSizePolicy::Minimum);
        w->setSizePolicy(policy);
        w->setMaximumHeight(QWIDGETSIZE_MAX);
    }

    void buildRows(QVBoxLayout *vbox, bool isCard)
    {
        vbox->setContentsMargins(16, 10, 16, 10);
        vbox->setSpacing(8);
        if (!isCard)
            return;

        QList<int> rowPositions;
        for (int i = 0; i < vbox->count(); ++i)
        {
            QLayoutItem *item = vbox->itemAt(i);
            if (item->spacerItem())
                continue;
            rowPositions.append(i);
            if (QLayout *child = item->layout())
            {
                child->setContentsMargins(0, 0, 0, 0);
                if (QGridLayout *grid = qobject_cast<QGridLayout *>(child))
                {
                    grid->setHorizontalSpacing(16);
                    grid->setVerticalSpacing(8);
                }
                else
                {
                    child->setSpacing(8);
                }
            }
            else if (QWidget *w = item->widget())
            {
                relaxWidget(w);
            }
        }

        int inserted = 0;
        for (int k = 0; k + 1 < rowPositions.count(); ++k)
        {
            auto *hairline = new QFrame;
            hairline->setObjectName(QStringLiteral("settingsRowHairline"));
            hairline->setFrameShape(QFrame::NoFrame);
            hairline->setFixedHeight(1);
            vbox->insertWidget(rowPositions.at(k) + 1 + inserted, hairline);
            ++inserted;
        }
    }

    void styleGrid(QGridLayout *grid)
    {
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(8);
    }

    void wrapGroupBox(QGroupBox *gb)
    {
        const QString title = gb->title();
        const QString originalName = gb->objectName();
        if ((originalName == QStringLiteral("groupWebUIUser")) || (originalName == QStringLiteral("groupWebUIAPIKey")))
        {
            // Plain sub-sections inside the Authentication card: keep their
            // native layouts and let them size to their contents.
            gb->setMaximumHeight(QWIDGETSIZE_MAX);
            if (QLayout *subLayout = gb->layout())
            {
                subLayout->setContentsMargins(16, 8, 16, 8);
                subLayout->setSpacing(8);
            }
            relaxWidget(gb);
            for (QWidget *child : gb->findChildren<QWidget *>())
                relaxWidget(child);
            return;
        }
        QLayout *parentLayout = (gb->parentWidget() != nullptr) ? gb->parentWidget()->layout() : nullptr;
        int parentIndex = -1;
        if (parentLayout != nullptr)
        {
            for (int i = 0; i < parentLayout->count(); ++i)
            {
                if (parentLayout->itemAt(i)->widget() == gb)
                {
                    parentIndex = i;
                    break;
                }
            }
        }
        // Remember the exact slot before removing: grid/form parents need the
        // wrapper restored to the same cell, not appended at the end.
        int gridRow = -1;
        int gridCol = -1;
        int gridRowSpan = 1;
        int gridColSpan = 1;
        int formRow = -1;
        QFormLayout::ItemRole formRole = QFormLayout::LabelRole;
        if (auto *grid = qobject_cast<QGridLayout *>(parentLayout))
        {
            for (int i = 0; i < grid->count(); ++i)
            {
                int r = 0;
                int c = 0;
                int rs = 1;
                int cs = 1;
                grid->getItemPosition(i, &r, &c, &rs, &cs);
                if (grid->itemAt(i)->widget() == gb)
                {
                    gridRow = r;
                    gridCol = c;
                    gridRowSpan = rs;
                    gridColSpan = cs;
                    break;
                }
            }
        }
        else if (auto *form = qobject_cast<QFormLayout *>(parentLayout))
        {
            form->getWidgetPosition(gb, &formRow, &formRole);
        }

        if (parentLayout != nullptr)
            parentLayout->removeWidget(gb);

        auto *outer = new QWidget;
        outer->setObjectName(QStringLiteral("settingsGroup"));
        auto *outerLayout = new QVBoxLayout(outer);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(8);

        // Title (or a ToggleSwitch in the header for checkable groups) sits
        // OUTSIDE the raised card, exactly as §5.6 / mockup 05 require.
        if (gb->isCheckable())
        {
            auto *header = new QHBoxLayout;
            header->setContentsMargins(4, 0, 0, 0);
            header->setSpacing(8);
            auto *toggle = new ToggleSwitch(outer);
            toggle->setObjectName(QStringLiteral("settingsCardToggle"));
            toggle->setText(title);
            toggle->setChecked(gb->isChecked());
            toggle->setAccessibleName(title);
            QObject::connect(toggle, &QCheckBox::toggled, gb, &QGroupBox::setChecked);
            QObject::connect(gb, &QGroupBox::toggled, toggle, &QCheckBox::setChecked);
            header->addWidget(toggle);
            header->addStretch();
            outerLayout->addLayout(header);
        }
        else if (!title.isEmpty())
        {
            auto *titleLabel = new QLabel(title, outer);
            titleLabel->setObjectName(QStringLiteral("settingsCardTitle"));
            titleLabel->setProperty("groupTitle", title);
            outerLayout->addWidget(titleLabel);
        }

        gb->setTitle({});
        // Keep the Designer object name intact (m_ui pointers and any name
        // based lookup stay valid); style the card via a dynamic property.
        gb->setProperty("settingsCard", true);
        gb->setMaximumHeight(QWIDGETSIZE_MAX);
        for (QWidget *child : gb->findChildren<QWidget *>())
            relaxWidget(child);
        relaxWidget(gb);
        if (QVBoxLayout *vbox = qobject_cast<QVBoxLayout *>(gb->layout()))
        {
            vbox->setContentsMargins(16, 10, 16, 10);
            vbox->setSpacing(0);
            buildRows(vbox, true);
        }
        else if (QGridLayout *grid = qobject_cast<QGridLayout *>(gb->layout()))
        {
            grid->setContentsMargins(16, 10, 16, 10);
            styleGrid(grid);
            // "Action on double-click": two 140+ px combos in one row must never
            // be clipped. Give the column stretch and a sane minimum width.
            if (originalName == QStringLiteral("groupBox_7"))
            {
                grid->setColumnStretch(1, 1);
                for (QComboBox *combo : gb->findChildren<QComboBox *>())
                    combo->setMinimumWidth(210);
            }
        }
        else if (gb->layout() != nullptr)
        {
            gb->layout()->setContentsMargins(16, 10, 16, 10);
        }

        outerLayout->addWidget(gb);
        if (parentLayout != nullptr)
        {
            if (auto *box = qobject_cast<QBoxLayout *>(parentLayout))
            {
                box->insertWidget(parentIndex, outer);
            }
            else if (auto *grid = qobject_cast<QGridLayout *>(parentLayout))
            {
                if (gridRow >= 0)
                    grid->addWidget(outer, gridRow, gridCol, gridRowSpan, gridColSpan);
                else
                    grid->addWidget(outer);
            }
            else if (auto *form = qobject_cast<QFormLayout *>(parentLayout))
            {
                if (formRow >= 0)
                    form->setWidget(formRow, formRole, outer);
                else
                    form->addRow(outer);
            }
            else
            {
                parentLayout->addWidget(outer);
            }
        }
    }

    void addSchemeSegments(QWidget *dialog)
    {
        auto *combo = dialog->findChild<QComboBox *>(QStringLiteral("comboColorScheme"));
        if (combo == nullptr)
            return;
        auto *grid = dialog->findChild<QGridLayout *>(QStringLiteral("layoutStyle"));
        if (grid == nullptr)
            return;

        int row = -1;
        int col = -1;
        for (int i = 0; i < grid->count(); ++i)
        {
            int r = 0;
            int c = 0;
            int rs = 0;
            int cs = 0;
            grid->getItemPosition(i, &r, &c, &rs, &cs);
            QLayoutItem *item = grid->itemAt(i);
            if ((item != nullptr) && (item->widget() == combo))
            {
                row = r;
                col = c;
                break;
            }
        }
        if ((row < 0) || (col < 0))
            return;

        auto *segments = new SegmentedControl(dialog);
        segments->setObjectName(QStringLiteral("colorSchemeSegments"));
        segments->setAccessibleName(QObject::tr("Color scheme"));
        segments->addSegment(QObject::tr("System", "System color scheme"));
        segments->addSegment(QObject::tr("Light", "Light color scheme"));
        segments->addSegment(QObject::tr("Dark", "Dark color scheme"));
        segments->setToolTip(QObject::tr("Follows Windows when set to System."));

        grid->removeWidget(combo);
        combo->hide();
        grid->addWidget(segments, row, col, 1, 1);

        QObject::connect(segments, &SegmentedControl::currentIndexChanged, combo,
                         [combo](int index) { combo->setCurrentIndex(index); });
        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), segments,
                         [segments](int index) { segments->setCurrentIndex(index); });
        segments->setCurrentIndex(combo->currentIndex());
    }

    void buildFooter(QWidget *dialog)
    {
        auto *root = qobject_cast<QVBoxLayout *>(dialog->layout());
        auto *buttonBox = dialog->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
        auto *restartHint = dialog->findChild<QLabel *>(QStringLiteral("labelRestartRequired"));
        if ((root == nullptr) || (buttonBox == nullptr))
            return;

        root->removeWidget(buttonBox);

        auto *footer = new QWidget;
        footer->setObjectName(QStringLiteral("settingsFooter"));
        auto *footerOuter = new QVBoxLayout(footer);
        footerOuter->setContentsMargins(0, 0, 0, 0);
        footerOuter->setSpacing(14);

        auto *hairline = new QFrame;
        hairline->setObjectName(QStringLiteral("settingsFooterHairline"));
        hairline->setFrameShape(QFrame::NoFrame);
        hairline->setFixedHeight(1);
        footerOuter->addWidget(hairline);

        auto *bar = new QHBoxLayout;
        bar->setContentsMargins(28, 0, 28, 0);
        bar->setSpacing(10);
        if (restartHint != nullptr)
        {
            restartHint->setParent(nullptr);
            auto *info = new QLabel(QStringLiteral("i"), footer);
            info->setObjectName(QStringLiteral("settingsInfoIcon"));
            info->setAlignment(Qt::AlignCenter);
            info->setToolTip(restartHint->text());
            bar->addWidget(info);
            bar->addWidget(restartHint);
        }
        bar->addStretch();
        bar->addWidget(buttonBox);
        footerOuter->addLayout(bar);

        root->addWidget(footer);
    }
}

void OptionsLayoutHelper::apply(QWidget *dialog)
{
    if (dialog == nullptr)
        return;

    buildFooter(dialog);

    auto *pages = dialog->findChild<QStackedWidget *>(QStringLiteral("tabOption"));
    if (pages == nullptr)
        return;

    for (int i = 0; i < pages->count(); ++i)
    {
        QWidget *page = pages->widget(i);
        if (page == nullptr)
            continue;

        // §5.6: 28 px side padding and 22 px between groups on scrollable pages.
        for (QScrollArea *area : page->findChildren<QScrollArea *>())
        {
            if ((area->widget() != nullptr) && (area->widget()->layout() != nullptr))
            {
                area->widget()->layout()->setContentsMargins(28, 0, 28, 28);
                area->widget()->layout()->setSpacing(22);
                area->widget()->setMaximumHeight(QWIDGETSIZE_MAX);
                QSizePolicy policy = area->widget()->sizePolicy();
                policy.setVerticalPolicy(QSizePolicy::Preferred);
                area->widget()->setSizePolicy(policy);
            }
        }

        QList<QGroupBox *> boxes = page->findChildren<QGroupBox *>();
        std::sort(boxes.begin(), boxes.end(), [page](QGroupBox *a, QGroupBox *b)
        {
            return widgetDepth(a, page) > widgetDepth(b, page);
        });
        for (QGroupBox *gb : boxes)
            wrapGroupBox(gb);
    }

    addSchemeSegments(dialog);


}
