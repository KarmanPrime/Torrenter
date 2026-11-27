/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021 Prince Gupta <guptaprince8832@gmail.com>
 * Copyright (C) 2015 Anton Lashkov <lenton_91@mail.ru>
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

#include "speedplotview.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QLocale>
#include <QToolTip>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/unicodestrings.h"
#include "base/utils/misc.h"
#include "gui/themetokens.h"

namespace
{
    // table of supposed nice steps for grid marks to get nice looking quarters of scale
    const qreal roundingTable[] = {1.2, 1.6, 2, 2.4, 2.8, 3.2, 4, 6, 8};

    struct SplitValue
    {
        qreal arg = 0;
        Utils::Misc::SizeUnit unit {};
        qlonglong sizeInBytes() const
        {
            return Utils::Misc::sizeInBytes(arg, unit);
        }
    };

    SplitValue getRoundedYScale(qreal value)
    {
        using Utils::Misc::SizeUnit;

        if (value == 0.0) return {0, SizeUnit::Byte};
        if (value <= 12.0) return {12, SizeUnit::Byte};

        SizeUnit calculatedUnit = SizeUnit::Byte;
        while (value > 1024)
        {
            value /= 1024;
            calculatedUnit = static_cast<SizeUnit>(static_cast<int>(calculatedUnit) + 1);
        }

        if (value > 100)
        {
            const qreal roundedValue {std::ceil(value / 40) * 40};
            return {roundedValue, calculatedUnit};
        }

        if (value > 10)
        {
            const qreal roundedValue {std::ceil(value / 4) * 4};
            return {roundedValue, calculatedUnit};
        }

        for (const auto &roundedValue : roundingTable)
        {
            if (value <= roundedValue)
                return {roundedValue, calculatedUnit};
        }
        return {10.0, calculatedUnit};
    }

    QString formatLabel(const qreal argValue, const Utils::Misc::SizeUnit unit)
    {
        // check is there need for digits after decimal separator
        const int precision = (argValue < 10) ? friendlyUnitPrecision(unit) : 0;
        return QLocale::system().toString(argValue, 'f', precision)
               + QChar::Nbsp + unitString(unit, true);
    }
}

SpeedPlotView::Averager::Averager(const milliseconds duration, const milliseconds resolution)
    : m_resolution {resolution}
    , m_maxDuration {duration}
    , m_sink {static_cast<DataCircularBuffer::size_type>(duration / resolution)}
{
    m_lastSampleTime.start();
}

bool SpeedPlotView::Averager::push(const SampleData &sampleData)
{
    // Accumulator overflow will be hit in worst case on longest used averaging span,
    // defined by resolution. Maximum resolution is 144 seconds
    // Using int32 for accumulator we get overflow when transfer speed reaches 2^31/144 ~~ 14.2 MBytes/s.
    // With quint64 this speed limit is 2^64/144 ~~ 114 PBytes/s.
    // This speed is inaccessible to an ordinary user.
    ++m_counter;
    for (int id = UP; id < NB_GRAPHS; ++id)
        m_accumulator[id] += sampleData[id];

    // system may go to sleep, that can cause very big elapsed interval
    const milliseconds updateInterval {static_cast<int64_t>(BitTorrent::Session::instance()->refreshInterval() * 1.25)};
    const milliseconds maxElapsed {std::max(updateInterval, m_resolution)};
    const milliseconds elapsed {std::min(milliseconds {m_lastSampleTime.elapsed()}, maxElapsed)};
    if (elapsed < m_resolution)
        return false; // still accumulating

    // it is time final averaging calculations
    for (int id = UP; id < NB_GRAPHS; ++id)
        m_accumulator[id] /= m_counter;

    m_currentDuration += elapsed;

    // remove extra data from front if we reached max duration
    if (m_currentDuration > m_maxDuration)
    {
        // once we go above the max duration never go below that
        // otherwise it will cause empty space in graphs
        while (!m_sink.empty()
               && ((m_currentDuration - m_sink.front().duration) >= m_maxDuration))
        {
            m_currentDuration -= m_sink.front().duration;
            m_sink.pop_front();
        }
    }

    // now flush out averaged data
    Q_ASSERT(m_sink.size() < m_sink.capacity());
    m_sink.push_back({elapsed, m_accumulator});

    // reset
    m_accumulator = {};
    m_counter = 0;
    m_lastSampleTime.restart();
    return true;
}

void SpeedPlotView::Averager::seedForQA(const SampleData &sampleData, const int count)
{
    for (int i = 0; i < count; ++i)
        m_sink.push_back({m_resolution, sampleData});
    m_currentDuration = m_resolution * count;
    m_lastSampleTime.restart();
}

