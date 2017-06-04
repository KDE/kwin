/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_MAIN_X11_H
#define KWIN_MAIN_X11_H
#include "main.h"

namespace KWin
{

class KWinSelectionOwner
    : public KSelectionOwner
{
    Q_OBJECT
public:
    explicit KWinSelectionOwner(int screen);
protected:
    bool genericReply(xcb_atom_t target, xcb_atom_t property, xcb_window_t requestor) override;
    void replyTargets(xcb_atom_t property, xcb_window_t requestor) override;
    void getAtoms() override;
private:
    xcb_atom_t make_selection_atom(int screen);
    static xcb_atom_t xa_version;
};

class ApplicationX11 : public Application
{
    Q_OBJECT
public:
    ApplicationX11(int &argc, char **argv);
    virtual ~ApplicationX11();

    void setReplace(bool replace);

protected:
    void performStartup() override;
    bool notify(QObject *o, QEvent *e) override;

private Q_SLOTS:
    void lostSelection();

private:
    void crashChecking();
    void setupCrashHandler();

    static void crashHandler(int signal);

    QScopedPointer<KWinSelectionOwner> owner;
    bool m_replace;
};

}

#endif
