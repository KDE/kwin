/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2003-2005 Sandro Giessl <sandro@giessl.com>

based on the window decoration "Web":
Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "plastikbutton.h"
#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QPainter>

namespace KWin
{

PlastikButtonProvider::PlastikButtonProvider()
    : QQuickImageProvider(Pixmap)
{
}

QPixmap PlastikButtonProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    int origSize = requestedSize.isValid() ? qMin(requestedSize.width(), requestedSize.height()) : 10;
    if (size) {
        *size = QSize(origSize, origSize);
    }
    QStringList idParts = id.split(QStringLiteral("/"));
    if (idParts.isEmpty()) {
        // incorrect id
        return QQuickImageProvider::requestPixmap(id, size, requestedSize);
    }
    bool active = false;
    bool toggled = false;
    bool shadow = false;
    if (idParts.length() > 1 && idParts.at(1) == QStringLiteral("true")) {
        active = true;
    }
    if (idParts.length() > 2 && idParts.at(2) == QStringLiteral("true")) {
        toggled = true;
    }
    if (idParts.length() > 3 && idParts.at(3) == QStringLiteral("true")) {
        shadow = true;
    }
    ButtonIcon button;
    switch (static_cast<DecorationButton>(idParts[0].toInt())) {
    case DecorationButtonClose:
        button = CloseIcon;
        break;
    case DecorationButtonMaximizeRestore:
        if (toggled) {
            button = MaxRestoreIcon;
        } else {
            button = MaxIcon;
        }
        break;
    case DecorationButtonMinimize:
        button = MinIcon;
        break;
    case DecorationButtonQuickHelp:
        button = HelpIcon;
        break;
    case DecorationButtonOnAllDesktops:
        if (toggled) {
            button = NotOnAllDesktopsIcon;
        } else {
            button = OnAllDesktopsIcon;
        }
        break;
    case DecorationButtonKeepAbove:
        if (toggled) {
            button = NoKeepAboveIcon;
        } else {
            button = KeepAboveIcon;
        }
        break;
    case DecorationButtonKeepBelow:
        if (toggled) {
            button = NoKeepBelowIcon;
        } else {
            button = KeepBelowIcon;
        }
        break;
    case DecorationButtonShade:
        if (toggled) {
            button = UnShadeIcon;
        } else {
            button = ShadeIcon;
        }
        break;
    case DecorationButtonApplicationMenu:
        button = AppMenuIcon;
        break;
    default:
        // not recognized icon
        return QQuickImageProvider::requestPixmap(id, size, requestedSize);
    }
    return icon(button, origSize, active, shadow);
}