const SpeedPlotView::DataCircularBuffer &SpeedPlotView::Averager::data() const
{
    return m_sink;
}

SpeedPlotView::SpeedPlotView(QWidget *parent)
    : QGraphicsView {parent}
{
    const auto mode = ThemeTokens::currentMode();
    const auto tokenColor = [mode](const ThemeTokens::Token token) { return ThemeTokens::color(token, mode); };
    const auto makePen = [](const QColor &color, Qt::PenStyle style = Qt::SolidLine)
    {
        QPen pen {color};
        pen.setWidthF(2);
        pen.setStyle(style);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        if (style == Qt::DashLine)
            pen.setDashPattern(QVector<qreal> {2, 4});
        return pen;
    };

    const QColor down = tokenColor(ThemeTokens::Token::StDown);
    const QColor seed = tokenColor(ThemeTokens::Token::StSeed);
    const QColor check = tokenColor(ThemeTokens::Token::StCheck);
    const QColor stalled = tokenColor(ThemeTokens::Token::StStalled);
    const QColor neutral = tokenColor(ThemeTokens::Token::Text2);
    const QColor error = tokenColor(ThemeTokens::Token::StError);

    m_properties[UP] = GraphProperties(tr("Total Upload"), makePen(seed));
    m_properties[DOWN] = GraphProperties(tr("Total Download"), makePen(down));
    m_properties[PAYLOAD_UP] = GraphProperties(tr("Payload Upload"), makePen(check, Qt::DashLine));
    m_properties[PAYLOAD_DOWN] = GraphProperties(tr("Payload Download"), makePen(check, Qt::DashLine));
    m_properties[OVERHEAD_UP] = GraphProperties(tr("Overhead Upload"), makePen(neutral, Qt::DashLine));
    m_properties[OVERHEAD_DOWN] = GraphProperties(tr("Overhead Download"), makePen(neutral, Qt::DashLine));
    m_properties[DHT_UP] = GraphProperties(tr("DHT Upload"), makePen(stalled, Qt::DashLine));
    m_properties[DHT_DOWN] = GraphProperties(tr("DHT Download"), makePen(stalled, Qt::DashLine));
    m_properties[TRACKER_UP] = GraphProperties(tr("Tracker Upload"), makePen(error, Qt::DashLine));
    m_properties[TRACKER_DOWN] = GraphProperties(tr("Tracker Download"), makePen(error, Qt::DashLine));

    setMouseTracking(true);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_lerpTimer.setInterval(16);
    connect(&m_lerpTimer, &QTimer::timeout, this, &SpeedPlotView::updateLerp);
    seedQAData();
}

void SpeedPlotView::setGraphEnable(GraphID id, bool enable)
{
    m_properties[id].enable = enable;
    viewport()->update();
}

void SpeedPlotView::pushPoint(const SpeedPlotView::SampleData &point)
{
    for (Averager *averager : {&m_averager5Min, &m_averager30Min
                                , &m_averager6Hour, &m_averager12Hour
                                , &m_averager24Hour})
    {
        if (averager->push(point))
        {
            if (m_currentAverager == averager)
            {
                if (isVisible() && !m_lerpTimer.isActive())
                    m_lerpTimer.start();
                viewport()->update();
            }
        }
    }
}

void SpeedPlotView::setPeriod(const TimePeriod period)
{
    switch (period)
    {
    case SpeedPlotView::MIN1:
        m_currentMaxDuration = 1min;
        m_currentAverager = &m_averager5Min;
        break;
    case SpeedPlotView::MIN5:
        m_currentMaxDuration = 5min;
        m_currentAverager = &m_averager5Min;
        break;
    case SpeedPlotView::MIN30:
        m_currentMaxDuration = 30min;
        m_currentAverager = &m_averager30Min;
        break;
    case SpeedPlotView::HOUR3:
        m_currentMaxDuration = 3h;
        m_currentAverager = &m_averager6Hour;
        break;
    case SpeedPlotView::HOUR6:
        m_currentMaxDuration = 6h;
        m_currentAverager = &m_averager6Hour;
        break;
    case SpeedPlotView::HOUR12:
        m_currentMaxDuration = 12h;
        m_currentAverager = &m_averager12Hour;
        break;
    case SpeedPlotView::HOUR24:
        m_currentMaxDuration = 24h;
        m_currentAverager = &m_averager24Hour;
        break;
    }

    if (isVisible())
    {
        if (m_displayMax <= 0)
            m_displayMax = static_cast<qreal>(maxYValue()) * 1.08;
        if (!m_lerpTimer.isActive())
            m_lerpTimer.start();
    }
    viewport()->update();
}

