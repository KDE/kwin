/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Casian Andrei <skeletk13@gmail.com>

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

#ifndef KWIN_COLOR_CORRECTION_P_H_
#define KWIN_COLOR_CORRECTION_P_H_

#include "kwinglcolorcorrection.h"

#include <QDBusAbstractInterface>
#include <QDBusMetaType>
#include <QDBusPendingReply>
#include <QRect>
#include <QVector>

class QDBusPendingCallWatcher;

/*
 * Clut
 * All this should be the same as in the color server code, in kolor-manager
 */
typedef QVector<quint16> Clut;
typedef QList<Clut> ClutList;
typedef struct { QRect r; Clut c; } RegionalClut;
typedef QMultiMap<uint, RegionalClut> RegionalClutMap;

Q_DECLARE_METATYPE(Clut)
Q_DECLARE_METATYPE(ClutList)
Q_DECLARE_METATYPE(RegionalClut)
Q_DECLARE_METATYPE(RegionalClutMap)

// Marshall the RegionalClut data into a D-Bus argument
inline QDBusArgument &operator<<(QDBusArgument &argument, const RegionalClut &rc)
{
    argument.beginStructure();
    argument << rc.r << rc.c;
    argument.endStructure();
    return argument;
}

// Retrieve the RegionalClut data from the D-Bus argument
inline const QDBusArgument &operator>>(const QDBusArgument &argument, RegionalClut &rc)
{
    argument.beginStructure();
    argument >> rc.r >> rc.c;
    argument.endStructure();
    return argument;
}


namespace KWin {

class ColorServerInterface;


/*
 * Color Correction Private Data
 */
class ColorCorrectionPrivate : public QObject
{
    Q_OBJECT

public:
    explicit ColorCorrectionPrivate(ColorCorrection* parent);
    virtual ~ColorCorrectionPrivate();

    void setupCCTextures();
    void deleteCCTextures();
    static void setupCCTexture(GLuint texture, const Clut &clut);

public slots:
    void colorServerUpdateSucceededSlot();
    void colorServerUpdateFailedSlot();

public:
    bool m_enabled;
    bool m_hasError;
    int m_ccTextureUnit;

    ColorServerInterface *m_csi;
    const ClutList *m_outputCluts;
    QVector<GLuint> m_outputCCTextures;
    const RegionalClutMap *m_regionCluts;
    QMultiMap<Window, QRect> m_windowRegions;
    QMap<const QRect*, GLuint> m_regionCCTextures; // keys from m_regions's values
    Clut m_dummyClut;
    GLuint m_dummyCCTexture;

    int m_lastOutput;

private:
    ColorCorrection *q_ptr;
    Q_DECLARE_PUBLIC(ColorCorrection);
};


/*
 * Color Server DBus interface
 */
class ColorServerInterface : public QDBusAbstractInterface
{
    Q_OBJECT

public:
    static inline const char *staticInterfaceName()
    { return "org.kde.KolorServer"; }

public:
    ColorServerInterface(const QString &service,
                         const QString &path,
                         const QDBusConnection &connection,
                         QObject *parent = 0);
    virtual ~ColorServerInterface();

    uint versionInfo() const;
    const ClutList& outputCluts() const;
    const RegionalClutMap& regionCluts() const;

public slots:
    void update();

signals:
    void updateSucceeded();
    void updateFailed();
    void outputClutsChanged();
    void regionClutsChanged();

private:
    QDBusPendingReply< uint > getVersionInfo();
    QDBusPendingReply< ClutList > getOutputCluts();
    QDBusPendingReply< RegionalClutMap > getRegionCluts();

private slots:
    void callFinishedSlot(QDBusPendingCallWatcher *watcher);

private:
    QDBusPendingCallWatcher *m_versionInfoWatcher;
    QDBusPendingCallWatcher *m_outputClutsWatcher;
    QDBusPendingCallWatcher *m_regionClutsWatcher;
    bool m_versionInfoUpdated;
    bool m_outputClutsUpdated;
    bool m_regionClutsUpdated;
    uint m_versionInfo;
    ClutList m_outputCluts;
    RegionalClutMap m_regionCluts;

    bool m_signaledFail;
};

} // KWin namespace

#endif // KWIN_COLOR_CORRECTION_P_H_
