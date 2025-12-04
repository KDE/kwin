/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "main.h"

#include "config-kwin.h"

#if KWIN_BUILD_X11
#include "atoms.h"
#endif
#include "colors/colormanager.h"
#include "compositor.h"
#include "core/outputbackend.h"
#include "core/rendertarget.h"
#include "core/session.h"
#include "cursor.h"
#include "cursorsource.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "inputmethod.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "options.h"
#include "outline.h"
#include "pluginmanager.h"
#include "pointer_input.h"
#include "scene/workspacescene.h"
#include "screenedge.h"
#include "sm.h"
#include "tabletmodemanager.h"
#include "wayland/surface.h"
#include "workspace.h"

#if KWIN_BUILD_X11
#include "utils/xcbutils.h"
#include "x11eventfilter.h"
#endif

#include "effect/effecthandler.h"

// KDE
#include <KAboutData>
#include <KLocalizedString>
// Qt
#include <QCommandLineParser>
#include <QQuickWindow>
#if KWIN_BUILD_X11
#include <private/qtx11extras_p.h>
#endif
#include <qplatformdefs.h>

#include <cerrno>

#if __has_include(<malloc.h>)
#include <malloc.h>
#endif
#include <unistd.h>

#if KWIN_BUILD_X11
#ifndef XCB_GE_GENERIC
#define XCB_GE_GENERIC 35
#endif
#endif

Q_DECLARE_METATYPE(KSharedConfigPtr)

