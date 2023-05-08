/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "main.h"

#include <config-kwin.h>

#include "atoms.h"
#include "colors/colormanager.h"
#include "composite.h"
#include "core/outputbackend.h"
#include "core/session.h"
#include "cursor.h"
#include "effects.h"
#include "input.h"
#include "inputmethod.h"
#include "options.h"
#include "outline.h"
#include "pluginmanager.h"
#include "pointer_input.h"
#include "screenedge.h"
#include "sm.h"
#include "tabletmodemanager.h"
#include "utils/xcbutils.h"
#include "wayland/surface_interface.h"
#include "workspace.h"
#include "x11eventfilter.h"
#include "wayland_server.h"

#if KWIN_BUILD_SCREENLOCKER
#include "screenlockerwatcher.h"
#endif

#include "libkwineffects/kwineffects.h"

// KDE
#include <KAboutData>
#include <KLocalizedString>
// Qt
#include <QCommandLineParser>
#include <QQuickWindow>
#include <private/qtx11extras_p.h>
#include <qplatformdefs.h>
#include <QDBusVariant>
#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusMetaType>

#include <cerrno>

#if __has_include(<malloc.h>)
#include <malloc.h>
#endif
#include <unistd.h>

// xcb
#include <xcb/damage.h>
#ifndef XCB_GE_GENERIC
#define XCB_GE_GENERIC 35
#endif

Q_DECLARE_METATYPE(KSharedConfigPtr)



using namespace Qt::StringLiterals;

using VariantMapMap = QMap<QString, QMap<QString, QVariant>>;

namespace KWin
{

static bool groupMatches(const QString &group, const QStringList &patterns)
{
    return std::any_of(patterns.cbegin(), patterns.cend(), [&group](const auto &pattern) {
        if (pattern.isEmpty()) {
            return true;
        }

        if (pattern == group) {
            return true;
        }

        if (pattern.endsWith(QLatin1Char('*')) && group.startsWith(pattern.left(pattern.length() - 1))) {
            return true;
        }

        return false;
    });
}

class SettingsModule : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    ~SettingsModule() override = default;
    Q_DISABLE_COPY_MOVE(SettingsModule);
    virtual inline QString group() = 0;
    virtual VariantMapMap readAll(const QStringList &groups) = 0;
    virtual QVariant read(const QString &group, const QString &key) = 0;
    Q_SIGNAL void settingChanged(const QString &group, const QString &key, const QDBusVariant &value);
};

class VirtualKeyboardSettings : public SettingsModule
{
    Q_OBJECT
    static constexpr auto KEY_ACTIVE = "active"_L1;
    static constexpr auto KEY_ACTIVE_CLIENT_SUPPORTS_TEXT_INPUT = "activeClientSupportsTextInput"_L1;
    static constexpr auto KEY_AVAILABLE = "available"_L1;
    static constexpr auto KEY_ENABLED = "enabled"_L1;
    static constexpr auto KEY_VISIBLE = "visible"_L1;
    static constexpr auto KEY_WILL_SHOW_ON_ACTIVE = "willShowOnActive"_L1;
    static constexpr auto KEYS = {KEY_ACTIVE, KEY_ACTIVE_CLIENT_SUPPORTS_TEXT_INPUT, KEY_AVAILABLE, KEY_ENABLED, KEY_VISIBLE, KEY_WILL_SHOW_ON_ACTIVE};

public:
    explicit VirtualKeyboardSettings(QObject *parent = nullptr)
        : SettingsModule(parent)
        , m_inputMethod(kwinApp()->inputMethod())
    {
        connect(m_inputMethod, &InputMethod::activeChanged, this, [this]() {
            Q_EMIT settingChanged(group(), KEY_ACTIVE, QDBusVariant(readInternal(KEY_ACTIVE)));
        });
        connect(m_inputMethod, &InputMethod::activeClientSupportsTextInputChanged, this, [this]() {
            Q_EMIT settingChanged(group(), KEY_ACTIVE_CLIENT_SUPPORTS_TEXT_INPUT, QDBusVariant(readInternal(KEY_ACTIVE_CLIENT_SUPPORTS_TEXT_INPUT)));
        });
        connect(m_inputMethod, &InputMethod::availableChanged, this, [this]() {
            Q_EMIT settingChanged(group(), KEY_AVAILABLE, QDBusVariant(readInternal(KEY_AVAILABLE)));
        });
        connect(m_inputMethod, &InputMethod::enabledChanged, this, [this]() {
            Q_EMIT settingChanged(group(), KEY_ENABLED, QDBusVariant(readInternal(KEY_ENABLED)));
        });
        connect(m_inputMethod, &InputMethod::visibleChanged, this, [this]() {
            Q_EMIT settingChanged(group(), KEY_VISIBLE, QDBusVariant(readInternal(KEY_VISIBLE)));
        });
    }

