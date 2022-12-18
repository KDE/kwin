/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "main_x11.h"

#include <config-kwin.h>

#include "backends/x11/standalone/x11_standalone_backend.h"
#include "core/outputbackend.h"
#include "core/session.h"
#include "outline.h"
#include "screenedge.h"
#include "sm.h"
#include "tabletmodemanager.h"
#include "utils/xcbutils.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KCrash>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KSelectionOwner>
#include <KSignalHandler>

#include <QAction>
#include <QComboBox>
#include <QCommandLineParser>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QVBoxLayout>
#include <qplatformdefs.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif
#include <QtDBus>

// system
#include <iostream>
#include <unistd.h>

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtWarningMsg)

namespace KWin
{

class AlternativeWMDialog : public QDialog
{
public:
    AlternativeWMDialog()
        : QDialog()
    {
        QWidget *mainWidget = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(mainWidget);
        QString text = i18n("KWin is unstable.\n"
                            "It seems to have crashed several times in a row.\n"
                            "You can select another window manager to run:");
        QLabel *textLabel = new QLabel(text, mainWidget);
        layout->addWidget(textLabel);
        wmList = new QComboBox(mainWidget);
        wmList->setEditable(true);
        layout->addWidget(wmList);

        addWM(QStringLiteral("metacity"));
        addWM(QStringLiteral("openbox"));
        addWM(QStringLiteral("fvwm2"));
        addWM(QStringLiteral("kwin_x11"));

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(mainWidget);
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setDefault(true);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttons);

        raise();
    }

    void addWM(const QString &wm)
    {
        // TODO: Check if WM is installed
        if (!QStandardPaths::findExecutable(wm).isEmpty()) {
            wmList->addItem(wm);
        }
    }
    QString selectedWM() const
    {
        return wmList->currentText();
    }

private:
    QComboBox *wmList;
};

class KWinSelectionOwner : public KSelectionOwner
{
    Q_OBJECT
public:
    explicit KWinSelectionOwner()
        : KSelectionOwner(make_selection_atom())
    {
    }

private:
    bool genericReply(xcb_atom_t target_P, xcb_atom_t property_P, xcb_window_t requestor_P) override
    {
        if (target_P == xa_version) {
            int32_t version[] = {2, 0};
            xcb_change_property(kwinApp()->x11Connection(), XCB_PROP_MODE_REPLACE, requestor_P,
                                property_P, XCB_ATOM_INTEGER, 32, 2, version);
        } else {
            return KSelectionOwner::genericReply(target_P, property_P, requestor_P);
        }
        return true;
    }

    void replyTargets(xcb_atom_t property_P, xcb_window_t requestor_P) override
    {
        KSelectionOwner::replyTargets(property_P, requestor_P);
        xcb_atom_t atoms[1] = {xa_version};
        // PropModeAppend !
        xcb_change_property(kwinApp()->x11Connection(), XCB_PROP_MODE_APPEND, requestor_P,
                            property_P, XCB_ATOM_ATOM, 32, 1, atoms);
    }

    void getAtoms() override
    {
        KSelectionOwner::getAtoms();
        if (xa_version == XCB_ATOM_NONE) {
            const QByteArray name(QByteArrayLiteral("VERSION"));
            UniqueCPtr<xcb_intern_atom_reply_t> atom(xcb_intern_atom_reply(
                kwinApp()->x11Connection(),
                xcb_intern_atom_unchecked(kwinApp()->x11Connection(), false, name.length(), name.constData()),
                nullptr));
            if (atom) {
                xa_version = atom->atom;
            }
        }
    }

    xcb_atom_t make_selection_atom()
    {
        QByteArray screen(QByteArrayLiteral("WM_S0"));
        UniqueCPtr<xcb_intern_atom_reply_t> atom(xcb_intern_atom_reply(
            kwinApp()->x11Connection(),
            xcb_intern_atom_unchecked(kwinApp()->x11Connection(), false, screen.length(), screen.constData()),
            nullptr));
        if (!atom) {
            return XCB_ATOM_NONE;
        }
        return atom->atom;
    }
    static xcb_atom_t xa_version;
};
xcb_atom_t KWinSelectionOwner::xa_version = XCB_ATOM_NONE;

//************************************
// ApplicationX11
//************************************

ApplicationX11::ApplicationX11(int &argc, char **argv)
    : Application(OperationModeX11, argc, argv)
    , owner()
    , m_replace(false)
{
    setX11Connection(QX11Info::connection());
    setX11RootWindow(QX11Info::appRootWindow());
}

