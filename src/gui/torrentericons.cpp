/*
 * Torrenter's monochrome icon set: Lucide-style line glyphs tinted at paint time.
 * No color is baked into the SVG templates (src/icons/LICENSE-lucide.txt).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "torrentericons.h"

#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QSvgRenderer>
#include <QSize>
#include <QSizeF>

#include <cmath>
#include <memory>
#include <array>

using namespace Qt::Literals::StringLiterals;

namespace
{
    QByteArray svg(const char *body)
    {
        return QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" "
                          "stroke=\"%COLOR%\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">")
                + QByteArray(body) + QByteArray("</svg>");
    }

    struct IconPalette
    {
        QColor normal;   // Text2 by default
        QColor hover;    // Text1
        QColor active;   // Accent
        QColor disabled; // Text4
    };

    IconPalette paletteForToken(const ThemeTokens::Token token, const ThemeTokens::Mode mode)
    {
        return {ThemeTokens::color(token, mode),
                ThemeTokens::color(ThemeTokens::Token::Text1, mode),
                ThemeTokens::color(ThemeTokens::Token::Accent, mode),
                ThemeTokens::color(ThemeTokens::Token::Text4, mode)};
    }

    // QIconEngine is required explicitly (QIcon does not pull it in on all Qt builds).
    class TintedIconEngine final : public QIconEngine
    {
    public:
        TintedIconEngine(QByteArray content, const IconPalette &palette)
            : m_content(std::move(content)), m_palette(palette)
        {
        }


        void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
        {
            const QByteArray color = colorFor(mode, state).name(QColor::HexRgb).toLatin1();
            const int modeIndex = (mode == QIcon::Disabled) ? 3
                : (mode == QIcon::Selected) ? 2 : (mode == QIcon::Active) ? 1 : 0;
            const int slot = modeIndex * 2 + ((state == QIcon::On) ? 1 : 0);
            if (!m_renderers[slot])
            {
                QByteArray content = m_content;
                content.replace("%COLOR%", color);
                m_renderers[slot] = std::make_unique<QSvgRenderer>(content);
            }
            m_renderers[slot]->render(painter, QRectF(rect));
        }

        // QIconEngine::pixmap() does not initialize its backing QPixmap before
        // calling paint(). Render onto a transparent buffer ourselves so icons
        // requested through QIcon::pixmap() have no uninitialized RGB noise.
        QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
        {
            return scaledPixmap(size, mode, state, 1.0);
        }

        QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
        {
            if (size.isEmpty() || !std::isfinite(scale) || scale <= 0)
                return {};

            const QSize pixelSize = (QSizeF(size) * scale).toSize();
            if (pixelSize.isEmpty())
                return {};

            QPixmap pm(pixelSize);
            pm.setDevicePixelRatio(scale);
            pm.fill(Qt::transparent);

            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            paint(&p, QRect(QPoint(0, 0), size), mode, state);
            return pm;
        }

        QIconEngine *clone() const override { return new TintedIconEngine(m_content, m_palette); }

        QString key() const override
        {
            return u"torrenter-%1-%2-%3-%4-%5"_s
                    .arg(static_cast<uint>(m_palette.normal.rgba()))
                    .arg(static_cast<uint>(m_palette.hover.rgba()))
                    .arg(static_cast<uint>(m_palette.active.rgba()))
                    .arg(static_cast<uint>(m_palette.disabled.rgba()))
                    .arg(QString::fromLatin1(m_content));
        }

        QList<QSize> availableSizes(QIcon::Mode, QIcon::State) override
        {
            return {QSize(14, 14), QSize(16, 16), QSize(18, 18), QSize(20, 20), QSize(24, 24), QSize(32, 32)};
        }

    private:
        QColor colorFor(const QIcon::Mode mode, const QIcon::State state) const
        {
            if (mode == QIcon::Disabled)
                return m_palette.disabled;
            if ((mode == QIcon::Selected) || (state == QIcon::On))
                return m_palette.active;
            if (mode == QIcon::Active)
                return m_palette.hover;
            return m_palette.normal;
        }

        QByteArray m_content;
        IconPalette m_palette;
        mutable std::array<std::unique_ptr<QSvgRenderer>, 8> m_renderers;
    };

    QIcon tinted(const QByteArray &content, const IconPalette &palette)
    {
        return QIcon(new TintedIconEngine(content, palette));
    }

    const QHash<QString, QByteArray> &templates()
    {
        static const QHash<QString, QByteArray> map {
            // Toolbar and actions
            {u"list-add"_s, svg(R"SVG(<path d="M5 12h14"/><path d="M12 5v14"/>)SVG")},
            {u"list-remove"_s, svg(R"SVG(<path d="M3 6h18"/><path d="M8 6V4h8v2"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/>)SVG")},
            {u"edit-clear"_s, svg(R"SVG(<path d="M3 6h18"/><path d="M8 6V4h8v2"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/>)SVG")},
            {u"edit-copy"_s, svg(R"SVG(<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"/>)SVG")},
            {u"edit-rename"_s, svg(R"SVG(<path d="M12 20h9"/><path d="M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4Z"/>)SVG")},
            {u"torrent-start"_s, svg(R"SVG(<path d="M6 3 20 12 6 21V3Z"/>)SVG")},
            {u"torrent-stop"_s, svg(R"SVG(<rect x="6" y="4" width="4" height="16" rx="1"/><rect x="14" y="4" width="4" height="16" rx="1"/>)SVG")},
            {u"torrent-start-forced"_s, svg(R"SVG(<path d="m3 4 9 8-9 8V4Z"/><path d="m13 4 9 8-9 8V4Z"/>)SVG")},
            {u"pause-session"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M9.5 9v6"/><path d="M14.5 9v6"/>)SVG")},
            {u"directory"_s, svg(R"SVG(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V7Z"/><path d="M3 11h18"/>)SVG")},
            {u"folder-new"_s, svg(R"SVG(<path d="M3 7h6l2 2h10v10H3V7Z"/><path d="M12 12v5"/><path d="M9.5 14.5h5"/>)SVG")},
            {u"folder-documents"_s, svg(R"SVG(<path d="M3 7h6l2 2h10v10H3V7Z"/><path d="M8 13h8"/><path d="M8 16h5"/>)SVG")},
            {u"folder-remote"_s, svg(R"SVG(<path d="M3 7h6l2 2h10v10H3V7Z"/><path d="M12 12a3 3 0 1 0 3 3"/>)SVG")},
            {u"insert-link"_s, svg(R"SVG(<path d="M9 17H7a5 5 0 0 1 0-10h2"/><path d="M15 7h2a5 5 0 0 1 0 10h-2"/><path d="M8 12h8"/>)SVG")},
            {u"application-url"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M2 12h20"/><path d="M12 2a15.3 15.3 0 0 1 0 20"/><path d="M12 2a15.3 15.3 0 0 0 0 20"/>)SVG")},
            {u"view-refresh"_s, svg(R"SVG(<path d="M21 12a9 9 0 0 0-9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"/><path d="M3 3v5h5"/><path d="M3 12a9 9 0 0 0 9 9 9.75 9.75 0 0 0 6.74-2.74L21 16"/><path d="M16 21h5v-5"/>)SVG")},
            {u"loading"_s, svg(R"SVG(<path d="M21 12a9 9 0 1 1-6.219-8.56"/>)SVG")},
            {u"edit-find"_s, svg(R"SVG(<circle cx="11" cy="11" r="8"/><path d="m21 21-4.3-4.3"/>)SVG")},
            {u"configure"_s, svg(R"SVG(<path d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2Z"/><circle cx="12" cy="12" r="3"/>)SVG")},
            {u"download"_s, svg(R"SVG(<path d="M12 3v14"/><path d="m6 11 6 6 6-6"/><path d="M4 21h16"/>)SVG")},
            {u"downloading"_s, svg(R"SVG(<path d="M12 3v14"/><path d="m6 11 6 6 6-6"/><path d="M4 21h16"/>)SVG")},
            {u"upload"_s, svg(R"SVG(<path d="M12 21V3"/><path d="m6 9 6-6 6 6"/><path d="M4 21h16"/>)SVG")},
            {u"go-up"_s, svg(R"SVG(<path d="m6 15 6-6 6 6"/>)SVG")},
            {u"go-down"_s, svg(R"SVG(<path d="m6 9 6 6 6-6"/>)SVG")},
            {u"go-top"_s, svg(R"SVG(<path d="m7 11 5-5 5 5"/><path d="m7 18 5-5 5 5"/>)SVG")},
            {u"go-bottom"_s, svg(R"SVG(<path d="m7 6 5 5 5-5"/><path d="m7 13 5 5 5-5"/>)SVG")},
            {u"task-complete"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="m8 12 3 3 5-6"/>)SVG")},
            {u"checked-completed"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="m8 12 3 3 5-6"/>)SVG")},
            {u"task-reject"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="m15 9-6 6"/><path d="m9 9 6 6"/>)SVG")},

            // Status / state
            {u"dialog-warning"_s, svg(R"SVG(<path d="M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0Z"/><path d="M12 9v4"/><path d="M12 17h.01"/>)SVG")},
            {u"tracker-warning"_s, svg(R"SVG(<path d="M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0Z"/><path d="M12 9v4"/><path d="M12 17h.01"/>)SVG")},
            {u"error"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M12 8v4"/><path d="M12 16h.01"/>)SVG")},
            {u"tracker-error"_s, svg(R"SVG(<path d="m7.86 2 8.28 0L22 7.86v8.28L16.14 22H7.86L2 16.14V7.86L7.86 2Z"/><path d="m15 9-6 6"/><path d="m9 9 6 6"/>)SVG")},
            {u"stopped"_s, svg(R"SVG(<rect x="6" y="4" width="4" height="16" rx="1"/><rect x="14" y="4" width="4" height="16" rx="1"/>)SVG")},
            {u"queued"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M12 6v6l4 2"/>)SVG")},
            {u"stalledDL"_s, svg(R"SVG(<path d="M12 3v14"/><path d="m6 11 6 6 6-6"/><path d="M4 21h16"/>)SVG")},
            {u"stalledUP"_s, svg(R"SVG(<path d="M12 21V3"/><path d="m6 9 6-6 6 6"/><path d="M4 21h16"/>)SVG")},
            {u"force-recheck"_s, svg(R"SVG(<path d="M21 12a9 9 0 0 0-9-9 9.75 9.75 0 0 0-6.74 2.74L3 8"/><path d="M3 3v5h5"/><path d="M3 12a9 9 0 0 0 9 9 9.75 9.75 0 0 0 6.74-2.74L21 16"/><path d="M16 21h5v-5"/>)SVG")},
            {u"reannounce"_s, svg(R"SVG(<path d="M20 12a8 8 0 1 0-2 6"/><path d="M20 4v7h-7"/><path d="M12 8v4l3 2"/>)SVG")},
            {u"set-location"_s, svg(R"SVG(<path d="M3 7h6l2 2h10v10H3V7Z"/><path d="M13 13h6"/><path d="m16 10 3 3-3 3"/>)SVG")},
            {u"trackerless"_s, svg(R"SVG(<circle cx="12" cy="12" r="10" stroke-dasharray="4 4"/>)SVG")},

            // Status-bar filters (mockup 11-icons.png)
            {u"panel-left"_s, svg(R"SVG(<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M9 4v16"/><path d="m14 9-3 3 3 3"/>)SVG")},
            {u"more-horizontal"_s, svg(R"SVG(<circle cx="5" cy="12" r="1.4" fill="%COLOR%" stroke="none"/><circle cx="12" cy="12" r="1.4" fill="%COLOR%" stroke="none"/><circle cx="19" cy="12" r="1.4" fill="%COLOR%" stroke="none"/>)SVG")},
            {u"filter-all"_s, svg(R"SVG(<path d="m12.83 2.18a2 2 0 0 0-1.66 0L2.6 6.08a1 1 0 0 0 0 1.83l8.58 3.91a2 2 0 0 0 1.66 0l8.58-3.9a1 1 0 0 0 0-1.83Z"/><path d="m22 12-9.17 4.91a2 2 0 0 1-1.66 0L2 12"/><path d="m22 17-9.17 4.91a2 2 0 0 1-1.66 0L2 17"/>)SVG")},
            {u"filter-active"_s, svg(R"SVG(<path d="M22 12h-4l-3 9L9 3l-3 9H2"/>)SVG")},
            {u"filter-inactive"_s, svg(R"SVG(<path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z"/>)SVG")},
            {u"filter-stalled"_s, svg(R"SVG(<path d="M5 22h14"/><path d="M5 2h14"/><path d="M17 22v-4a4 4 0 0 0-4-4 4 4 0 0 0-4 4v4"/><path d="M7 2v4a4 4 0 0 0 4 4 4 4 0 0 0 4-4V2"/>)SVG")},

            // Options navigation (mockup 05-preferences.png)
            {u"preferences-desktop"_s, svg(R"SVG(<rect x="2" y="3" width="20" height="14" rx="2"/><path d="M8 21h8"/><path d="M12 17v4"/>)SVG")},
            {u"preferences-bittorrent"_s, svg(R"SVG(<path d="m6 15-4-4 6.75-6.77a7.79 7.79 0 0 1 11 11L13 22l-4-4 6.39-6.36a2.14 2.14 0 0 0-3-3L6 15"/><path d="m5 8 4 4"/><path d="m12 15 4 4"/>)SVG")},
            {u"network-connect"_s, svg(R"SVG(<rect x="16" y="16" width="6" height="6" rx="1"/><rect x="2" y="16" width="6" height="6" rx="1"/><rect x="9" y="2" width="6" height="6" rx="1"/><path d="M5 16v-3a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2v3"/><path d="M12 8v8"/>)SVG")},
            {u"preferences-webui"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10Z"/><path d="M2 12h20"/>)SVG")},
            {u"preferences-advanced"_s, svg(R"SVG(<path d="M21 4h-7M10 4H3"/><path d="M21 12h-9M8 12H3"/><path d="M21 20h-5M13 20H3"/><path d="M14 2v4M8 10v4M16 18v4"/>)SVG")},

            {u"panel-bottom-close"_s, svg(R"SVG(<rect x="3" y="3" width="18" height="14" rx="2"/><path d="M3 15h18"/><path d="m9 9 3 3 3-3"/>)SVG")},
            {u"torrenter-glyph"_s, svg(R"SVG(<g fill="none" stroke="%COLOR%" stroke-width="5.5" stroke-linecap="round" stroke-linejoin="round"><path d="M15 22c5.7-5.4 11.3-5.4 17 0s11.3 5.4 17 0"/><path d="M32 26v20"/><path d="M24 39l8 8 8-8"/></g>)SVG")},
            {u"torrenter-icon"_s, svg(R"SVG(<rect x="3" y="3" width="18" height="18" rx="4"/><path d="M12 7v8"/><path d="m8 11 4 4 4-4"/>)SVG")},
            {u"check-check"_s, svg(R"SVG(<path d="M18 6 7 17l-4-4"/><path d="m22 10-7.5 7.5L13 16"/>)SVG")},
            {u"check"_s, svg(R"SVG(<path d="M20 6 9 17l-5-5"/>)SVG")},
            {u"minus"_s, svg(R"SVG(<path d="M5 12h14"/>)SVG")},
            {u"folder-open"_s, svg(R"SVG(<path d="m6 14 1.5-2h13L22 16v4H4v-4l2-2Z"/><path d="M4 16h18"/>)SVG")},
            {u"square-dashed"_s, svg(R"SVG(<path d="M4 4h4M12 4h4M20 4v4M20 12v4M20 20h-4M12 20H8M4 20v-4M4 12V8"/>)SVG")},
            {u"chevrons-up-down"_s, svg(R"SVG(<path d="m7 15 5 5 5-5"/><path d="m7 9 5-5 5 5"/>)SVG")},
            {u"chevron-right"_s, svg(R"SVG(<path d="m9 18 6-6-6-6"/>)SVG")},
            {u"chevron-down"_s, svg(R"SVG(<path d="m6 9 6 6 6-6"/>)SVG")},
            {u"chevron-up"_s, svg(R"SVG(<path d="m6 15 6-6 6 6"/>)SVG")},
            // Panels, trackers, misc
            {u"view-categories"_s, svg(R"SVG(<path d="M3 6a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6Z"/>)SVG")},
            {u"tags"_s, svg(R"SVG(<path d="M3 12V4h8l10 10-8 8L3 12Z"/><circle cx="7.5" cy="7.5" r="1"/>)SVG")},
            {u"name"_s, svg(R"SVG(<path d="M4 6h16M4 12h12M4 18h8"/>)SVG")},
            {u"hash"_s, svg(R"SVG(<path d="M4 9h16M4 15h16M10 3 8 21M16 3l-2 18"/>)SVG")},
            {u"ratio"_s, svg(R"SVG(<path d="m16 16 3-8 3 8c-.87.65-1.92 1-3 1s-2.13-.35-3-1Z"/><path d="m2 16 3-8 3 8c-.87.65-1.92 1-3 1s-2.13-.35-3-1Z"/><path d="M7 21h10"/><path d="M12 3v18"/><path d="M3 7h2c2 0 5-1 7-2 2 1 5 2 7 2h2"/>)SVG")},
            {u"trackers"_s, svg(R"SVG(<path d="M4.9 4.9a10 10 0 0 0 0 14.2"/><path d="M7.8 7.8a6 6 0 0 0 0 8.4"/><path d="m9 20 3-6 3 6"/><path d="M16.2 16.2a6 6 0 0 0 0-8.4"/><path d="M19.1 19.1a10 10 0 0 0 0-14.2"/><circle cx="12" cy="12" r="2"/>)SVG")},
            {u"peers"_s, svg(R"SVG(<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M22 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/>)SVG")},
            {u"peers-add"_s, svg(R"SVG(<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M19 8v6"/><path d="M16 11h6"/>)SVG")},
            {u"peers-remove"_s, svg(R"SVG(<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M17 11h6"/>)SVG")},
            {u"ip-blocked"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="m4.9 4.9 14.2 14.2"/>)SVG")},
            {u"network-server"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10Z"/><path d="M2 12h20"/>)SVG")},
            {u"speedometer"_s, svg(R"SVG(<path d="M3.5 17.5a9 9 0 1 1 17 0"/><path d="M12 6.5v1.5"/><path d="M6.3 9.3l1.1 1.1"/><path d="M17.7 9.3l-1.1 1.1"/><path d="M12 15l4-5"/><circle cx="12" cy="15" r="1.6"/>)SVG")},
            {u"chart-line"_s, svg(R"SVG(<path d="M4 20V4"/><path d="M4 20h16"/><path d="m7 15 4-5 3 3 5-7"/>)SVG")},
            {u"view-statistics"_s, svg(R"SVG(<path d="M3 3v16a2 2 0 0 0 2 2h16"/><path d="M7 16v-4"/><path d="M12 16V7"/><path d="M17 16v-7"/>)SVG")},
            {u"object-locked"_s, svg(R"SVG(<rect x="4" y="10" width="16" height="12" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/>)SVG")},
            {u"view-preview"_s, svg(R"SVG(<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7-10-7-10-7Z"/><circle cx="12" cy="12" r="3"/>)SVG")},
            {u"plugins"_s, svg(R"SVG(<path d="M10 3 4 9l6 6 6-6-6-6Z"/><path d="m18 11-4 4 4 4 4-4-4-4Z"/><path d="m7 18 3 3"/>)SVG")},
            {u"help-contents"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M9.1 9a3 3 0 0 1 5.8 1c0 2-3 3-3 3"/><path d="M12 17h.01"/>)SVG")},
            {u"help-about"_s, svg(R"SVG(<circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/>)SVG")},
            {u"application-exit"_s, svg(R"SVG(<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><path d="m16 17 5-5-5-5"/><path d="M21 12H9"/>)SVG")},
            {u"application-rss"_s, svg(R"SVG(<path d="M4 11a9 9 0 0 1 9 9"/><path d="M4 4a16 16 0 0 1 16 16"/><circle cx="5" cy="19" r="1"/>)SVG")},
            {u"rss_read_article"_s, svg(R"SVG(<path d="M4 11a9 9 0 0 1 9 9"/><path d="M4 4a16 16 0 0 1 16 16"/><circle cx="5" cy="19" r="1"/><path d="M9 7h11"/><path d="M9 11h11"/>)SVG")},
            {u"rss_unread_article"_s, svg(R"SVG(<path d="M4 11a9 9 0 0 1 9 9"/><path d="M4 4a16 16 0 0 1 16 16"/><circle cx="5" cy="19" r="1"/><path d="M9 7h11"/><path d="M9 11h7"/>)SVG")},
            {u"browser-cookies"_s, svg(R"SVG(<path d="M12 2a10 10 0 1 0 10 10 4 4 0 0 1-5-5 4 4 0 0 1-5-5Z"/><path d="M8.5 9h.01"/><path d="M13 8h.01"/><path d="M8.5 14h.01"/><path d="M15 14h.01"/>)SVG")},
            {u"mail-inbox"_s, svg(R"SVG(<path d="M22 12h-6l-2 3h-4l-2-3H2"/><path d="M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11Z"/>)SVG")},
            {u"firewalled"_s, svg(R"SVG(<path d="M20 13c0 5-3.5 7.5-8 9-4.5-1.5-8-4-8-9V5l8-3 8 3v8Z"/><path d="M12 8v4"/><path d="M12 16h.01"/>)SVG")},
            {u"connected"_s, svg(R"SVG(<path d="M2 8.8a15 15 0 0 1 20 0"/><path d="M5 13a10 10 0 0 1 14 0"/><path d="M8.5 16.5a5 5 0 0 1 7 0"/><path d="M12 20h.01"/>)SVG")},
            {u"disconnected"_s, svg(R"SVG(<path d="m2 2 20 20"/><path d="M2 8.8a15 15 0 0 1 6.4-3.9"/><path d="M15.6 4.9A15 15 0 0 1 22 8.8"/><path d="M5 13a10 10 0 0 1 2.6-1.8"/><path d="M16.4 11.2A10 10 0 0 1 19 13"/><path d="M8.5 16.5a5 5 0 0 1 7 0"/><path d="M12 20h.01"/>)SVG")},
            {u"wallet-open"_s, svg(R"SVG(<rect x="3" y="6" width="18" height="14" rx="2"/><path d="M3 10h18"/><path d="M16 15h2"/>)SVG")},
            {u"create-torrent"_s, svg(R"SVG(<path d="M13 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h4"/><path d="M13 3l6 6"/><path d="M13 3v4a2 2 0 0 0 2 2h4v3"/><path d="M18 15v6"/><path d="M15 18h6"/>)SVG")},
            {u"torrent-creator"_s, svg(R"SVG(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h9"/><path d="M14 2v6h6"/><path d="M12 18v-6"/><path d="M9 15h6"/>)SVG")},
            {u"torrent-magnet"_s, svg(R"SVG(<path d="m6 15-4-4 6.75-6.77a7.79 7.79 0 0 1 11 11L13 22l-4-4 6.39-6.36a2.14 2.14 0 0 0-3-3L6 15"/><path d="m5 8 4 4"/><path d="m12 15 4 4"/>)SVG")},
            {u"slow"_s, svg(R"SVG(<path d="M6 9a5 5 0 0 1 10 0v5a5 5 0 0 1-10 0V9Z"/><path d="M16 13h4"/><path d="M18 9l2-3"/><path d="M20 16h2"/>)SVG")},
            {u"slow_off"_s, svg(R"SVG(<path d="M6 9a5 5 0 0 1 10 0v5a5 5 0 0 1-10 0V9Z"/><path d="M16 13h4"/><path d="M18 9l2-3"/><path d="M20 16h2"/><path d="M3 3l18 18"/>)SVG")},

            // Transfer-list file-type glyphs (design 01-main-dark / 07-empty-state)
            {u"disc-3"_s, svg(R"SVG(<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="3"/><path d="M12 3v2"/><path d="M12 19v2"/><path d="M3 12h2"/><path d="M19 12h2"/>)SVG")},
            {u"package"_s, svg(R"SVG(<path d="m12 2 9 5v10l-9 5-9-5V7l9-5Z"/><path d="m3 7 9 5 9-5"/><path d="M12 12v10"/><path d="m7.5 4.5 9 5"/>)SVG")},
            {u"file-archive"_s, svg(R"SVG(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6Z"/><path d="M14 2v6h6"/><rect x="9" y="12" width="6" height="5" rx="1"/><path d="M12 12v5"/>)SVG")},
            {u"file-video"_s, svg(R"SVG(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6Z"/><path d="M14 2v6h6"/><path d="m10 11 5 3-5 3v-6Z"/>)SVG")},
            {u"music"_s, svg(R"SVG(<path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/>)SVG")},
            {u"cpu"_s, svg(R"SVG(<rect x="5" y="5" width="14" height="14" rx="2"/><rect x="9" y="9" width="6" height="6" rx="1"/><path d="M9 2v3"/><path d="M15 2v3"/><path d="M9 19v3"/><path d="M15 19v3"/><path d="M2 9h3"/><path d="M2 15h3"/><path d="M19 9h3"/><path d="M19 15h3"/>)SVG")},
            {u"database"_s, svg(R"SVG(<ellipse cx="12" cy="5" rx="9" ry="3"/><path d="M3 5v14c0 1.66 4.03 3 9 3s9-1.34 9-3V5"/><path d="M3 12c0 1.66 4.03 3 9 3s9-1.34 9-3"/>)SVG")},
            {u"file-text"_s, svg(R"SVG(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6Z"/><path d="M14 2v6h6"/><path d="M8 13h8"/><path d="M8 17h8"/><path d="M8 9h2"/>)SVG")},
            {u"folder"_s, svg(R"SVG(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V7Z"/><path d="M3 11h18"/>)SVG")},
            {u"file"_s, svg(R"SVG(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6Z"/><path d="M14 2v6h6"/>)SVG")},
            {u"x"_s, svg(R"SVG(<path d="M18 6 6 18"/><path d="m6 6 12 12"/>)SVG")},
            {u"file-down"_s, svg(R"SVG(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6Z"/><path d="M14 2v6h6"/><path d="M12 18v-6"/><path d="m9 15 3 3 3-3"/>)SVG")},
            {u"magnet"_s, svg(R"SVG(<path d="m6 15-4-4 6.75-6.77a7.79 7.79 0 0 1 11 0L20 11l-4 4"/><path d="m13 5 2 2"/><path d="M5 15v4a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-4"/>)SVG")},
        };
        return map;
    }
}

QIcon TorrenterIcons::icon(const QString &id, const ThemeTokens::Token token, const ThemeTokens::Mode mode)
{
    const auto &map = templates();
    const auto it = map.constFind(id);
    if (it == map.constEnd())
        return {}; // no generic-dot fallback: app/tray art is left to the theme source
    return tinted(it.value(), paletteForToken(token, mode));
}

QIcon TorrenterIcons::icon(const QString &id, const QColor &color)
{
    const auto &map = templates();
    const auto it = map.constFind(id);
    if (it == map.constEnd())
        return {};
    const IconPalette flat {color, color, color, color};
    return tinted(it.value(), flat);
}

bool TorrenterIcons::hasIcon(const QString &id)
{
    return templates().contains(id);
}