namespace KWin
{

Options *options;
#if KWIN_BUILD_X11
Atoms *atoms;
#endif
int Application::crashes = 0;

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
#if KWIN_BUILD_X11
    , m_eventFilter(new XcbEventFilter())
#endif
    , m_configLock(false)
    , m_config(KSharedConfig::openConfig(QStringLiteral("kwinrc")))
    , m_kxkbConfig()
    , m_kdeglobals(KSharedConfig::openConfig(QStringLiteral("kdeglobals")))
{
    qRegisterMetaType<Options::WindowOperation>("Options::WindowOperation");
    qRegisterMetaType<KWin::EffectWindow *>();
    qRegisterMetaType<KWin::SurfaceInterface *>("KWin::SurfaceInterface *");
    qRegisterMetaType<KSharedConfigPtr>();
    qRegisterMetaType<std::chrono::nanoseconds>();
}

void Application::setConfigLock(bool lock)
{
    m_configLock = lock;
}

void Application::start()
{
    // Prevent KWin from synchronously autostarting kactivitymanagerd
    // Indeed, kactivitymanagerd being a QApplication it will depend
    // on KWin startup... this is unsatisfactory dependency wise,
    // and it turns out that it leads to a deadlock in the Wayland case
    setProperty("org.kde.KActivities.core.disableAutostart", true);

    setQuitOnLastWindowClosed(false);
    setQuitLockEnabled(false);

    if (!m_config->isImmutable() && m_configLock) {
        // TODO: This shouldn't be necessary
        // config->setReadOnly( true );
        m_config->reparseConfiguration();
    }
    if (!m_kxkbConfig) {
        m_kxkbConfig = KSharedConfig::openConfig(QStringLiteral("kxkbrc"), KConfig::NoGlobals);
    }
    if (!m_inputConfig) {
        m_inputConfig = KSharedConfig::openConfig(QStringLiteral("kcminputrc"), KConfig::NoGlobals);
    }

    performStartup();
}

Application::~Application()
{
    delete options;
    destroyAtoms();
    destroyPlatform();
    m_session.reset();
}

void Application::destroyAtoms()
{
#if KWIN_BUILD_X11
    delete atoms;
    atoms = nullptr;
#endif
}

void Application::destroyPlatform()
{
    m_outputBackend.reset();
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

void Application::createAboutData()
{
    KAboutData aboutData(QStringLiteral("kwin"), // The program name used internally
                         i18n("KWin"), // A displayable program name string
                         KWIN_VERSION_STRING, // The program version string
                         i18n("KDE window manager"), // Short description of what the app does
                         KAboutLicense::GPL, // The license this code is released under
                         i18n("(c) 1999-2019, The KDE Developers")); // Copyright Statement

    aboutData.addAuthor(i18n("Matthias Ettrich"), QString(), QStringLiteral("ettrich@kde.org"));
    aboutData.addAuthor(i18n("Cristian Tibirna"), QString(), QStringLiteral("tibirna@kde.org"));
    aboutData.addAuthor(i18n("Daniel M. Duley"), QString(), QStringLiteral("mosfet@kde.org"));
    aboutData.addAuthor(i18n("Luboš Luňák"), QString(), QStringLiteral("l.lunak@kde.org"));
    aboutData.addAuthor(i18n("Martin Flöser"), QString(), QStringLiteral("mgraesslin@kde.org"));
    aboutData.addAuthor(i18n("David Edmundson"), QStringLiteral("Maintainer"), QStringLiteral("davidedmundson@kde.org"));
    aboutData.addAuthor(i18n("Roman Gilg"), QStringLiteral("Maintainer"), QStringLiteral("subdiff@gmail.com"));
    aboutData.addAuthor(i18n("Vlad Zahorodnii"), QStringLiteral("Maintainer"), QStringLiteral("vlad.zahorodnii@kde.org"));
    aboutData.addAuthor(i18n("Xaver Hugl"), QStringLiteral("Maintainer"), QStringLiteral("xaver.hugl@gmail.com"));
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

void Application::setupMalloc()
{
#ifdef M_TRIM_THRESHOLD
    // Prevent fragmentation of the heap by malloc (glibc).
    //
    // The default threshold is 128*1024, which can result in a large memory usage
    // due to fragmentation especially if we use the raster graphicssystem. On the
    // otherside if the threshold is too low, free() starts to permanently ask the kernel
    // about shrinking the heap.
    const int pagesize = sysconf(_SC_PAGESIZE);
    mallopt(M_TRIM_THRESHOLD, 5 * pagesize);
#endif // M_TRIM_THRESHOLD
}

void Application::setupLocalizedString()
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kwin"));
}

void Application::createWorkspace()
{
    // we want all QQuickWindows with an alpha buffer, do here as Workspace might create QQuickWindows
    QQuickWindow::setDefaultAlphaBuffer(true);

    // This tries to detect compositing options and can use GLX. GLX problems
    // (X errors) shouldn't cause kwin to abort, so this is out of the
    // critical startup section where x errors cause kwin to abort.

    // create workspace.
    (void)new Workspace();
    Q_EMIT workspaceCreated();
}

void Application::createInput()
{
    auto input = InputRedirection::create(this);
    input->init();
}

void Application::createAtoms()
{
#if KWIN_BUILD_X11
    atoms = new Atoms;
#endif
}

void Application::createOptions()
{
    options = new Options;
}

void Application::createPlugins()
{
    m_pluginManager = std::make_unique<PluginManager>();
}

void Application::createColorManager()
{
    m_colorManager = std::make_unique<ColorManager>();
}

void Application::createInputMethod()
{
    m_inputMethod = std::make_unique<InputMethod>();
}

void Application::createTabletModeManager()
{
    m_tabletModeManager = std::make_unique<TabletModeManager>();
}

TabletModeManager *Application::tabletModeManager() const
{
    return m_tabletModeManager.get();
}

#if KWIN_BUILD_X11
void Application::installNativeX11EventFilter()
{
    installNativeEventFilter(m_eventFilter.get());
}

void Application::removeNativeX11EventFilter()
{
    removeNativeEventFilter(m_eventFilter.get());
}
#endif

void Application::destroyInput()
{
    delete InputRedirection::self();
}

void Application::destroyWorkspace()
{
    delete Workspace::self();
}

void Application::destroyCompositor()
{
    delete Compositor::self();
}

void Application::destroyPlugins()
{
    m_pluginManager.reset();
}

void Application::destroyColorManager()
{
    m_colorManager.reset();
}

void Application::destroyInputMethod()
{
    m_inputMethod.reset();
}

void Application::setXwaylandScale(qreal scale)
{
    Q_ASSERT(scale != 0);
    if (scale != m_xwaylandScale) {
        m_xwaylandScale = scale;
        applyXwaylandScale();
        Q_EMIT xwaylandScaleChanged();
    }
}

void Application::applyXwaylandScale()
{
    const bool xwaylandClientsScale = KConfig(QStringLiteral("kdeglobals"))
                                          .group(QStringLiteral("KScreen"))
                                          .readEntry("XwaylandClientsScale", true);

    KConfigGroup xwaylandGroup = kwinApp()->config()->group(QStringLiteral("Xwayland"));
    if (xwaylandClientsScale) {
        xwaylandGroup.writeEntry("Scale", m_xwaylandScale, KConfig::Notify);
    } else {
        xwaylandGroup.deleteEntry("Scale", KConfig::Notify);
    }
    xwaylandGroup.sync();

#if KWIN_BUILD_X11
    if (x11Connection()) {
        // rerun the fonts kcm init that does the appropriate xrdb call with the new settings
        QProcess::startDetached("kcminit", {"kcm_fonts_init", "kcm_style_init"});
    }
#endif
}

#if KWIN_BUILD_X11
void Application::registerEventFilter(X11EventFilter *filter)
{
    if (filter->isGenericEvent()) {
        m_genericEventFilters.append(new X11EventFilterContainer(filter));
    } else {
        m_eventFilters.append(new X11EventFilterContainer(filter));
    }
}

static X11EventFilterContainer *takeEventFilter(X11EventFilter *eventFilter,
                                                QList<QPointer<X11EventFilterContainer>> &list)
{
    for (int i = 0; i < list.count(); ++i) {
        X11EventFilterContainer *container = list.at(i);
        if (container->filter() == eventFilter) {
            return list.takeAt(i);
        }
    }
    return nullptr;
}

void Application::unregisterEventFilter(X11EventFilter *filter)
{
    X11EventFilterContainer *container = nullptr;
    if (filter->isGenericEvent()) {
        container = takeEventFilter(filter, m_genericEventFilters);
    } else {
        container = takeEventFilter(filter, m_eventFilters);
    }
    delete container;
}

bool Application::dispatchEvent(xcb_generic_event_t *event)
{
    static const QList<QByteArray> s_xcbEerrors({QByteArrayLiteral("Success"),
                                                 QByteArrayLiteral("BadRequest"),
                                                 QByteArrayLiteral("BadValue"),
                                                 QByteArrayLiteral("BadWindow"),
                                                 QByteArrayLiteral("BadPixmap"),
                                                 QByteArrayLiteral("BadAtom"),
                                                 QByteArrayLiteral("BadCursor"),
                                                 QByteArrayLiteral("BadFont"),
                                                 QByteArrayLiteral("BadMatch"),
                                                 QByteArrayLiteral("BadDrawable"),
                                                 QByteArrayLiteral("BadAccess"),
                                                 QByteArrayLiteral("BadAlloc"),
                                                 QByteArrayLiteral("BadColor"),
                                                 QByteArrayLiteral("BadGC"),
                                                 QByteArrayLiteral("BadIDChoice"),
                                                 QByteArrayLiteral("BadName"),
                                                 QByteArrayLiteral("BadLength"),
                                                 QByteArrayLiteral("BadImplementation"),
                                                 QByteArrayLiteral("Unknown")});

    const uint8_t x11EventType = event->response_type & ~0x80;
    if (!x11EventType) {
        // let's check whether it's an error from one of the extensions KWin uses
        xcb_generic_error_t *error = reinterpret_cast<xcb_generic_error_t *>(event);
        const QList<Xcb::ExtensionData> extensions = Xcb::Extensions::self()->extensions();
        for (const auto &extension : extensions) {
            if (error->major_code == extension.majorOpcode) {
                QByteArray errorName;
                if (error->error_code < s_xcbEerrors.size()) {
                    errorName = s_xcbEerrors.at(error->error_code);
                } else if (error->error_code >= extension.errorBase) {
                    const int index = error->error_code - extension.errorBase;
                    if (index >= 0 && index < extension.errorCodes.size()) {
                        errorName = extension.errorCodes.at(index);
                    }
                }
                if (errorName.isEmpty()) {
                    errorName = QByteArrayLiteral("Unknown");
                }
                qCWarning(KWIN_CORE, "XCB error: %d (%s), sequence: %d, resource id: %d, major code: %d (%s), minor code: %d (%s)",
                          int(error->error_code), errorName.constData(),
                          int(error->sequence), int(error->resource_id),
                          int(error->major_code), extension.name.constData(),
                          int(error->minor_code),
                          extension.opCodes.size() > error->minor_code ? extension.opCodes.at(error->minor_code).constData() : "Unknown");
                return true;
            }
        }
        return false;
    }

    if (x11EventType == XCB_GE_GENERIC) {
        xcb_ge_generic_event_t *ge = reinterpret_cast<xcb_ge_generic_event_t *>(event);

        // We need to make a shadow copy of the event filter list because an activated event
        // filter may mutate it by removing or installing another event filter.
        const auto eventFilters = m_genericEventFilters;

        for (X11EventFilterContainer *container : eventFilters) {
            if (!container) {
                continue;
            }
            X11EventFilter *filter = container->filter();
            if (filter->extension() == ge->extension && filter->genericEventTypes().contains(ge->event_type) && filter->event(event)) {
                return true;
            }
        }
    } else {
        // We need to make a shadow copy of the event filter list because an activated event
        // filter may mutate it by removing or installing another event filter.
        const auto eventFilters = m_eventFilters;

        for (X11EventFilterContainer *container : eventFilters) {
            if (!container) {
                continue;
            }
            X11EventFilter *filter = container->filter();
            if (filter->eventTypes().contains(x11EventType) && filter->event(event)) {
                return true;
            }
        }
    }

    if (workspace()) {
        return workspace()->workspaceEvent(event);
    }

    return false;
}

static quint32 monotonicTime()
{
    timespec ts;

    const int result = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (result) {
        qCWarning(KWIN_CORE, "Failed to query monotonic time: %s", strerror(errno));
    }

    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000L;
}

xcb_timestamp_t Application::x11Time() const
{
    return monotonicTime();
}

bool XcbEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "xcb_generic_event_t") {
        return kwinApp()->dispatchEvent(static_cast<xcb_generic_event_t *>(message));
    }
    return false;
}