const SpeedPlotView::DataCircularBuffer &SpeedPlotView::currentData() const
{
    return m_currentAverager->data();
}

quint64 SpeedPlotView::maxYValue() const
{
    const DataCircularBuffer &queue = currentData();

    quint64 maxYValue = 0;
    for (int id = UP; id < NB_GRAPHS; ++id)
    {

        if (!m_properties[static_cast<GraphID>(id)].enable)
            continue;

        milliseconds duration {0};
        for (int i = static_cast<int>(queue.size()) - 1; i >= 0; --i)
        {
            maxYValue = std::max(maxYValue, queue[i].data[id]);
            duration += queue[i].duration;
            if (duration >= m_currentMaxDuration)
                break;
        }
    }

    return maxYValue;
}

#ifdef TORRENTER_QA_HOOKS
void SpeedPlotView::seedQAData()
{
    if (!qEnvironmentVariableIsSet("TORRENTER_QA_SPEED_DATA") || m_qaSeeded)
        return;
    m_qaSeeded = true;
    for (int i = 0; i < 300; ++i)
    {
        SampleData sample {};
        const double phase = static_cast<double>(i) / 300.0;
        sample[DOWN] = static_cast<quint64>(std::max(0.2, 7.0 + 5.0 * std::sin(phase * 12.0) + 2.5 * std::sin(phase * 31.0)) * 1024 * 1024);
        sample[UP] = static_cast<quint64>(std::max(0.1, 2.0 + 1.4 * std::sin(phase * 9.0 + 1.0)) * 1024 * 1024);
        sample[PAYLOAD_DOWN] = sample[DOWN] * 0.9;
        sample[PAYLOAD_UP] = sample[UP] * 0.9;
        m_averager5Min.seedForQA(sample, 1);
    }
    m_currentAverager = &m_averager5Min;
    m_currentMaxDuration = 5min;
}
#else
void SpeedPlotView::seedQAData() {}
#endif

void SpeedPlotView::updateLerp()
{
    const qreal target = static_cast<qreal>(maxYValue()) * 1.08;
    if (m_displayMax < target)
        m_displayMax = target; // expand immediately to avoid clipping
    else
        m_displayMax += (target - m_displayMax) * 0.08;
    if (std::abs(target - m_displayMax) < 1.0)
    {
        m_displayMax = target;
        m_lerpTimer.stop();
    }
    viewport()->update();
}