    inline QString group() final
    {
        return u"org.kde.VirtualKeyboard"_s;
    }

    VariantMapMap readAll(const QStringList &groups) final
    {
        Q_UNUSED(groups);
        VariantMapMap result;
        QVariantMap map;
        for (const auto &key : KEYS) {
            map.insert(key, readInternal(key));
        }
        result.insert(group(), map);
        return result;
    }

    QVariant read(const QString &group, const QString &key) final
    {
        Q_UNUSED(group);
        for (const auto &keyIt : KEYS) {
            if (key == keyIt) {
                return readInternal(key);
            }
        }
        return {};
    }

private:
    inline QVariant readInternal(const QString &key)
    {
        if (key == KEY_WILL_SHOW_ON_ACTIVE) {
            return m_inputMethod->isAvailable() && m_inputMethod->isEnabled() && m_inputMethod->shouldShowOnActive();
        }
        return m_inputMethod->property(qUtf8Printable(key));
    }

    InputMethod *m_inputMethod;
};

// For consistency reasons TabletSettings have their property names changed.
// org.kde.TabletModel.enabled on our end is called tabletMode on the KWin side.
// As a consequence of that we do not meta program a mapping but instead manually write out the logic per key.
class TabletModeSettings : public SettingsModule
{
    Q_OBJECT
    static constexpr auto KEY_ENABLED = "enabled"_L1;
    static constexpr auto KEY_AVAILABLE = "available"_L1;

public:
    explicit TabletModeSettings(QObject *parent = nullptr)
        : SettingsModule(parent)
        , m_manager(kwinApp()->tabletModeManager())
    {
        connect(m_manager, &TabletModeManager::tabletModeAvailableChanged, this, [this](bool available) {
            Q_EMIT settingChanged(group(), KEY_AVAILABLE, QDBusVariant(available));
        });
        connect(m_manager, &TabletModeManager::tabletModeChanged, this, [this](bool enabled) {
            Q_EMIT settingChanged(group(), KEY_ENABLED, QDBusVariant(enabled));
        });
    }

    inline QString group() final
    {
        return u"org.kde.TabletMode"_s;
    }

    VariantMapMap readAll(const QStringList &groups) final
    {
        Q_UNUSED(groups);
        VariantMapMap result;
        result.insert(group(), {{KEY_AVAILABLE, read(group(), KEY_AVAILABLE)}, {KEY_ENABLED, read(group(), KEY_ENABLED)}});
        return result;
    }

    QVariant read(const QString &group, const QString &key) final
    {
        Q_UNUSED(group);
        if (key == KEY_AVAILABLE) {
            return m_manager->isTabletModeAvailable();
        }
        if (key == KEY_ENABLED) {
            return m_manager->effectiveTabletMode();
        }
        return {};
    }

    const TabletModeManager *const m_manager;
};

class SettingsPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    SettingsPortal(QObject *parent)
        : QDBusAbstractAdaptor(parent)
    {
        if (kwinApp()->inputMethod()) {
            m_settings.push_back(std::make_unique<VirtualKeyboardSettings>(this));
        }
        m_settings.push_back(std::make_unique<TabletModeSettings>(this));
        for (const auto &setting : std::as_const(m_settings)) {
            connect(setting.get(), &SettingsModule::settingChanged, this, &SettingsPortal::SettingChanged);
        }
        qDBusRegisterMetaType<VariantMapMap>();
    }

    uint version() const // only used as property, not slot
    {
        return 1;
    }

