/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

#include <kconfig.h>
#include <kwin/workspace.h>
#include <kwin/options.h>

#include "misc.h"
#include "plastik.h"
#include "plastik.moc"
#include "plastikclient.h"

using namespace KWinInternal;

// static bool pixmaps_created = false;

bool PlastikHandler::m_initialized    = false;
bool PlastikHandler::m_animateButtons = true;
bool PlastikHandler::m_titleShadow    = true;
bool PlastikHandler::m_shrinkBorders  = true;
int  PlastikHandler::m_borderSize     = 4;
int  PlastikHandler::m_titleHeight    = 19;
int  PlastikHandler::m_titleHeightTool= 12;
QFont PlastikHandler::m_titleFont = QFont();
QFont PlastikHandler::m_titleFontTool = QFont();
Qt::AlignmentFlags PlastikHandler::m_titleAlign = Qt::AlignHCenter;

PlastikHandler::PlastikHandler()
{
    reset();
}

PlastikHandler::~PlastikHandler()
{
    m_initialized = false;
}

void PlastikHandler::reset()
{
    m_titleFont = options->font(true, false);
    m_titleFontTool = m_titleFont;
    // Shrink font by 3pt, but no less than 7pts, less is ugly and unreadable
    m_titleFontTool.setPointSize(m_titleFont.pointSize() > 10? m_titleFont.pointSize() - 3: 7);
    m_titleFontTool.setWeight( QFont::Normal ); // and disable bold

    // read in the configuration
    readConfig();

    // reset all clients
    Workspace::self()->slotResetAllClientsDelayed();
    m_initialized = true;
}

void PlastikHandler::readConfig()
{
    // create a config object
    KConfig config("kwinplastikrc");
    config.setGroup("General");

    // grab settings
    m_titleShadow    = config.readBoolEntry("TitleShadow", true);
    m_shrinkBorders  = config.readBoolEntry("ShrinkBorders", true);
    m_borderSize     = config.readNumEntry("BorderSize", 4);

    QFontMetrics fm(m_titleFont);  // active font = inactive font
    int titleHeightMin = config.readNumEntry("TitleHeightMin", 19);
    // The title should strech with bigger font sizes!
    m_titleHeight = QMAX(titleHeightMin, fm.height() + 4); // 4 px for the shadow etc.

    fm = QFontMetrics(m_titleFontTool);  // active font = inactive font
    int titleHeightToolMin = config.readNumEntry("TitleHeightToolMin", 13);
    // The title should strech with bigger font sizes!
    m_titleHeightTool = QMAX(titleHeightToolMin, fm.height() ); // don't care about the shadow etc.

    QString value = config.readEntry("TitleAlignment", "AlignHCenter");
    if (value == "AlignLeft")         m_titleAlign = Qt::AlignLeft;
    else if (value == "AlignHCenter") m_titleAlign = Qt::AlignHCenter;
    else if (value == "AlignRight")   m_titleAlign = Qt::AlignRight;

    m_animateButtons = config.readBoolEntry("AnimateButtons", true);
}

QColor PlastikHandler::getColor(ColorType type, const bool active)
{
    switch (type) {
        case WindowContour:
            return options->colorGroup(Options::TitleBar, active).background().dark(180);
        case TitleGradientFrom:
            return options->colorGroup(Options::TitleBar, active).background();
            break;
        case TitleGradientTo:
            return alphaBlendColors(options->colorGroup(Options::TitleBar, active).background(),
                    Qt::white, active?210:220);
            break;
        case TitleGradientToTop:
            return alphaBlendColors(options->colorGroup(Options::TitleBar, active).background(),
                    Qt::white, active?180:190);
            break;
        case TitleHighlightTop:
        case SideHighlightLeft:
            return alphaBlendColors(options->colorGroup(Options::TitleBar, active).background(),
                    Qt::white, active?150:160);
            break;
        case SideHighlightRight:
        case SideHighlightBottom:
            return alphaBlendColors(options->colorGroup(Options::TitleBar, active).background(),
                    Qt::black, active?150:160);
            break;
        case Border:
            return options->colorGroup(Options::Frame, active).background();
        case TitleFont:
            return options->color(Options::Font, active);
        default:
            return Qt::black;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Plugin Stuff                                                             //
//////////////////////////////////////////////////////////////////////////////

static PlastikHandler *handler = 0;

extern "C"
{
    Client* allocate(Workspace *ws, WId w, int)
    {
        return new PlastikClient(ws, w);
    }

    void init()
    {
        handler = new PlastikHandler();
    }

    void reset()
    {
        handler->reset();
    }

    void deinit()
    {
         delete handler;
         handler = 0;
    }
}
