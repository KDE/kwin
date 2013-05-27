/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_PLUGINS_H
#define KWIN_PLUGINS_H

#include <kdecoration_plugins_p.h>
#include <kwinglobals.h>

namespace KWin
{

class DecorationPlugin
    : public QObject, public KDecorationPlugins
{
    Q_OBJECT
public:
    virtual ~DecorationPlugin();
    virtual bool provides(Requirement);
    /**
     * @returns @c true if there is no decoration plugin.
     **/
    bool isDisabled() const;

    bool hasShadows() const;
    bool hasAlpha() const;
    bool supportsAnnounceAlpha() const;
    bool supportsTabbing() const;
    bool supportsFrameOverlap() const;
    bool supportsBlurBehind() const;
    Qt::Corner closeButtonCorner();

    QString supportInformation();

    // D-Bus interface
    /**
     * @deprecated
     * @todo: remove KDE5
     **/
    QList<int> supportedColors() const;

public Q_SLOTS:
    void resetCompositing();
protected:
    virtual void error(const QString& error_msg);
private:
    void setDisabled(bool noDecoration);
    bool m_disabled;
    KWIN_SINGLETON(DecorationPlugin)
};

inline DecorationPlugin *decorationPlugin() {
    return DecorationPlugin::self();
}

} // namespace

#endif