#endif

QProcessEnvironment Application::processStartupEnvironment() const
{
    return m_processEnvironment;
}

void Application::setProcessStartupEnvironment(const QProcessEnvironment &environment)
{
    m_processEnvironment = environment;
}

void Application::setOutputBackend(std::unique_ptr<OutputBackend> &&backend)
{
    Q_ASSERT(!m_outputBackend);
    m_outputBackend = std::move(backend);
}

void Application::setSession(std::unique_ptr<Session> &&session)
{
    Q_ASSERT(!m_session);
    m_session = std::move(session);
}

PluginManager *Application::pluginManager() const
{
    return m_pluginManager.get();
}

InputMethod *Application::inputMethod() const
{
    return m_inputMethod.get();
}

ColorManager *Application::colorManager() const
{
    return m_colorManager.get();
}

XwaylandInterface *Application::xwayland() const
{
    return nullptr;
}

static PlatformCursorImage grabCursorOpenGL()
{
    auto scene = Compositor::self()->scene();
    if (!scene) {
        return PlatformCursorImage{};
    }
    Cursor *cursor = Cursors::self()->currentCursor();
    LogicalOutput *output = workspace()->outputAt(cursor->pos());

    const auto texture = GLTexture::allocate(GL_RGBA8, (cursor->geometry().size() * output->scale()).toSize());
    if (!texture) {
        return PlatformCursorImage{};
    }
    texture->setContentTransform(OutputTransform::FlipY);
    GLFramebuffer framebuffer(texture.get());
    RenderTarget renderTarget(&framebuffer);

    SceneView sceneView(scene, output, nullptr, nullptr);
    ItemTreeView cursorView(&sceneView, scene->cursorItem(), output, nullptr, nullptr);
    cursorView.prePaint();
    cursorView.paint(renderTarget, QPoint(), infiniteRegion());
    cursorView.postPaint();

    QImage image = texture->toImage();
    image.setDevicePixelRatio(output->scale());

    return PlatformCursorImage(image, cursor->hotspot());
}