ApplicationX11::~ApplicationX11()
{
    setTerminating();
    destroyPlugins();
    destroyCompositor();
    destroyColorManager();
    destroyWorkspace();
    // If there was no --replace (no new WM)
    if (owner != nullptr && owner->ownerWindow() != XCB_WINDOW_NONE) {
        Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
    }
}

void ApplicationX11::setReplace(bool replace)
{
    m_replace = replace;
}

std::unique_ptr<Edge> ApplicationX11::createScreenEdge(ScreenEdges *parent)
{
    return static_cast<X11StandaloneBackend *>(outputBackend())->createScreenEdge(parent);
}

void ApplicationX11::createPlatformCursor(QObject *parent)
{
    static_cast<X11StandaloneBackend *>(outputBackend())->createPlatformCursor(parent);
}

std::unique_ptr<OutlineVisual> ApplicationX11::createOutline(Outline *outline)
{
    // first try composited Outline
    if (auto outlineVisual = Application::createOutline(outline)) {
        return outlineVisual;
    }
    return static_cast<X11StandaloneBackend *>(outputBackend())->createOutline(outline);
}

void ApplicationX11::createEffectsHandler(Compositor *compositor, WorkspaceScene *scene)
{
    static_cast<X11StandaloneBackend *>(outputBackend())->createEffectsHandler(compositor, scene);
}

void ApplicationX11::startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName)
{
    static_cast<X11StandaloneBackend *>(outputBackend())->startInteractiveWindowSelection(callback, cursorName);
}

void ApplicationX11::startInteractivePositionSelection(std::function<void(const QPoint &)> callback)
{
    static_cast<X11StandaloneBackend *>(outputBackend())->startInteractivePositionSelection(callback);
}

PlatformCursorImage ApplicationX11::cursorImage() const
{
    return static_cast<X11StandaloneBackend *>(outputBackend())->cursorImage();
}

void ApplicationX11::lostSelection()
{
    sendPostedEvents();
    destroyPlugins();
    destroyCompositor();
    destroyColorManager();
    destroyWorkspace();
    // Remove windowmanager privileges
    Xcb::selectInput(kwinApp()->x11RootWindow(), XCB_EVENT_MASK_PROPERTY_CHANGE);
    removeNativeX11EventFilter();
    quit();
}

void ApplicationX11::performStartup()
{
    crashChecking();

    owner.reset(new KWinSelectionOwner());
    connect(owner.get(), &KSelectionOwner::failedToClaimOwnership, [] {
        fputs(i18n("kwin: unable to claim manager selection, another wm running? (try using --replace)\n").toLocal8Bit().constData(), stderr);
        ::exit(1);
    });
    connect(owner.get(), &KSelectionOwner::lostOwnership, this, &ApplicationX11::lostSelection);
    connect(owner.get(), &KSelectionOwner::claimedOwnership, this, [this] {
        installNativeX11EventFilter();
        // first load options - done internally by a different thread
        createOptions();

        if (!outputBackend()->initialize()) {
            std::exit(1);
        }

        // Check  whether another windowmanager is running
        const uint32_t maskValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
        UniqueCPtr<xcb_generic_error_t> redirectCheck(xcb_request_check(kwinApp()->x11Connection(),
                                                                        xcb_change_window_attributes_checked(kwinApp()->x11Connection(),
                                                                                                             kwinApp()->x11RootWindow(),
                                                                                                             XCB_CW_EVENT_MASK,
                                                                                                             maskValues)));
        if (redirectCheck) {
            fputs(i18n("kwin: another window manager is running (try using --replace)\n").toLocal8Bit().constData(), stderr);
            if (!wasCrash()) { // if this is a crash-restart, DrKonqi may have stopped the process w/o killing the connection
                ::exit(1);
            }
        }

        // Update the timestamp if a global shortcut is pressed or released. Needed
        // to ensure that kwin can grab the keyboard.
        connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutActiveChanged, this, [this](QAction *triggeredAction) {
            QVariant timestamp = triggeredAction->property("org.kde.kglobalaccel.activationTimestamp");
            bool ok = false;
            const quint32 t = timestamp.toULongLong(&ok);
            if (ok) {
                kwinApp()->setX11Time(t);
            }
        });

        createInput();
        createWorkspace();
        createColorManager();
        createPlugins();

        Xcb::sync(); // Trigger possible errors, there's still a chance to abort

        notifyKSplash();
        notifyStarted();
    });
    // we need to do an XSync here, otherwise the QPA might crash us later on
    Xcb::sync();
    owner->claim(m_replace || wasCrash(), true);

    createAtoms();

    createTabletModeManager();
}

