/*
 * Torrenter's monochrome icon set: Lucide-style line glyphs tinted at paint time.
 * No color is baked into the SVG templates (src/icons/LICENSE-lucide.txt).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

#include "themetokens.h"

namespace TorrenterIcons
{
    // Build a QIcon from a monochrome Lucide-style glyph, tinted from a design token.
    // The engine is QIcon::Mode/State aware:
    //   Normal -> token (Text2 by default), Active (hover) -> Text1,
    //   Selected or State::On (active) -> Accent, Disabled -> Text4.
    QIcon icon(const QString &id,
               ThemeTokens::Token token = ThemeTokens::Token::Text2,
               ThemeTokens::Mode mode = ThemeTokens::Mode::Light);

    // Convenience overload: tint every mode with one explicit color.
    QIcon icon(const QString &id, const QColor &color);

    // True when the id resolves to a real glyph (i.e. no generic-dot fallback).
    bool hasIcon(const QString &id);
}