QPixmap PlastikButtonProvider::icon(ButtonIcon icon, int size, bool active, bool shadow)
{
    Q_UNUSED(active);
    if (size%2 == 0)
        --size;

    QPixmap image(size, size);
    image.fill(Qt::transparent);
    QPainter p(&image);
    KConfigGroup wmConfig(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("WM"));
    const QColor color = wmConfig.readEntry("activeForeground", QPalette().color(QPalette::Active, QPalette::HighlightedText));

    if (shadow) {
        p.setPen(KColorScheme::shade(color, KColorScheme::ShadowShade));
    } else {
        p.setPen(color);
    }

    const QRect r = image.rect();

    // line widths
    int lwTitleBar = 1;
    if (r.width() > 16) {
        lwTitleBar = 4;
    } else if (r.width() > 4) {
        lwTitleBar = 2;
    }
    int lwArrow = 1;
    if (r.width() > 16) {
        lwArrow = 4;
    } else if (r.width() > 7) {
        lwArrow = 2;
    }

    switch(icon) {
        case CloseIcon:
        {
            int lineWidth = 1;
            if (r.width() > 16) {
                lineWidth = 3;
            } else if (r.width() > 4) {
                lineWidth = 2;
            }

            drawObject(p, DiagonalLine, r.x(), r.y(), r.width(), lineWidth);
            drawObject(p, CrossDiagonalLine, r.x(), r.bottom(), r.width(), lineWidth);

            break;
        }

        case MaxIcon:
        {
            int lineWidth2 = 1; // frame
            if (r.width() > 16) {
                lineWidth2 = 2;
            } else if (r.width() > 4) {
                lineWidth2 = 1;
            }

            drawObject(p, HorizontalLine, r.x(), r.top(), r.width(), lwTitleBar);
            drawObject(p, HorizontalLine, r.x(), r.bottom()-(lineWidth2-1), r.width(), lineWidth2);
            drawObject(p, VerticalLine, r.x(), r.top(), r.height(), lineWidth2);
            drawObject(p, VerticalLine, r.right()-(lineWidth2-1), r.top(), r.height(), lineWidth2);

            break;
        }

        case MaxRestoreIcon:
        {
            int lineWidth2 = 1; // frame
            if (r.width() > 16) {
                lineWidth2 = 2;
            } else if (r.width() > 4) {
                lineWidth2 = 1;
            }

            int margin1, margin2;
            margin1 = margin2 = lineWidth2*2;
            if (r.width() < 8)
                margin1 = 1;

            // background window
            drawObject(p, HorizontalLine, r.x()+margin1, r.top(), r.width()-margin1, lineWidth2);
            drawObject(p, HorizontalLine, r.right()-margin2, r.bottom()-(lineWidth2-1)-margin1, margin2, lineWidth2);
            drawObject(p, VerticalLine, r.x()+margin1, r.top(), margin2, lineWidth2);
            drawObject(p, VerticalLine, r.right()-(lineWidth2-1), r.top(), r.height()-margin1, lineWidth2);

            // foreground window
            drawObject(p, HorizontalLine, r.x(), r.top()+margin2, r.width()-margin2, lwTitleBar);
            drawObject(p, HorizontalLine, r.x(), r.bottom()-(lineWidth2-1), r.width()-margin2, lineWidth2);
            drawObject(p, VerticalLine, r.x(), r.top()+margin2, r.height(), lineWidth2);
            drawObject(p, VerticalLine, r.right()-(lineWidth2-1)-margin2, r.top()+margin2, r.height(), lineWidth2);

            break;
        }

        case MinIcon:
        {
            drawObject(p, HorizontalLine, r.x(), r.bottom()-(lwTitleBar-1), r.width(), lwTitleBar);

            break;
        }

        case HelpIcon:
        {
            int center = r.x()+r.width()/2 -1;
            int side = r.width()/4;

            // paint a question mark... code is quite messy, to be cleaned up later...! :o

            if (r.width() > 16) {
                int lineWidth = 3;

                // top bar
                drawObject(p, HorizontalLine, center-side+3, r.y(), 2*side-3-1, lineWidth);
                // top bar rounding
                drawObject(p, CrossDiagonalLine, center-side-1, r.y()+5, 6, lineWidth);
                drawObject(p, DiagonalLine, center+side-3, r.y(), 5, lineWidth);
                // right bar
                drawObject(p, VerticalLine, center+side+2-lineWidth, r.y()+3, r.height()-(2*lineWidth+side+2+1), lineWidth);
                // bottom bar
                drawObject(p, CrossDiagonalLine, center, r.bottom()-2*lineWidth, side+2, lineWidth);
                drawObject(p, HorizontalLine, center, r.bottom()-3*lineWidth+2, lineWidth, lineWidth);
                // the dot
                drawObject(p, HorizontalLine, center, r.bottom()-(lineWidth-1), lineWidth, lineWidth);
            } else if (r.width() > 8) {
                int lineWidth = 2;

                // top bar
                drawObject(p, HorizontalLine, center-(side-1), r.y(), 2*side-1, lineWidth);
                // top bar rounding
                if (r.width() > 9) {
                    drawObject(p, CrossDiagonalLine, center-side-1, r.y()+3, 3, lineWidth);
                } else {
                    drawObject(p, CrossDiagonalLine, center-side-1, r.y()+2, 3, lineWidth);
                }
                drawObject(p, DiagonalLine, center+side-1, r.y(), 3, lineWidth);
                // right bar
                drawObject(p, VerticalLine, center+side+2-lineWidth, r.y()+2, r.height()-(2*lineWidth+side+1), lineWidth);
                // bottom bar
                drawObject(p, CrossDiagonalLine, center, r.bottom()-2*lineWidth+1, side+2, lineWidth);
                // the dot
                drawObject(p, HorizontalLine, center, r.bottom()-(lineWidth-1), lineWidth, lineWidth);
            } else {
                int lineWidth = 1;

                // top bar
                drawObject(p, HorizontalLine, center-(side-1), r.y(), 2*side, lineWidth);
                // top bar rounding
                drawObject(p, CrossDiagonalLine, center-side-1, r.y()+1, 2, lineWidth);
                // right bar
                drawObject(p, VerticalLine, center+side+1, r.y(), r.height()-(side+2+1), lineWidth);
                // bottom bar
                drawObject(p, CrossDiagonalLine, center, r.bottom()-2, side+2, lineWidth);
                // the dot
                drawObject(p, HorizontalLine, center, r.bottom(), 1, 1);
            }

            break;
        }

        case NotOnAllDesktopsIcon:
        {
            int lwMark = r.width()-lwTitleBar*2-2;
            if (lwMark < 1)
                lwMark = 3;

            drawObject(p, HorizontalLine, r.x()+(r.width()-lwMark)/2, r.y()+(r.height()-lwMark)/2, lwMark, lwMark);

            // Fall through to OnAllDesktopsIcon intended!
            Q_FALLTHROUGH();
        }
        case OnAllDesktopsIcon:
        {
            // horizontal bars
            drawObject(p, HorizontalLine, r.x()+lwTitleBar, r.y(), r.width()-2*lwTitleBar, lwTitleBar);
            drawObject(p, HorizontalLine, r.x()+lwTitleBar, r.bottom()-(lwTitleBar-1), r.width()-2*lwTitleBar, lwTitleBar);
            // vertical bars
            drawObject(p, VerticalLine, r.x(), r.y()+lwTitleBar, r.height()-2*lwTitleBar, lwTitleBar);
            drawObject(p, VerticalLine, r.right()-(lwTitleBar-1), r.y()+lwTitleBar, r.height()-2*lwTitleBar, lwTitleBar);


            break;
        }

        case NoKeepAboveIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, CrossDiagonalLine, r.x(), center+2*lwArrow, center-r.x(), lwArrow);
            drawObject(p, DiagonalLine, r.x()+center, r.y()+1+2*lwArrow, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.y()+2*lwArrow, (lwArrow-2)*2, lwArrow);

            // Fall through to KeepAboveIcon intended!
            Q_FALLTHROUGH();
        }
        case KeepAboveIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, CrossDiagonalLine, r.x(), center, center-r.x(), lwArrow);
            drawObject(p, DiagonalLine, r.x()+center, r.y()+1, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.y(), (lwArrow-2)*2, lwArrow);

            break;
        }

        case NoKeepBelowIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, DiagonalLine, r.x(), center-2*lwArrow, center-r.x(), lwArrow);
            drawObject(p, CrossDiagonalLine, r.x()+center, r.bottom()-1-2*lwArrow, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.bottom()-(lwArrow-1)-2*lwArrow, (lwArrow-2)*2, lwArrow);

            // Fall through to KeepBelowIcon intended!
            Q_FALLTHROUGH();
        }
        case KeepBelowIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, DiagonalLine, r.x(), center, center-r.x(), lwArrow);
            drawObject(p, CrossDiagonalLine, r.x()+center, r.bottom()-1, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.bottom()-(lwArrow-1), (lwArrow-2)*2, lwArrow);

            break;
        }

        case ShadeIcon:
        {
            drawObject(p, HorizontalLine, r.x(), r.y(), r.width(), lwTitleBar);

            break;
        }

        case UnShadeIcon:
        {
            int lw1 = 1;
            int lw2 = 1;
            if (r.width() > 16) {
                lw1 = 4;
                lw2 = 2;
            } else if (r.width() > 7) {
                lw1 = 2;
                lw2 = 1;
            }

            int h = qMax( (r.width()/2), (lw1+2*lw2) );

            // horizontal bars
            drawObject(p, HorizontalLine, r.x(), r.y(), r.width(), lw1);
            drawObject(p, HorizontalLine, r.x(), r.x()+h-(lw2-1), r.width(), lw2);
            // vertical bars
            drawObject(p, VerticalLine, r.x(), r.y(), h, lw2);
            drawObject(p, VerticalLine, r.right()-(lw2-1), r.y(), h, lw2);

            break;
        }
        case AppMenuIcon:
        {
            drawObject(p, HorizontalLine, r.x(), r.top()+(lwTitleBar-1), r.width(), lwTitleBar);
            drawObject(p, HorizontalLine, r.x(), r.center().y(), r.width(), lwTitleBar);
            drawObject(p, HorizontalLine, r.x(), r.bottom()-(lwTitleBar-1), r.width(), lwTitleBar);
            break;
        }

        default:
            break;
    }

    p.end();

    return image;
}

