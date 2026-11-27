/*
 * Torrenter native Windows title bar integration.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <QObject>

class QWidget;

namespace WindowsThemeHelper
{
    void install();
    void apply(QWidget *window);
    void applyAll();
}

#endif