public Q_SLOTS:
    void ReadAll(const QStringList &groups, const QDBusMessage &message)
    {
        qCDebug(KWIN_CORE) << "ReadAll called with parameters:";
        qCDebug(KWIN_CORE) << "    groups: " << groups;

        VariantMapMap result;

        for (const auto &setting : m_settings) {
            if (groupMatches(setting->group(), groups)) {
                result.insert(setting->readAll(groups));
            }
        }

        QDBusMessage reply = message.createReply(QVariant::fromValue(result));
        QDBusConnection::sessionBus().send(reply);
    }

    void Read(const QString &group, const QString &key, const QDBusMessage &message)
    {
        qCDebug(KWIN_CORE) << "Read called with parameters:";
        qCDebug(KWIN_CORE) << "    group: " << group;
        qCDebug(KWIN_CORE) << "    key: " << key;

        const auto sentMesssage = std::any_of(m_settings.cbegin(), m_settings.cend(), [&message, &group, &key](const auto &setting) {
            if (group.startsWith(setting->group())) {
                const QVariant result = setting->read(group, key);
                QDBusMessage reply;
                if (result.isNull()) {
                    reply = message.createErrorReply(QDBusError::UnknownProperty, QStringLiteral("Property doesn't exist"));
                } else {
                    reply = message.createReply(QVariant::fromValue(QDBusVariant(result)));
                }
                QDBusConnection::sessionBus().send(reply);
                return true;
            }
            return false;
        });
        if (sentMesssage) {
            return;
        }

        qCWarning(KWIN_CORE) << "Namespace " << group << " is not supported";
        QDBusMessage reply = message.createErrorReply(QDBusError::UnknownProperty, QStringLiteral("Namespace is not supported"));
        QDBusConnection::sessionBus().send(reply);
    }

Q_SIGNALS:
    void SettingChanged(const QString &group, const QString &key, const QDBusVariant &value);

private:
    std::vector<std::unique_ptr<SettingsModule>> m_settings;
};

class PortalInterface : public QObject
{
    Q_OBJECT
public:
    PortalInterface(QObject *parent = nullptr)
        : QObject(parent)
    {
        qDBusRegisterMetaType<VariantMapMap>();
        new SettingsPortal(this);

        QDBusConnection sessionBus = QDBusConnection::sessionBus();
        if (!sessionBus.registerService(QStringLiteral("org.freedesktop.impl.portal.kwin"))) {
            qCDebug(KWIN_CORE) << "Failed to register portal service";
            return;
        }
        if (!sessionBus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), this, QDBusConnection::ExportAdaptors)) {
            qCDebug(KWIN_CORE) << "Failed to register portal object";
            return;
        }
    }
};

Options *options;
Atoms *atoms;
int Application::crashes = 0;

Application::Application(Application::OperationMode mode, int &argc, char **argv)
    : QApplication(argc, argv)
    , m_eventFilter(new XcbEventFilter())
    , m_configLock(false)
    , m_config(KSharedConfig::openConfig(QStringLiteral("kwinrc")))
    , m_kxkbConfig()
    , m_operationMode(mode)
{
    qRegisterMetaType<Options::WindowOperation>("Options::WindowOperation");
    qRegisterMetaType<KWin::EffectWindow *>();
    qRegisterMetaType<KWaylandServer::SurfaceInterface *>("KWaylandServer::SurfaceInterface *");
    qRegisterMetaType<KSharedConfigPtr>();
    qRegisterMetaType<std::chrono::nanoseconds>();
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

    performStartup();
    new PortalInterface(this);
}

Application::~Application()
{
    delete options;
    destroyAtoms();
    destroyPlatform();
    m_session.reset();
}

void Application::notifyStarted()
{
    Q_EMIT started();
}

void Application::destroyAtoms()
{
    delete atoms;
    atoms = nullptr;
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
                         QStringLiteral(KWIN_VERSION_STRING), // The program version string
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
    (void)new Workspace();
    Q_EMIT workspaceCreated();
}

void Application::createInput()
{
#if KWIN_BUILD_SCREENLOCKER
    m_screenLockerWatcher = std::make_unique<ScreenLockerWatcher>();
#endif
    auto input = InputRedirection::create(this);
    input->init();
    createPlatformCursor(this);
}

