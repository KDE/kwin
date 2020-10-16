/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "main.h"
#include <config-kwin.h>
// kwin
#include "platform.h"
#include "atoms.h"
#include "composite.h"
#include "cursor.h"
#include "input.h"
#include "logind.h"
#include "options.h"
#include "screens.h"
#include "screenlockerwatcher.h"
#include "sm.h"
#include "workspace.h"
#include "xcbutils.h"

#include <kwineffects.h>

// KDE
#include <KAboutData>
#include <KLocalizedString>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <KWaylandServer/surface_interface.h>
// Qt
#include <qplatformdefs.h>
#include <QCommandLineParser>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTranslator>
#include <QLibraryInfo>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif // HAVE_MALLOC_H

// xcb
#include <xcb/damage.h>
#ifndef XCB_GE_GENERIC
#define XCB_GE_GENERIC 35
#endif

Q_DECLARE_METATYPE(KSharedConfigPtr)

namespace KWin
{

Options* options;

Atoms* atoms;

int screen_number = -1;
bool is_multihead = false;

int Application::crashes = 0;

bool Application::isX11MultiHead()
{
    return is_multihead;
}

void Application::setX11MultiHead(bool multiHead)
{
    is_multihead = multiHead;
}

void Application::setX11ScreenNumber(int screenNumber)
{
    screen_number = screenNumber;
}

int Application::x11ScreenNumber()
{
    return screen_number;
}

Application::Application(Application::OperationMode mode, int &argc, char **argv)
    : QApplication(argc, argv)
    , m_eventFilter(new XcbEventFilter())
    , m_configLock(false)
    , m_config()
    , m_kxkbConfig()
    , m_operationMode(mode)
{
    qRegisterMetaType<Options::WindowOperation>("Options::WindowOperation");
    qRegisterMetaType<KWin::EffectWindow*>();
    qRegisterMetaType<KWaylandServer::SurfaceInterface *>("KWaylandServer::SurfaceInterface *");
    qRegisterMetaType<KSharedConfigPtr>();
}

void Application::setConfigLock(bool lock)
{
    m_configLock = lock;
}

Application::OperationMode Application::operationMode() const
{
    return m_operationMode;
}

void Application::setOperationMode(OperationMode mode)
{
    m_operationMode = mode;
}

bool Application::shouldUseWaylandForCompositing() const
{
    return m_operationMode == OperationModeWaylandOnly || m_operationMode == OperationModeXwayland;
}

void Application::start()
{
    setQuitOnLastWindowClosed(false);

    if (!m_config) {
        m_config = KSharedConfig::openConfig();
    }
    if (!m_config->isImmutable() && m_configLock) {
        // TODO: This shouldn't be necessary
        //config->setReadOnly( true );
        m_config->reparseConfiguration();
    }
    if (!m_kxkbConfig) {
        m_kxkbConfig = KSharedConfig::openConfig(QStringLiteral("kxkbrc"), KConfig::NoGlobals);
    }

    performStartup();
}

Application::~Application()
{
    delete options;
    destroyAtoms();
    destroyPlatform();
}

void Application::notifyStarted()
{
    emit started();
}

void Application::destroyAtoms()
{
    delete atoms;
    atoms = nullptr;
}

void Application::destroyPlatform()
{
    delete m_platform;
    m_platform = nullptr;
}

void Application::resetCrashesCount()
{
    crashes = 0;
}

void Application::setCrashCount(int count)
{
    crashes = count;
}

bool Application::wasCrash()
{
    return crashes > 0;
}

static const char description[] = I18N_NOOP("KDE window manager");

void Application::createAboutData()
{
    KAboutData aboutData(QStringLiteral(KWIN_NAME),          // The program name used internally
                         i18n("KWin"),                       // A displayable program name string
                         QStringLiteral(KWIN_VERSION_STRING), // The program version string
                         i18n(description),                  // Short description of what the app does
                         KAboutLicense::GPL,            // The license this code is released under
                         i18n("(c) 1999-2019, The KDE Developers"));   // Copyright Statement

    aboutData.addAuthor(i18n("Matthias Ettrich"), QString(), QStringLiteral("ettrich@kde.org"));
    aboutData.addAuthor(i18n("Cristian Tibirna"), QString(), QStringLiteral("tibirna@kde.org"));
    aboutData.addAuthor(i18n("Daniel M. Duley"),  QString(), QStringLiteral("mosfet@kde.org"));
    aboutData.addAuthor(i18n("Luboš Luňák"),      QString(), QStringLiteral("l.lunak@kde.org"));
    aboutData.addAuthor(i18n("Martin Flöser"),    QString(), QStringLiteral("mgraesslin@kde.org"));
    aboutData.addAuthor(i18n("David Edmundson"),  QStringLiteral("Maintainer"), QStringLiteral("davidedmundson@kde.org"));
    aboutData.addAuthor(i18n("Roman Gilg"),       QStringLiteral("Maintainer"), QStringLiteral("subdiff@gmail.com"));
    aboutData.addAuthor(i18n("Vlad Zahorodnii"),  QStringLiteral("Maintainer"), QStringLiteral("vlad.zahorodnii@kde.org"));
    KAboutData::setApplicationData(aboutData);
}

static const QString s_lockOption = QStringLiteral("lock");
static const QString s_crashesOption = QStringLiteral("crashes");

void Application::setupCommandLine(QCommandLineParser *parser)
{
    QCommandLineOption lockOption(s_lockOption, i18n("Disable configuration options"));
    QCommandLineOption crashesOption(s_crashesOption, i18n("Indicate that KWin has recently crashed n times"), QStringLiteral("n"));

    parser->setApplicationDescription(i18n("KDE window manager"));
    parser->addOption(lockOption);
    parser->addOption(crashesOption);
    KAboutData::applicationData().setupCommandLine(parser);
}

void Application::processCommandLine(QCommandLineParser *parser)
{
    KAboutData aboutData = KAboutData::applicationData();
    aboutData.processCommandLine(parser);
    setConfigLock(parser->isSet(s_lockOption));
    Application::setCrashCount(parser->value(s_crashesOption).toInt());
}

void Application::setupTranslator()
{
    QTranslator *qtTranslator = new QTranslator(qApp);
    qtTranslator->load("qt_" + QLocale::system().name(),
                       QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    installTranslator(qtTranslator);
}

void Application::setupMalloc()
{
#ifdef M_TRIM_THRESHOLD
    // Prevent fragmentation of the heap by malloc (glibc).
    //
    // The default threshold is 128*1024, which can result in a large memory usage
    // due to fragmentation especially if we use the raster graphicssystem. On the
    // otherside if the threshold is too low, free() starts to permanently ask the kernel
    // about shrinking the heap.
#ifdef HAVE_UNISTD_H
    const int pagesize = sysconf(_SC_PAGESIZE);
#else
    const int pagesize = 4*1024;
#endif // HAVE_UNISTD_H
    mallopt(M_TRIM_THRESHOLD, 5*pagesize);
#endif // M_TRIM_THRESHOLD
}

void Application::setupLocalizedString()
{
    KLocalizedString::setApplicationDomain("kwin");
}

void Application::createWorkspace()
{
    // we want all QQuickWindows with an alpha buffer, do here as Workspace might create QQuickWindows
    QQuickWindow::setDefaultAlphaBuffer(true);

    // This tries to detect compositing options and can use GLX. GLX problems
    // (X errors) shouldn't cause kwin to abort, so this is out of the
    // critical startup section where x errors cause kwin to abort.

    // create workspace.
    (void) new Workspace();
    emit workspaceCreated();
}

void Application::createInput()
{
    ScreenLockerWatcher::create(this);
    LogindIntegration::create(this);
    auto input = InputRedirection::create(this);
    input->init();
    m_platform->createPlatformCursor(this);
}

void Application::createScreens()
{
    if (Screens::self()) {
        return;
    }
    Screens::create(this);
    emit screensCreated();
}

void Application::createAtoms()
{
    atoms = new Atoms;
}

void Application::createOptions()
{
    options = new Options;
}

void Application::installNativeX11EventFilter()
{
    installNativeEventFilter(m_eventFilter.data());
}

void Application::removeNativeX11EventFilter()
{
    removeNativeEventFilter(m_eventFilter.data());
}

void Application::destroyWorkspace()
{
    delete Workspace::self();
}

void Application::destroyCompositor()
{
    delete Compositor::self();
}

void Application::updateX11Time(xcb_generic_event_t *event)
{
    xcb_timestamp_t time = XCB_TIME_CURRENT_TIME;
    const uint8_t eventType = event->response_type & ~0x80;
    switch(eventType) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        time = reinterpret_cast<xcb_key_press_event_t*>(event)->time;
        break;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        time = reinterpret_cast<xcb_button_press_event_t*>(event)->time;
        break;
    case XCB_MOTION_NOTIFY:
        time = reinterpret_cast<xcb_motion_notify_event_t*>(event)->time;
        break;
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        time = reinterpret_cast<xcb_enter_notify_event_t*>(event)->time;
        break;
    case XCB_FOCUS_IN:
    case XCB_FOCUS_OUT:
    case XCB_KEYMAP_NOTIFY:
    case XCB_EXPOSE:
    case XCB_GRAPHICS_EXPOSURE:
    case XCB_NO_EXPOSURE:
    case XCB_VISIBILITY_NOTIFY:
    case XCB_CREATE_NOTIFY:
    case XCB_DESTROY_NOTIFY:
    case XCB_UNMAP_NOTIFY:
    case XCB_MAP_NOTIFY:
    case XCB_MAP_REQUEST:
    case XCB_REPARENT_NOTIFY:
    case XCB_CONFIGURE_NOTIFY:
    case XCB_CONFIGURE_REQUEST:
    case XCB_GRAVITY_NOTIFY:
    case XCB_RESIZE_REQUEST:
    case XCB_CIRCULATE_NOTIFY:
    case XCB_CIRCULATE_REQUEST:
        // no timestamp
        return;
    case XCB_PROPERTY_NOTIFY:
        time = reinterpret_cast<xcb_property_notify_event_t*>(event)->time;
        break;
    case XCB_SELECTION_CLEAR:
        time = reinterpret_cast<xcb_selection_clear_event_t*>(event)->time;
        break;
    case XCB_SELECTION_REQUEST:
        time = reinterpret_cast<xcb_selection_request_event_t*>(event)->time;
        break;
    case XCB_SELECTION_NOTIFY:
        time = reinterpret_cast<xcb_selection_notify_event_t*>(event)->time;
        break;
    case XCB_COLORMAP_NOTIFY:
    case XCB_CLIENT_MESSAGE:
    case XCB_MAPPING_NOTIFY:
    case XCB_GE_GENERIC:
        // no timestamp
        return;
    default:
        // extension handling
        if (Xcb::Extensions::self()) {
            if (eventType == Xcb::Extensions::self()->shapeNotifyEvent()) {
                time = reinterpret_cast<xcb_shape_notify_event_t*>(event)->server_time;
            }
            if (eventType == Xcb::Extensions::self()->damageNotifyEvent()) {
                time = reinterpret_cast<xcb_damage_notify_event_t*>(event)->timestamp;
            }
        }
        break;
    }
    setX11Time(time);
}

bool XcbEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long int *result)
{
    Q_UNUSED(result)
    if (eventType != "xcb_generic_event_t") {
        return false;
    }
    auto event = static_cast<xcb_generic_event_t *>(message);
    kwinApp()->updateX11Time(event);
    if (!Workspace::self()) {
        // Workspace not yet created
        return false;
    }
    return Workspace::self()->workspaceEvent(event);
}

static bool s_useLibinput = false;

void Application::setUseLibinput(bool use)
{
    s_useLibinput = use;
}

bool Application::usesLibinput()
{
    return s_useLibinput;
}

QProcessEnvironment Application::processStartupEnvironment() const
{
    return QProcessEnvironment::systemEnvironment();
}

void Application::initPlatform(const KPluginMetaData &plugin)
{
    Q_ASSERT(!m_platform);
    m_platform = qobject_cast<Platform *>(plugin.instantiate());
    if (m_platform) {
        m_platform->setParent(this);
        // check whether it needs libinput
        const QJsonObject &metaData = plugin.rawData();
        auto it = metaData.find(QStringLiteral("input"));
        if (it != metaData.end()) {
            if ((*it).isBool()) {
                if (!(*it).toBool()) {
                    qCDebug(KWIN_CORE) << "Platform does not support input, enforcing libinput support";
                    setUseLibinput(true);
                }
            }
        }
    }
}

ApplicationWaylandAbstract::ApplicationWaylandAbstract(OperationMode mode, int &argc, char **argv)
    : Application(mode, argc, argv)
{
}

ApplicationWaylandAbstract::~ApplicationWaylandAbstract()
{
}

} // namespace

