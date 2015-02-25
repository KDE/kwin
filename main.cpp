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

#include "main.h"
#include <config-kwin.h>
// kwin
#include "atoms.h"
#include "options.h"
#include "sm.h"
#include "workspace.h"
#include "xcbutils.h"

// KDE
#include <KAboutData>
#include <KConfig>
#include <KConfigGroup>
#include <KCrash>
#include <KLocalizedString>
#include <KSharedConfig>
// Qt
#include <qplatformdefs.h>
#include <QDebug>
#include <QComboBox>
#include <qcommandlineparser.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtDBus/QtDBus>

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

namespace KWin
{

Options* options;

Atoms* atoms;

int screen_number = -1;
bool is_multihead = false;

class AlternativeWMDialog : public QDialog
{
public:
    AlternativeWMDialog()
        : QDialog() {
        QWidget* mainWidget = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(mainWidget);
        QString text = i18n(
                           "KWin is unstable.\n"
                           "It seems to have crashed several times in a row.\n"
                           "You can select another window manager to run:");
        QLabel* textLabel = new QLabel(text, mainWidget);
        layout->addWidget(textLabel);
        wmList = new QComboBox(mainWidget);
        wmList->setEditable(true);
        layout->addWidget(wmList);

        addWM(QStringLiteral("metacity"));
        addWM(QStringLiteral("openbox"));
        addWM(QStringLiteral("fvwm2"));
        addWM(QStringLiteral(KWIN_INTERNAL_NAME_X11));

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(mainWidget);
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setDefault(true);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttons);

        raise();
    }

    void addWM(const QString& wm) {
        // TODO: Check if WM is installed
        if (!QStandardPaths::findExecutable(wm).isEmpty())
            wmList->addItem(wm);
    }
    QString selectedWM() const {
        return wmList->currentText();
    }

private:
    QComboBox* wmList;
};

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
    , m_operationMode(mode)
{
    qRegisterMetaType<Options::WindowOperation>("Options::WindowOperation");
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
    return m_operationMode == OperationModeWaylandAndX11;
}

bool Application::requiresCompositing() const
{
    return shouldUseWaylandForCompositing();
}

void Application::start()
{
    setQuitOnLastWindowClosed(false);

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    if (!config->isImmutable() && m_configLock) {
        // TODO: This shouldn't be necessary
        //config->setReadOnly( true );
        config->reparseConfiguration();
    }

    crashChecking();

    performStartup();
}

Application::~Application()
{
    delete options;
    delete atoms;
}

void Application::crashChecking()
{
    KCrash::setEmergencySaveFunction(Application::crashHandler);
    if (crashes >= 4) {
        // Something has gone seriously wrong
        AlternativeWMDialog dialog;
        QString cmd = QStringLiteral(KWIN_INTERNAL_NAME_X11);
        if (dialog.exec() == QDialog::Accepted)
            cmd = dialog.selectedWM();
        else
            ::exit(1);
        if (cmd.length() > 500) {
            qCDebug(KWIN_CORE) << "Command is too long, truncating";
            cmd = cmd.left(500);
        }
        qCDebug(KWIN_CORE) << "Starting" << cmd << "and exiting";
        char buf[1024];
        sprintf(buf, "%s &", cmd.toAscii().data());
        system(buf);
        ::exit(1);
    }
    if (crashes >= 2) {
        // Disable compositing if we have had too many crashes
        qCDebug(KWIN_CORE) << "Too many crashes recently, disabling compositing";
        KConfigGroup compgroup(KSharedConfig::openConfig(), "Compositing");
        compgroup.writeEntry("Enabled", false);
    }
    // Reset crashes count if we stay up for more that 15 seconds
    QTimer::singleShot(15 * 1000, this, SLOT(resetCrashesCount()));
}

bool Application::notify(QObject* o, QEvent* e)
{
    if (Workspace::self()->workspaceEvent(e))
        return true;
    return QApplication::notify(o, e);
}

void Application::crashHandler(int signal)
{
    crashes++;

    fprintf(stderr, "Application::crashHandler() called with signal %d; recent crashes: %d\n", signal, crashes);
    char cmd[1024];
    sprintf(cmd, "%s --crashes %d &",
            QFile::encodeName(QCoreApplication::applicationFilePath()).constData(), crashes);

    sleep(1);
    system(cmd);
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
                         i18n("(c) 1999-2013, The KDE Developers"));   // Copyright Statement

    aboutData.addAuthor(i18n("Matthias Ettrich"), QString(), QStringLiteral("ettrich@kde.org"));
    aboutData.addAuthor(i18n("Cristian Tibirna"), QString(), QStringLiteral("tibirna@kde.org"));
    aboutData.addAuthor(i18n("Daniel M. Duley"),  QString(), QStringLiteral("mosfet@kde.org"));
    aboutData.addAuthor(i18n("Luboš Luňák"),      QString(), QStringLiteral("l.lunak@kde.org"));
    aboutData.addAuthor(i18n("Martin Gräßlin"),   i18n("Maintainer"), QStringLiteral("mgraesslin@kde.org"));
    KAboutData::setApplicationData(aboutData);
}

static const QString s_lockOption = QStringLiteral("lock");
static const QString s_crashesOption = QStringLiteral("crashes");

void Application::setupCommandLine(QCommandLineParser *parser)
{
    QCommandLineOption lockOption(s_lockOption, i18n("Disable configuration options"));
    QCommandLineOption crashesOption(s_crashesOption, i18n("Indicate that KWin has recently crashed n times"), QStringLiteral("n"));

    parser->setApplicationDescription(i18n("KDE window manager"));
    parser->addVersionOption();
    parser->addHelpOption();
    parser->addOption(lockOption);
    parser->addOption(crashesOption);
    KAboutData::applicationData().setupCommandLine(parser);
}

void Application::processCommandLine(QCommandLineParser *parser)
{
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

void Application::setupLoggingCategoryFilters()
{
    QLoggingCategory::setFilterRules(QStringLiteral("aurorae.debug = true\n") +
                                     QStringLiteral("kwineffects.debug = true\n") +
                                     QStringLiteral("kwin_core.debug = true"));
}

void Application::notifyKSplash()
{
    // Tell KSplash that KWin has started
    QDBusMessage ksplashProgressMessage = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KSplash"),
                                                                            QStringLiteral("/KSplash"),
                                                                            QStringLiteral("org.kde.KSplash"),
                                                                            QStringLiteral("setStage"));
    ksplashProgressMessage.setArguments(QList<QVariant>() << QStringLiteral("wm"));
    QDBusConnection::sessionBus().asyncCall(ksplashProgressMessage);
}

void Application::createWorkspace()
{
    // ensure the helper atoms are retrieved before we create the Workspace
    atoms->retrieveHelpers();

    // we want all QQuickWindows with an alpha buffer, do here as Workspace might create QQuickWindows
    QQuickWindow::setDefaultAlphaBuffer(true);

    // This tries to detect compositing options and can use GLX. GLX problems
    // (X errors) shouldn't cause kwin to abort, so this is out of the
    // critical startup section where x errors cause kwin to abort.

    // create workspace.
    (void) new Workspace(isSessionRestored());
}

void Application::createAtoms()
{
    atoms = new Atoms;
}

void Application::createOptions()
{
    options = new Options;
}

void Application::setupEventFilters()
{
    installNativeEventFilter(m_eventFilter.data());
}

void Application::destroyWorkspace()
{
    delete Workspace::self();
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

} // namespace

#include "main.moc"
