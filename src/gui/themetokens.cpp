/*
 * Torrenter design tokens - generated from tokens/tokens.json.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "themetokens.h"

#include <QFile>
#include <QHash>
#include <iterator>

using namespace Qt::Literals::StringLiterals;

namespace ThemeTokens
{
    namespace
    {
        const QColor lightColors[] = {
            QColor(QStringLiteral("#F6F6F4")),
            QColor(QStringLiteral("#F3F3F1")),
            QColor(QStringLiteral("#FDFDFC")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#F0F1EF")),
            QColor(QStringLiteral("#E8E9E6")),
            QColor(QStringLiteral("#E2F4EE")),
            QColor(QStringLiteral("#CDEDE2")),
            QColor(QStringLiteral("#EEEEEC")),
            QColor(QStringLiteral("#E4E4E1")),
            QColor(QStringLiteral("#D3D4D0")),
            QColor(QStringLiteral("#111214")),
            QColor(QStringLiteral("#55585F")),
            QColor(QStringLiteral("#6E7179")),
            QColor(QStringLiteral("#B4B6BB")),
            QColor(QStringLiteral("#077A5F")),
            QColor(QStringLiteral("#066B53")),
            QColor(QStringLiteral("#055C47")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#077A5F")),
            QColor(QStringLiteral("#E6E7E4")),
            QColor(QStringLiteral("#0A9D78")),
            QColor(QStringLiteral("#6A55E0")),
            QColor(QStringLiteral("#8F84DB")),
            QColor(QStringLiteral("#B7791F")),
            QColor(QStringLiteral("#7A7E87")),
            QColor(QStringLiteral("#66748A")),
            QColor(QStringLiteral("#1E7FC2")),
            QColor(QStringLiteral("#D63A3A")),
            QColor(QStringLiteral("#1A077A5F")),
            QColor(QStringLiteral("#59077A5F")),
            QColor(QStringLiteral("#73077A5F")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#1B1C1F")),
            QColor(QStringLiteral("#ECEDEF")),
            QColor(QStringLiteral("#FFFFFF")),
            QColor(QStringLiteral("#17669C")),
            QColor(QStringLiteral("#6E62C9")),
            QColor(QStringLiteral("#077A5F")),
            QColor(QStringLiteral("#C42F2F")),
            QColor(QStringLiteral("#686C75")),
            QColor(QStringLiteral("#66748A")),
            QColor(QStringLiteral("#6A55E0")),
            QColor(QStringLiteral("#8F5D12")),
            QColor(QStringLiteral("#FEFEFE"))   // AlternateRow (light)
        };

        const QColor darkColors[] = {
            QColor(QStringLiteral("#0C0D0F")),
            QColor(QStringLiteral("#0F1012")),
            QColor(QStringLiteral("#131417")),
            QColor(QStringLiteral("#18191D")),
            QColor(QStringLiteral("#1D1E23")),
            QColor(QStringLiteral("#1A1C20")),
            QColor(QStringLiteral("#202228")),
            QColor(QStringLiteral("#12302A")),
            QColor(QStringLiteral("#164036")),
            QColor(QStringLiteral("#1B1D21")),
            QColor(QStringLiteral("#24262B")),
            QColor(QStringLiteral("#30333A")),
            QColor(QStringLiteral("#ECEDEF")),
            QColor(QStringLiteral("#A0A3AB")),
            QColor(QStringLiteral("#7D818A")),
            QColor(QStringLiteral("#4A4D54")),
            QColor(QStringLiteral("#45E3B8")),
            QColor(QStringLiteral("#6BEBC8")),
            QColor(QStringLiteral("#2FC99F")),
            QColor(QStringLiteral("#03201A")),
            QColor(QStringLiteral("#5BE8C1")),
            QColor(QStringLiteral("#24262B")),
            QColor(QStringLiteral("#45E3B8")),
            QColor(QStringLiteral("#9D8CFF")),
            QColor(QStringLiteral("#7B72C4")),
            QColor(QStringLiteral("#F0B455")),
            QColor(QStringLiteral("#868A93")),
            QColor(QStringLiteral("#7C8AA0")),
            QColor(QStringLiteral("#5CC2FF")),
            QColor(QStringLiteral("#FF6B6B")),
            QColor(QStringLiteral("#1F45E3B8")),
            QColor(QStringLiteral("#6645E3B8")),
            QColor(QStringLiteral("#8C45E3B8")),
            QColor(QStringLiteral("#2A0606")),
            QColor(QStringLiteral("#2A2C32")),
            QColor(QStringLiteral("#ECEDEF")),
            QColor(QStringLiteral("#232429")),
            QColor(QStringLiteral("#5CC2FF")),
            QColor(QStringLiteral("#9C94E0")),
            QColor(QStringLiteral("#45E3B8")),
            QColor(QStringLiteral("#FF6B6B")),
            QColor(QStringLiteral("#A0A3AB")),
            QColor(QStringLiteral("#97A3B6")),
            QColor(QStringLiteral("#9D8CFF")),
            QColor(QStringLiteral("#F0B455")),
            QColor(QStringLiteral("#151619"))   // AlternateRow (dark)
        };

        struct NamedToken
        {
            QString name;
            Token token;
        };

        const NamedToken namedTokens[] = {
        {u"window"_s, Token::Window},
        {u"chrome"_s, Token::Chrome},
        {u"surface"_s, Token::Surface},
        {u"raised"_s, Token::Raised},
        {u"overlay"_s, Token::Overlay},
        {u"hover"_s, Token::Hover},
        {u"pressed"_s, Token::Pressed},
        {u"selected"_s, Token::Selected},
        {u"selectedStrong"_s, Token::SelectedStrong},
        {u"lineSubtle"_s, Token::LineSubtle},
        {u"line"_s, Token::Line},
        {u"lineStrong"_s, Token::LineStrong},
        {u"text1"_s, Token::Text1},
        {u"text2"_s, Token::Text2},
        {u"text3"_s, Token::Text3},
        {u"text4"_s, Token::Text4},
        {u"accent"_s, Token::Accent},
        {u"accentHover"_s, Token::AccentHover},
        {u"accentPress"_s, Token::AccentPress},
        {u"onAccent"_s, Token::OnAccent},
        {u"accentText"_s, Token::AccentText},
        {u"track"_s, Token::Track},
        {u"stDown"_s, Token::StDown},
        {u"stSeed"_s, Token::StSeed},
        {u"stDone"_s, Token::StDone},
        {u"stStalled"_s, Token::StStalled},
        {u"stPaused"_s, Token::StPaused},
        {u"stQueued"_s, Token::StQueued},
        {u"stCheck"_s, Token::StCheck},
        {u"stError"_s, Token::StError},
        {u"accentSoft"_s, Token::AccentSoft},
        {u"accentLine"_s, Token::AccentLine},
        {u"focus"_s, Token::Focus},
        {u"onDanger"_s, Token::OnDanger},
        {u"tooltipBg"_s, Token::TooltipBg},
        {u"tooltipText"_s, Token::TooltipText},
        {u"dialogField"_s, Token::DialogField},
        {u"ink.check"_s, Token::Inkcheck},
        {u"ink.done"_s, Token::Inkdone},
        {u"ink.down"_s, Token::Inkdown},
        {u"ink.error"_s, Token::Inkerror},
        {u"ink.paused"_s, Token::Inkpaused},
        {u"ink.queued"_s, Token::Inkqueued},
        {u"ink.seed"_s, Token::Inkseed},
        {u"ink.stalled"_s, Token::Inkstalled},
        {u"alternateRow"_s, Token::AlternateRow},
    };

        struct ThemeColor
        {
            QString id;
            QColor light;
            QColor dark;
        };

        const ThemeColor themeColors[] = {
        {u"Log.BannedPeer"_s, QColor(QStringLiteral("#C42F2F")), QColor(QStringLiteral("#FF6B6B"))},
        {u"Log.Critical"_s, QColor(QStringLiteral("#C42F2F")), QColor(QStringLiteral("#FF6B6B"))},
        {u"Log.Info"_s, QColor(QStringLiteral("#077A5F")), QColor(QStringLiteral("#5BE8C1"))},
        {u"Log.TimeStamp"_s, QColor(QStringLiteral("#6E7179")), QColor(QStringLiteral("#7D818A"))},
        {u"Log.Warning"_s, QColor(QStringLiteral("#8F5D12")), QColor(QStringLiteral("#F0B455"))},
        {u"Palette.AlternateBase"_s, QColor(QStringLiteral("#FFFFFF")), QColor(QStringLiteral("#18191D"))},
        {u"Palette.Base"_s, QColor(QStringLiteral("#FDFDFC")), QColor(QStringLiteral("#131417"))},
        {u"Palette.BrightText"_s, QColor(QStringLiteral("#111214")), QColor(QStringLiteral("#ECEDEF"))},
        {u"Palette.BrightTextDisabled"_s, QColor(QStringLiteral("#B4B6BB")), QColor(QStringLiteral("#4A4D54"))},
        {u"Palette.Button"_s, QColor(QStringLiteral("#FFFFFF")), QColor(QStringLiteral("#18191D"))},
        {u"Palette.ButtonText"_s, QColor(QStringLiteral("#111214")), QColor(QStringLiteral("#ECEDEF"))},
        {u"Palette.ButtonTextDisabled"_s, QColor(QStringLiteral("#B4B6BB")), QColor(QStringLiteral("#4A4D54"))},
        {u"Palette.Dark"_s, QColor(QStringLiteral("#EEEEEC")), QColor(QStringLiteral("#1B1D21"))},
        {u"Palette.Highlight"_s, QColor(QStringLiteral("#E2F4EE")), QColor(QStringLiteral("#12302A"))},
        {u"Palette.HighlightedText"_s, QColor(QStringLiteral("#111214")), QColor(QStringLiteral("#ECEDEF"))},
        {u"Palette.HighlightedTextDisabled"_s, QColor(QStringLiteral("#6E7179")), QColor(QStringLiteral("#7D818A"))},
        {u"Palette.Light"_s, QColor(QStringLiteral("#D3D4D0")), QColor(QStringLiteral("#30333A"))},
        {u"Palette.Link"_s, QColor(QStringLiteral("#077A5F")), QColor(QStringLiteral("#5BE8C1"))},
        {u"Palette.LinkVisited"_s, QColor(QStringLiteral("#077A5F")), QColor(QStringLiteral("#5BE8C1"))},
        {u"Palette.Mid"_s, QColor(QStringLiteral("#E4E4E1")), QColor(QStringLiteral("#24262B"))},
        {u"Palette.Midlight"_s, QColor(QStringLiteral("#E4E4E1")), QColor(QStringLiteral("#24262B"))},
        {u"Palette.Shadow"_s, QColor(QStringLiteral("#000000")), QColor(QStringLiteral("#000000"))},
        {u"Palette.Text"_s, QColor(QStringLiteral("#111214")), QColor(QStringLiteral("#ECEDEF"))},
        {u"Palette.TextDisabled"_s, QColor(QStringLiteral("#B4B6BB")), QColor(QStringLiteral("#4A4D54"))},
        {u"Palette.ToolTipBase"_s, QColor(QStringLiteral("#1B1C1F")), QColor(QStringLiteral("#2A2C32"))},
        {u"Palette.ToolTipText"_s, QColor(QStringLiteral("#ECEDEF")), QColor(QStringLiteral("#ECEDEF"))},
        {u"Palette.ToolTipTextDisabled"_s, QColor(QStringLiteral("#7D818A")), QColor(QStringLiteral("#7D818A"))},
        {u"Palette.Window"_s, QColor(QStringLiteral("#F6F6F4")), QColor(QStringLiteral("#0C0D0F"))},
        {u"Palette.WindowText"_s, QColor(QStringLiteral("#111214")), QColor(QStringLiteral("#ECEDEF"))},
        {u"Palette.WindowTextDisabled"_s, QColor(QStringLiteral("#B4B6BB")), QColor(QStringLiteral("#4A4D54"))},
        {u"PiecesBar.Border"_s, QColor(QStringLiteral("#FDFDFC")), QColor(QStringLiteral("#131417"))},
        {u"PiecesBar.MissingPiece"_s, QColor(QStringLiteral("#E6E7E4")), QColor(QStringLiteral("#24262B"))},
        {u"PiecesBar.PartialPiece"_s, QColor(QStringLiteral("#055C47")), QColor(QStringLiteral("#2FC99F"))},
        {u"PiecesBar.Piece"_s, QColor(QStringLiteral("#0A9D78")), QColor(QStringLiteral("#45E3B8"))},
        {u"ProgressBar"_s, QColor(QStringLiteral("#0A9D78")), QColor(QStringLiteral("#45E3B8"))},
        {u"RSS.ReadArticle"_s, QColor(QStringLiteral("#6E7179")), QColor(QStringLiteral("#7D818A"))},
        {u"RSS.UnreadArticle"_s, QColor(QStringLiteral("#111214")), QColor(QStringLiteral("#ECEDEF"))},
        {u"TransferList.CheckingDownloading"_s, QColor(QStringLiteral("#17669C")), QColor(QStringLiteral("#5CC2FF"))},
        {u"TransferList.CheckingResumeData"_s, QColor(QStringLiteral("#17669C")), QColor(QStringLiteral("#5CC2FF"))},
        {u"TransferList.CheckingUploading"_s, QColor(QStringLiteral("#17669C")), QColor(QStringLiteral("#5CC2FF"))},
        {u"TransferList.Downloading"_s, QColor(QStringLiteral("#077A5F")), QColor(QStringLiteral("#45E3B8"))},
        {u"TransferList.DownloadingMetadata"_s, QColor(QStringLiteral("#077A5F")), QColor(QStringLiteral("#45E3B8"))},
        {u"TransferList.Error"_s, QColor(QStringLiteral("#C42F2F")), QColor(QStringLiteral("#FF6B6B"))},
        {u"TransferList.ForcedDownloading"_s, QColor(QStringLiteral("#077A5F")), QColor(QStringLiteral("#45E3B8"))},
        {u"TransferList.ForcedDownloadingMetadata"_s, QColor(QStringLiteral("#077A5F")), QColor(QStringLiteral("#45E3B8"))},
        {u"TransferList.ForcedUploading"_s, QColor(QStringLiteral("#6A55E0")), QColor(QStringLiteral("#9D8CFF"))},
        {u"TransferList.MissingFiles"_s, QColor(QStringLiteral("#C42F2F")), QColor(QStringLiteral("#FF6B6B"))},
        {u"TransferList.Moving"_s, QColor(QStringLiteral("#17669C")), QColor(QStringLiteral("#5CC2FF"))},
        {u"TransferList.QueuedDownloading"_s, QColor(QStringLiteral("#66748A")), QColor(QStringLiteral("#97A3B6"))},
        {u"TransferList.QueuedUploading"_s, QColor(QStringLiteral("#66748A")), QColor(QStringLiteral("#97A3B6"))},
        {u"TransferList.StalledDownloading"_s, QColor(QStringLiteral("#8F5D12")), QColor(QStringLiteral("#F0B455"))},
        {u"TransferList.StalledUploading"_s, QColor(QStringLiteral("#6A55E0")), QColor(QStringLiteral("#9D8CFF"))},
        {u"TransferList.StoppedDownloading"_s, QColor(QStringLiteral("#686C75")), QColor(QStringLiteral("#A0A3AB"))},
        {u"TransferList.StoppedUploading"_s, QColor(QStringLiteral("#6E62C9")), QColor(QStringLiteral("#9C94E0"))},
        {u"TransferList.Uploading"_s, QColor(QStringLiteral("#6A55E0")), QColor(QStringLiteral("#9D8CFF"))}
        };
    }

    QColor color(const Token token, const Mode mode)
    {
        const int index = static_cast<int>(token);
        if ((index < 0) || (index >= static_cast<int>(std::size(lightColors))))
            return {};
        return (mode == Mode::Dark) ? darkColors[index] : lightColors[index];
    }

    QColor colorById(const QString &id, const Mode mode)
    {
        for (const ThemeColor &entry : themeColors)
        {
            if (entry.id == id)
                return (mode == Mode::Dark) ? entry.dark : entry.light;
        }
        return {};
    }

    QString processStyleSheet(const Mode mode)
    {
        QFile file(QStringLiteral(":/uithemedefault/torrenter.qss.in"));
        if (!file.open(QIODevice::ReadOnly))
            return {};

        QString sheet = QString::fromUtf8(file.readAll());
        for (const NamedToken &entry : namedTokens)
        {
            const QColor tokenColor = color(entry.token, mode);
            const QString hex = (tokenColor.alpha() == 255)
                    ? tokenColor.name()
                    : tokenColor.name(QColor::HexArgb);
            sheet.replace(QStringLiteral("${") % entry.name % QStringLiteral("}"), hex);
        }
        return sheet;
    }
}
