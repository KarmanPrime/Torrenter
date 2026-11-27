/*
 * Torrenter Options dialog - structural layout helper (Phase A / mockup 05).
 * Rebuilds Designer group boxes as real "title outside + card + rows" structures
 * without changing any widget object name or signal connection.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

class QWidget;

namespace OptionsLayoutHelper
{
    void apply(QWidget *dialog);
}