void Application::createAtoms()
{
    atoms = new Atoms;
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

void Application::installNativeX11EventFilter()
{
    installNativeEventFilter(m_eventFilter.get());
}

void Application::removeNativeX11EventFilter()
{
    removeNativeEventFilter(m_eventFilter.get());
}

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

std::unique_ptr<Edge> Application::createScreenEdge(ScreenEdges *edges)
{
    return std::make_unique<Edge>(edges);
}

void Application::createPlatformCursor(QObject *parent)
{
    new InputRedirectionCursor(parent);
}

std::unique_ptr<OutlineVisual> Application::createOutline(Outline *outline)
{
    if (Compositor::compositing()) {
        return std::make_unique<CompositedOutlineVisual>(outline);
    }
    return nullptr;
}

void Application::createEffectsHandler(Compositor *compositor, WorkspaceScene *scene)
{
    new EffectsHandlerImpl(compositor, scene);
}

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

void Application::setXwaylandScale(qreal scale)
{
    if (scale != m_xwaylandScale) {
        m_xwaylandScale = scale;
        Q_EMIT xwaylandScaleChanged();
    }
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
    static const QVector<QByteArray> s_xcbEerrors({QByteArrayLiteral("Success"),
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

    kwinApp()->updateX11Time(event);

    const uint8_t x11EventType = event->response_type & ~0x80;
    if (!x11EventType) {
        // let's check whether it's an error from one of the extensions KWin uses
        xcb_generic_error_t *error = reinterpret_cast<xcb_generic_error_t *>(event);
        const QVector<Xcb::ExtensionData> extensions = Xcb::Extensions::self()->extensions();
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

void Application::updateXTime()
{
    switch (operationMode()) {
    case Application::OperationModeX11:
        setX11Time(QX11Info::getTimestamp(), TimestampUpdate::Always);
        break;

    case Application::OperationModeXwayland:
        setX11Time(monotonicTime(), TimestampUpdate::Always);
        break;

    default:
        // Do not update the current X11 time stamp if it's the Wayland only session.
        break;
    }
}

void Application::updateX11Time(xcb_generic_event_t *event)
{
    xcb_timestamp_t time = XCB_TIME_CURRENT_TIME;
    const uint8_t eventType = event->response_type & ~0x80;
    switch (eventType) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        time = reinterpret_cast<xcb_key_press_event_t *>(event)->time;
        break;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        time = reinterpret_cast<xcb_button_press_event_t *>(event)->time;
        break;
    case XCB_MOTION_NOTIFY:
        time = reinterpret_cast<xcb_motion_notify_event_t *>(event)->time;
        break;
    case XCB_ENTER_NOTIFY:
    case XCB_LEAVE_NOTIFY:
        time = reinterpret_cast<xcb_enter_notify_event_t *>(event)->time;
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
        time = reinterpret_cast<xcb_property_notify_event_t *>(event)->time;
        break;
    case XCB_SELECTION_CLEAR:
        time = reinterpret_cast<xcb_selection_clear_event_t *>(event)->time;
        break;
    case XCB_SELECTION_REQUEST:
        time = reinterpret_cast<xcb_selection_request_event_t *>(event)->time;
        break;
    case XCB_SELECTION_NOTIFY:
        time = reinterpret_cast<xcb_selection_notify_event_t *>(event)->time;
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
                time = reinterpret_cast<xcb_shape_notify_event_t *>(event)->server_time;
            }
            if (eventType == Xcb::Extensions::self()->damageNotifyEvent()) {
                time = reinterpret_cast<xcb_damage_notify_event_t *>(event)->timestamp;
            }
        }
        break;
    }
    setX11Time(time);
}

bool XcbEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "xcb_generic_event_t") {
        return kwinApp()->dispatchEvent(static_cast<xcb_generic_event_t *>(message));
    }
    return false;
}

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

#if KWIN_BUILD_SCREENLOCKER
ScreenLockerWatcher *Application::screenLockerWatcher() const
{
    return m_screenLockerWatcher.get();
}
#endif

PlatformCursorImage Application::cursorImage() const
{
    Cursor *cursor = Cursors::self()->currentCursor();
    return PlatformCursorImage(cursor->image(), cursor->hotspot());
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

#include "main.moc"