void ApplicationX11::setupCrashHandler()
{
    KCrash::setEmergencySaveFunction(ApplicationX11::crashHandler);
}

void ApplicationX11::crashChecking()
{
    setupCrashHandler();
    if (crashes >= 4) {
        // Something has gone seriously wrong
        AlternativeWMDialog dialog;
        QString cmd = QStringLiteral("kwin_x11");
        if (dialog.exec() == QDialog::Accepted) {
            cmd = dialog.selectedWM();
        } else {
            ::exit(1);
        }
        if (cmd.length() > 500) {
            qCDebug(KWIN_CORE) << "Command is too long, truncating";
            cmd = cmd.left(500);
        }
        qCDebug(KWIN_CORE) << "Starting" << cmd << "and exiting";
        char buf[1024];
        sprintf(buf, "%s &", cmd.toLatin1().data());
        system(buf);
        ::exit(1);
    }
    // Reset crashes count if we stay up for more that 15 seconds
    QTimer::singleShot(15 * 1000, this, &Application::resetCrashesCount);
}

void ApplicationX11::notifyKSplash()
{
    // Tell KSplash that KWin has started
    QDBusMessage ksplashProgressMessage = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KSplash"),
                                                                         QStringLiteral("/KSplash"),
                                                                         QStringLiteral("org.kde.KSplash"),
                                                                         QStringLiteral("setStage"));
    ksplashProgressMessage.setArguments(QList<QVariant>() << QStringLiteral("wm"));
    QDBusConnection::sessionBus().asyncCall(ksplashProgressMessage);
}

void ApplicationX11::crashHandler(int signal)
{
    crashes++;

    fprintf(stderr, "Application::crashHandler() called with signal %d; recent crashes: %d\n", signal, crashes);
    char cmd[1024];
    sprintf(cmd, "%s --crashes %d &",
            QFile::encodeName(QCoreApplication::applicationFilePath()).constData(), crashes);

    sleep(1);
    system(cmd);
}

} // namespace

int main(int argc, char *argv[])
{
    KWin::Application::setupMalloc();
    KWin::Application::setupLocalizedString();

    signal(SIGPIPE, SIG_IGN);

    // enforce xcb plugin, unfortunately command line switch has precedence
    setenv("QT_QPA_PLATFORM", "xcb", true);

    qunsetenv("QT_DEVICE_PIXEL_RATIO");
    qunsetenv("QT_SCALE_FACTOR");
    QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    // KSMServer talks to us directly on DBus.
    QCoreApplication::setAttribute(Qt::AA_DisableSessionManager);
    // For sharing thumbnails between our scene graph and qtquick.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    // shared opengl contexts must have the same reset notification policy
    format.setOptions(QSurfaceFormat::ResetNotification);
    // disables vsync for any QtQuick windows we create (BUG 406180)
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);

    KWin::ApplicationX11 a(argc, argv);
    a.setupTranslator();
    // reset QT_QPA_PLATFORM so we don't propagate it to our children (e.g. apps launched from the overview effect)
    qunsetenv("QT_QPA_PLATFORM");

    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    KSignalHandler::self()->watchSignal(SIGHUP);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived,
                     &a, &QCoreApplication::exit);

    KWin::Application::createAboutData();

    QCommandLineOption replaceOption(QStringLiteral("replace"), i18n("Replace already-running ICCCM2.0-compliant window manager"));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(replaceOption);
#if KWIN_BUILD_ACTIVITIES
    QCommandLineOption noActivitiesOption(QStringLiteral("no-kactivities"),
                                          i18n("Disable KActivities integration."));
    parser.addOption(noActivitiesOption);
#endif

    parser.process(a);
    a.processCommandLine(&parser);
    a.setReplace(parser.isSet(replaceOption));
#if KWIN_BUILD_ACTIVITIES
    if (parser.isSet(noActivitiesOption)) {
        a.setUseKActivities(false);
    }
#endif

    // perform sanity checks
    if (a.platformName().toLower() != QStringLiteral("xcb")) {
        fprintf(stderr, "%s: FATAL ERROR expecting platform xcb but got platform %s\n",
                argv[0], qPrintable(a.platformName()));
        exit(1);
    }
    if (!QX11Info::display()) {
        fprintf(stderr, "%s: FATAL ERROR KWin requires Xlib support in the xcb plugin. Do not configure Qt with -no-xcb-xlib\n",
                argv[0]);
        exit(1);
    }

    a.setSession(KWin::Session::create(KWin::Session::Type::Noop));
    a.setOutputBackend(std::make_unique<KWin::X11StandaloneBackend>());
    a.start();

    return a.exec();
}

#include "main_x11.moc"