void PlastikButtonProvider::drawObject(QPainter &p, Object object, int x, int y, int length, int lineWidth)
{
    switch(object) {
        case DiagonalLine:
            if (lineWidth <= 1) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y+i);
                }
            } else if (lineWidth <= 2) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y+i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y+i);
                    p.drawPoint(x+i,y+1+i);
                }
            } else {
                for (int i = 1; i < (length-1); ++i) {
                    p.drawPoint(x+i,y+i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y+i);
                    p.drawPoint(x+i,y+1+i);
                }
                for (int i = 0; i < (length-2); ++i) {
                    p.drawPoint(x+2+i,y+i);
                    p.drawPoint(x+i,y+2+i);
                }
            }
            break;
        case CrossDiagonalLine:
            if (lineWidth <= 1) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y-i);
                }
            } else if (lineWidth <= 2) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y-i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y-i);
                    p.drawPoint(x+i,y-1-i);
                }
            } else {
                for (int i = 1; i < (length-1); ++i) {
                    p.drawPoint(x+i,y-i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y-i);
                    p.drawPoint(x+i,y-1-i);
                }
                for (int i = 0; i < (length-2); ++i) {
                    p.drawPoint(x+2+i,y-i);
                    p.drawPoint(x+i,y-2-i);
                }
            }
            break;
        case HorizontalLine:
            for (int i = 0; i < lineWidth; ++i) {
                p.drawLine(x,y+i, x+length-1, y+i);
            }
            break;
        case VerticalLine:
            for (int i = 0; i < lineWidth; ++i) {
                p.drawLine(x+i,y, x+i, y+length-1);
            }
            break;
        default:
            break;
    }
}

} // namespace
