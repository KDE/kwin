/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_DBUS_INTERFACE_H
#define KWIN_DBUS_INTERFACE_H

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;

namespace KWin
{

/**
 * @brief This class is a wrapper for the org.kde.KWin D-Bus interface.
 *
 * The main purpose of this class is to be exported on the D-Bus as object /KWin.
 * It is a pure wrapper to provide the deprecated D-Bus methods which have been
 * removed from Workspace which used to implement the complete D-Bus interface.
 *
 * Nowadays the D-Bus interfaces are distributed, parts of it are exported on
 * /Compositor, parts on /Effects and parts on /KWin. The implementation in this
 * class just delegates the method calls to the actual implementation in one of the
 * three singletons.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @todo KDE5: remove the methods provided on /Effects and /Compositor
 **/
class DBusInterface: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin")
public:
    DBusInterface(QObject *parent);
    virtual ~DBusInterface();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    Q_NOREPLY void cascadeDesktop();
    void circulateDesktopApplications();
    int currentDesktop();
    QList<int> decorationSupportedColors();
    void doNotManage(const QString &name);
    Q_NOREPLY void killWindow();
    void nextDesktop();
    void previousDesktop();
    Q_NOREPLY void reconfigure();
    bool setCurrentDesktop(int desktop);
    /**
     * @deprecated
     **/
    void showWindowMenuAt(qlonglong winId, int x, int y);
    bool startActivity(const QString &in0);
    bool stopActivity(const QString &in0);
    QString supportInformation();
    Q_NOREPLY void unclutterDesktop();
    // from compositor
    /**
     * @deprecated
     **/
    bool compositingActive();
    /**
     * @deprecated
     **/
    QString compositingNotPossibleReason();
    /**
     * @deprecated
     **/
    bool compositingPossible();
    /**
     * @deprecated
     **/
    QString compositingType();
    /**
     * @deprecated
     **/
    bool openGLIsBroken();
    /**
     * @deprecated
     **/
    Q_NOREPLY void toggleCompositing();
    /**
     * @deprecated
     **/
    bool waitForCompositingSetup();
    // from effectshandler
    /**
     * @deprecated
     **/
    QStringList activeEffects();
    /**
     * @deprecated
     **/
    QStringList listOfEffects();
    /**
     * @deprecated
     **/
    void loadEffect(const QString &name);
    /**
     * @deprecated
     **/
    QStringList loadedEffects();
    /**
     * @deprecated
     **/
    void reconfigureEffect(const QString &name);
    /**
     * @deprecated
     **/
    QString supportInformationForEffect(const QString &name);
    /**
     * @deprecated
     **/
    void toggleEffect(const QString &name);
    /**
     * @deprecated
     **/
    void unloadEffect(const QString &name);

Q_SIGNALS: // SIGNALS
    /**
     * @deprecated
     **/
    void compositingToggled(bool active);
};

} // namespace

#endif // KWIN_DBUS_INTERFACE_H