void SpeedPlotView::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor lineColor = ThemeTokens::color(ThemeTokens::Token::Line, ThemeTokens::currentMode());
    const QColor lineStrong = ThemeTokens::color(ThemeTokens::Token::LineStrong, ThemeTokens::currentMode());
    const QColor text3 = ThemeTokens::color(ThemeTokens::Token::Text3, ThemeTokens::currentMode());

    const QRect viewportRect = viewport()->rect();
    QRectF plot = QRectF(viewportRect).adjusted(12, 10, -70, -28);
    if (plot.width() < 40 || plot.height() < 40)
        return;

    if (m_displayMax <= 0)
        m_displayMax = static_cast<qreal>(maxYValue()) * 1.08;
    const SplitValue niceScale = getRoundedYScale(static_cast<qreal>(m_displayMax));
    const qreal yMax = std::max<qreal>(1.0, static_cast<qreal>(niceScale.sizeInBytes()));

    // Horizontal dashed grid lines (2 on, 4 off).
    QPen gridPen {lineColor};
    gridPen.setWidthF(1);
    gridPen.setStyle(Qt::CustomDashLine);
    gridPen.setDashPattern(QVector<qreal> {2, 4});
    painter.setPen(gridPen);
    for (int i = 0; i <= 4; ++i)
    {
        const qreal y = plot.top() + (i * plot.height() / 4.0);
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    // Y labels on the right of the plot.
    QFont labelFont = painter.font();
    labelFont.setPixelSize(11);
    painter.setFont(labelFont);
    painter.setPen(text3);
    for (int i = 0; i <= 4; ++i)
    {
        const QString label = formatLabel(niceScale.arg * (4 - i) / 4.0, niceScale.unit);
        const qreal y = plot.top() + (i * plot.height() / 4.0);
        painter.drawText(QRectF(plot.right() + 6, y - 8, 60, 16), Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    // X labels: relative time underneath.
    const auto relativeTime = [](const milliseconds duration)
    {
        const int seconds = static_cast<int>(duration.count() / 1000);
        if (seconds >= 3600)
            return QString {u"-"_s + QString::number(seconds / 3600) + u":"_s
                + QString::number((seconds % 3600) / 60).rightJustified(2, u'0') + u":"_s
                + QString::number(seconds % 60).rightJustified(2, u'0')};
        return QString {u"-"_s + QString::number(seconds / 60) + u":"_s
            + QString::number(seconds % 60).rightJustified(2, u'0')};
    };
    for (int i = 0; i < 5; ++i)
    {
        const qreal x = plot.left() + (i * plot.width() / 4.0);
        const QString label = (i == 4) ? tr("now") : relativeTime(m_currentMaxDuration * (4 - i) / 4);
        painter.drawText(QRectF(x - 24, plot.bottom() + 6, 48, 16), Qt::AlignHCenter | Qt::AlignVCenter, label);
    }

    const DataCircularBuffer &queue = currentData();
    const milliseconds lastDuration {queue.empty() ? 0ms : queue.back().duration};
    const qreal xTickSize = static_cast<qreal>(plot.width()) / std::max<qreal>(1.0, (m_currentMaxDuration - lastDuration).count());

    // Build chronologically ordered points for each visible series.
    struct Series
    {
        GraphID id;
        QVector<QPointF> points;
        QVector<milliseconds> durations;
        QVector<int> sourceIndexes;
        QPainterPath path;
        QPainterPath fill;
    };
    QList<Series> seriesList;
    for (const int id : {static_cast<int>(DOWN), static_cast<int>(UP), static_cast<int>(PAYLOAD_DOWN),
             static_cast<int>(PAYLOAD_UP), static_cast<int>(OVERHEAD_DOWN), static_cast<int>(OVERHEAD_UP),
             static_cast<int>(DHT_DOWN), static_cast<int>(DHT_UP), static_cast<int>(TRACKER_DOWN), static_cast<int>(TRACKER_UP)})
    {
        const GraphID graphId = static_cast<GraphID>(id);
        if (!m_properties[graphId].enable)
            continue;
        Series series;
        series.id = graphId;
        milliseconds duration {0};
        for (int i = static_cast<int>(queue.size()) - 1; i >= 0; --i)
        {
            const qreal x = plot.right() - (duration.count() * xTickSize);
            const qreal y = plot.bottom() - std::min<qreal>(1.0, static_cast<qreal>(queue[i].data[id]) / yMax) * plot.height();
            series.points.prepend(QPointF(x, y));
            series.durations.prepend(duration);
            series.sourceIndexes.prepend(i);
            duration += queue[i].duration;
            if (duration >= m_currentMaxDuration)
                break;
        }
        if (series.points.size() < 2)
            continue;

        // Catmull-Rom spline converted to cubic Beziers.
        series.path.moveTo(series.points.first());
        for (int i = 0; i < series.points.size() - 1; ++i)
        {
            const QPointF &p0 = series.points.at(std::max(0, i - 1));
            const QPointF &p1 = series.points.at(i);
            const QPointF &p2 = series.points.at(i + 1);
            const QPointF &p3 = series.points.at(std::min(static_cast<int>(series.points.size()) - 1, i + 2));
            series.path.cubicTo(p1 + (p2 - p0) / 6.0, p2 - (p3 - p1) / 6.0, p2);
        }
        series.fill = series.path;
        series.fill.lineTo(series.points.last().x(), plot.bottom());
        series.fill.lineTo(series.points.first().x(), plot.bottom());
        series.fill.closeSubpath();
        seriesList.append(series);
    }

    painter.save();
    painter.setClipRect(plot);
    if (!seriesList.isEmpty())
    {
        const QColor downColor = m_properties[seriesList.first().id].pen.color();
        // Gradient fill under the first visible series, with an 8% left fade-in.
        const int strips = 16;
        const qreal fadeWidth = plot.width() * 0.08;
        const auto drawFadedFill = [&painter, &plot, &downColor, &seriesList](const QRectF &band, const qreal alpha)
        {
            painter.setClipRect(band);
            QColor start = downColor;
            start.setAlphaF(alpha);
            QColor end = downColor;
            end.setAlphaF(0);
            QLinearGradient gradient(QPointF(0, plot.top()), QPointF(0, plot.bottom()));
            gradient.setColorAt(0, start);
            gradient.setColorAt(1, end);
            painter.fillPath(seriesList.first().fill, gradient);
        };
        for (int strip = 0; strip < strips; ++strip)
        {
            const qreal x0 = plot.left() + (fadeWidth * strip / strips);
            const qreal x1 = plot.left() + (fadeWidth * (strip + 1) / strips);
            drawFadedFill(QRectF(x0, plot.top(), x1 - x0, plot.height()), 0.16 * (strip + 1) / strips);
        }
        drawFadedFill(QRectF(plot.left() + fadeWidth, plot.top(),
            plot.right() - plot.left() - fadeWidth, plot.height()), 0.16);
        painter.setClipRect(plot);
    }

    for (const Series &series : seriesList)
    {
        painter.setPen(m_properties[series.id].pen);
        painter.drawPath(series.path);
        if (!series.points.isEmpty())
        {
            const QPointF newest = series.points.last();
            QColor halo = m_properties[series.id].pen.color();
            halo.setAlphaF(0.15);
            painter.setPen(Qt::NoPen);
            painter.setBrush(halo);
            painter.drawEllipse(newest, 4.5, 4.5);
            painter.setBrush(m_properties[series.id].pen.color());
            painter.drawEllipse(newest, 1.75, 1.75);
        }
    }

    // Hover crosshair, ring markers and tooltip.
    bool qaHover = false;
#ifdef TORRENTER_QA_HOOKS
    qaHover = qEnvironmentVariableIsSet("TORRENTER_QA_SPEED_HOVER");
    if (qaHover)
        m_hoverPos = QPointF(plot.left() + plot.width() * 0.55, plot.top() + plot.height() * 0.5);
#endif
    if ((m_hovering || qaHover) && plot.contains(m_hoverPos) && !seriesList.isEmpty())
    {
        painter.setPen(QPen {lineStrong, 1});
        painter.drawLine(QPointF(m_hoverPos.x(), plot.top()), QPointF(m_hoverPos.x(), plot.bottom()));
        const Series &hoverSeries = seriesList.first();
        int index = 0;
        qreal bestDistance = std::abs(hoverSeries.points.at(0).x() - m_hoverPos.x());
        for (int i = 1; i < hoverSeries.points.size(); ++i)
        {
            const qreal distance = std::abs(hoverSeries.points.at(i).x() - m_hoverPos.x());
            if (distance < bestDistance)
            {
                bestDistance = distance;
                index = i;
            }
        }
        QString tooltip = u"<b>"_s + relativeTime(hoverSeries.durations.at(index)) + u"</b>"_s;
        for (const Series &series : seriesList)
        {
            if (index >= series.points.size())
                continue;
            const QPointF marker = series.points.at(index);
            painter.setPen(QPen {m_properties[series.id].pen.color(), 2});
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(marker, 2, 2);
            const int sourceIndex = series.sourceIndexes.at(index);
            const QVariant value = (sourceIndex >= 0 && sourceIndex < static_cast<int>(queue.size()))
                ? QVariant {queue[sourceIndex].data[series.id]} : QVariant {};
            tooltip += u"<br><span style='color:"_s + m_properties[series.id].pen.color().name() + u"'>● "_s
                + m_properties[series.id].name.toHtmlEscaped() + u"</span> "_s
                + Utils::Misc::friendlyUnit(value.toLongLong(), true).toHtmlEscaped();
        }
        const QPoint tooltipPos = mapToGlobal(m_hoverPos.toPoint() + QPoint(14, -84));
        QToolTip::showText(tooltipPos, tooltip, viewport());
    }
    else
    {
        QToolTip::hideText();
    }
    painter.restore();
}

void SpeedPlotView::mouseMoveEvent(QMouseEvent *event)
{
    m_hovering = true;
    m_hoverPos = event->position();
    viewport()->update();
    QGraphicsView::mouseMoveEvent(event);
}

void SpeedPlotView::leaveEvent(QEvent *event)
{
    m_hovering = false;
    QToolTip::hideText();
    viewport()->update();
    QGraphicsView::leaveEvent(event);
}

void SpeedPlotView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
#ifdef TORRENTER_QA_HOOKS
    if (qEnvironmentVariableIsSet("TORRENTER_QA_SPEED_HOVER"))
    {
        m_hovering = true;
        m_hoverPos = QPointF(viewport()->width() * 0.55, viewport()->height() * 0.5);
    }
#endif
    if (m_displayMax <= 0)
        m_displayMax = static_cast<qreal>(maxYValue()) * 1.08;
    viewport()->update();
}

void SpeedPlotView::hideEvent(QHideEvent *event)
{
    m_lerpTimer.stop();
    QToolTip::hideText();
    m_hovering = false;
    QGraphicsView::hideEvent(event);
}

SpeedPlotView::GraphProperties::GraphProperties()
    : enable(false)
{
}

SpeedPlotView::GraphProperties::GraphProperties(const QString &name, const QPen &pen, bool enable)
    : name(name)
    , pen(pen)
    , enable(enable)
{
}