static PlatformCursorImage grabCursorSoftware()
{
    auto scene = Compositor::self()->scene();
    if (!scene) {
        return PlatformCursorImage{};
    }
    Cursor *cursor = Cursors::self()->currentCursor();
    LogicalOutput *output = workspace()->outputAt(cursor->pos());

    QImage image((cursor->geometry().size() * output->scale()).toSize(), QImage::Format_ARGB32_Premultiplied);
    RenderTarget renderTarget(&image);

    SceneView sceneView(scene, output, nullptr, nullptr);
    ItemTreeView cursorView(&sceneView, scene->cursorItem(), output, nullptr, nullptr);
    cursorView.prePaint();
    cursorView.paint(renderTarget, QPoint(), infiniteRegion());
    cursorView.postPaint();

    image.setDevicePixelRatio(output->scale());
    return PlatformCursorImage(image, cursor->hotspot());
}

PlatformCursorImage Application::cursorImage() const
{
    Cursor *cursor = Cursors::self()->currentCursor();
    if (cursor->geometry().isEmpty()) {
        return PlatformCursorImage();
    }

    if (auto shapeSource = qobject_cast<ShapeCursorSource *>(cursor->source())) {
        return PlatformCursorImage(shapeSource->image(), shapeSource->hotspot());
    }

    // The cursor content is provided by a client, grab the contents of the cursor scene.
    switch (effects->compositingType()) {
    case OpenGLCompositing:
        return grabCursorOpenGL();
    case QPainterCompositing:
        return grabCursorSoftware();
    default:
        Q_UNREACHABLE();
    }
}

void Application::startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName)
{
    if (!input()) {
        callback(nullptr);
        return;
    }
    input()->startInteractiveWindowSelection(callback, cursorName);
}

void Application::startInteractivePositionSelection(std::function<void(const QPointF &)> callback)
{
    if (!input()) {
        callback(QPointF(-1, -1));
        return;
    }
    input()->startInteractivePositionSelection(callback);
}

} // namespace

#include "moc_main.cpp"
