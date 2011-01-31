/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/


#ifndef KWIN_TABBOX_PREVIEWHANDLERIMPL_H
#define KWIN_TABBOX_PREVIEWHANDLERIMPL_H

#include "tabboxhandler.h"

namespace KWin
{
namespace TabBox
{

class PreviewClientImpl :
    public KWin::TabBox::TabBoxClient
{
public:
    PreviewClientImpl(WId id);
    ~PreviewClientImpl();

    virtual QString caption() const;
    virtual int height() const;
    virtual QPixmap icon(const QSize& size = QSize(32, 32)) const;
    virtual bool isMinimized() const;
    virtual int width() const;
    virtual WId window() const;
    virtual int x() const;
    virtual int y() const;

private:
    WId m_id;
};

class PreviewHandlerImpl :
    public KWin::TabBox::TabBoxHandler
{
public:
    PreviewHandlerImpl();
    ~PreviewHandlerImpl();
    virtual KWin::TabBox::TabBoxClient* clientToAddToList(KWin::TabBox::TabBoxClient* client, int desktop, bool allDesktops) const;
    virtual KWin::TabBox::TabBoxClientList stackingOrder() const;
    virtual int nextDesktopFocusChain(int desktop) const;
    virtual int numberOfDesktops() const;
    virtual int currentDesktop() const;
    virtual QString desktopName(int desktop) const;
    virtual QString desktopName(KWin::TabBox::TabBoxClient* client) const;
    virtual KWin::TabBox::TabBoxClient* nextClientFocusChain(KWin::TabBox::TabBoxClient* client) const;
    virtual KWin::TabBox::TabBoxClient* activeClient() const;
    virtual int activeScreen() const;
    virtual TabBoxClient* desktopClient() const;

private:
    TabBoxClientList m_stackingOrder;
};

} // namespace TabBox
} // namespace KWin

#endif // KWIN_TABBOX_PREVIEWHANDLERIMPL_H
