/*
 * Torrenter native Windows title bar integration.
 * Keeps the native frame (snap, accessibility, DPI) and only tints it.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "windowsthemehelper.h"

#ifdef Q_OS_WIN

#include <dwmapi.h>
#include <windows.h>

#include <QApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QString>
#include <QStyleHints>
#include <QTimer>

#include "base/logger.h"
#include <QWidget>

#include "themetokens.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
    class ThemeEventFilter final : public QObject
    {
    public:
        bool installed = false;
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (QWidget *widget = qobject_cast<QWidget *>(watched))
            {
                if ((event->type() == QEvent::Show) || (event->type() == QEvent::WinIdChange))
                {
                    WindowsThemeHelper::apply(widget);
                    // Qt finalizes native window attributes after Show; reapply on the next turn.
                    QTimer::singleShot(0, widget, [widget] { WindowsThemeHelper::apply(widget); });
                }
            }
            return QObject::eventFilter(watched, event);
        }
    };

    ThemeEventFilter *filterInstance()
    {
        static ThemeEventFilter *filter = new ThemeEventFilter;
        return filter;
    }
}

namespace WindowsThemeHelper
{
    void apply(QWidget *window)
    {
        if (!window || !window->isWindow() || !window->windowHandle())
            return;

        const HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (!hwnd)
            return;

        const auto mode = ThemeTokens::currentMode();

        const BOOL darkMode = (mode == ThemeTokens::Mode::Dark) ? TRUE : FALSE;
        HRESULT darkResult = ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        if (FAILED(darkResult))
        {
            // Windows 10 1809 compatibility.
            darkResult = ::DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));
        }

        const QColor chrome = ThemeTokens::color(ThemeTokens::Token::Chrome, mode);
        const COLORREF captionColor = RGB(chrome.red(), chrome.green(), chrome.blue());
        ::DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));

        const QColor text = ThemeTokens::color(ThemeTokens::Token::Text2, mode);
        const COLORREF textColor = RGB(text.red(), text.green(), text.blue());
        ::DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));

        const QColor border = ThemeTokens::color(ThemeTokens::Token::LineSubtle, mode);
        const COLORREF borderColor = RGB(border.red(), border.green(), border.blue());
        ::DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));

        const DWM_WINDOW_CORNER_PREFERENCE corners = qobject_cast<QMenu *>(window)
                ? DWMWCP_ROUNDSMALL : DWMWCP_ROUND;
        ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    }

    void applyAll()
    {
        for (QWidget *window : QApplication::topLevelWidgets())
            apply(window);
    }

    void install()
    {
        ThemeEventFilter *filter = filterInstance();
        filter->setParent(qApp);
        if (!filter->installed)
        {
            qApp->installEventFilter(filter);
            filter->installed = true;
        }
    }
}

#endif
