/*
 * Torrenter design tokens - generated from tokens/tokens.json.
 * This is the only place where raw color values live.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>
#include <QString>

namespace ThemeTokens
{
    enum class Mode
    {
        Light,
        Dark
    };

    // QA hook: TORRENTER_QA_LIGHT=1 forces the light palette/QSS without
    // touching the Windows system theme.
    inline Mode currentMode()
    {
#ifdef TORRENTER_QA_HOOKS
        if (qEnvironmentVariableIsSet("TORRENTER_QA_LIGHT"))
            return Mode::Light;
#endif
        return (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)
                ? Mode::Dark : Mode::Light;
    }


    enum class Token
    {
        Window,
        Chrome,
        Surface,
        Raised,
        Overlay,
        Hover,
        Pressed,
        Selected,
        SelectedStrong,
        LineSubtle,
        Line,
        LineStrong,
        Text1,
        Text2,
        Text3,
        Text4,
        Accent,
        AccentHover,
        AccentPress,
        OnAccent,
        AccentText,
        Track,
        StDown,
        StSeed,
        StDone,
        StStalled,
        StPaused,
        StQueued,
        StCheck,
        StError,
        AccentSoft,
        AccentLine,
        Focus,
        OnDanger,
        TooltipBg,
        TooltipText,
        DialogField,
        Inkcheck,
        Inkdone,
        Inkdown,
        Inkerror,
        Inkpaused,
        Inkqueued,
        Inkseed,
        Inkstalled,
        AlternateRow,
    };

    QColor color(Token token, Mode mode);
    QColor colorById(const QString &id, Mode mode);
    QString processStyleSheet(Mode mode);

    inline constexpr int rowHeight = 36;
    inline constexpr int rowCompactHeight = 28;
    inline constexpr int sidebarRowHeight = 30;
    inline constexpr int sidebarWidth = 236;
    inline constexpr int radiusDialog = 14;
    inline constexpr int radiusPill = 999;
    inline constexpr int durationMicro = 120;
    inline constexpr int durationSnappy = 220;
    inline constexpr int durationSmooth = 300;
}
