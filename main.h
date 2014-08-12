/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
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

#ifndef MAIN_H
#define MAIN_H

#include <kwinglobals.h>

#include <KSelectionWatcher>
#include <KSelectionOwner>
// Qt
#include <QApplication>
#include <QAbstractNativeEventFilter>

class QCommandLineParser;

namespace KWin
{

class XcbEventFilter : public QAbstractNativeEventFilter
{
public:
    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override;
};

class KWIN_EXPORT Application : public  QApplication
{
    Q_OBJECT
public:
    /**
    * @brief This enum provides the various operation modes of KWin depending on the available
    * Windowing Systems at startup. For example whether KWin only talks to X11 or also to a Wayland
    * Compositor.
    *
    */
    enum OperationMode {
        /**
        * @brief KWin uses only X11 for managing windows and compositing
        */
        OperationModeX11,
        /**
        * @brief KWin uses X11 for managing windows, but renders to a Wayland compositor.
        * Input is received from the Wayland compositor.
        */
        OperationModeWaylandAndX11
    };
    virtual ~Application();

    void setConfigLock(bool lock);

    void start();
    /**
    * @brief The operation mode used by KWin.
    *
    * @return OperationMode
    */
    OperationMode operationMode() const;
    void setOperationMode(OperationMode mode);
    bool shouldUseWaylandForCompositing() const;
    bool requiresCompositing() const;

    void setupTranslator();
    void setupCommandLine(QCommandLineParser *parser);
    void processCommandLine(QCommandLineParser *parser);

    void registerDBusService();

    static void setCrashCount(int count);
    static bool wasCrash();

    /**
     * Creates the KAboutData object for the KWin instance and registers it as
     * KAboutData::setApplicationData.
     **/
    static void createAboutData();

    /**
     * @returns the X11 Screen number. If not applicable it's set to @c -1.
     **/
    static int x11ScreenNumber();
    /**
     * Sets the X11 screen number of this KWin instance to @p screenNumber.
     **/
    static void setX11ScreenNumber(int screenNumber);
    /**
     * @returns whether this is a multi head setup on X11.
     **/
    static bool isX11MultiHead();
    /**
     * Sets whether this is a multi head setup on X11.
     */
    static void setX11MultiHead(bool multiHead);

    static void setupMalloc();
    static void setupLocalizedString();
    static void setupLoggingCategoryFilters();

protected:
    Application(OperationMode mode, int &argc, char **argv);
    virtual void performStartup() = 0;

    void notifyKSplash();
    void createWorkspace();
    void createAtoms();
    void createOptions();
    void setupEventFilters();
    void destroyWorkspace();

    bool notify(QObject* o, QEvent* e);
    static void crashHandler(int signal);

private Q_SLOTS:
    void resetCrashesCount();

private:
    void crashChecking();
    QScopedPointer<XcbEventFilter> m_eventFilter;
    bool m_configLock;
    OperationMode m_operationMode;
    static int crashes;
};

inline static Application *kwinApp()
{
    return static_cast<Application*>(QCoreApplication::instance());
}

} // namespace

#endif
