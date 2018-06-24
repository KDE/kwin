/********************************************************************
Copyright (C) 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "aurorae.h"
#include "auroraetheme.h"
#include "config-kwin.h"
// qml imports
#include "decorationoptions.h"
// KDecoration2
#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationSettings>
#include <KDecoration2/DecorationShadow>
// KDE
#include <KConfigGroup>
#include <KConfigLoader>
#include <KConfigDialogManager>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KLocalizedTranslator>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KPackage/PackageLoader>
// Qt
#include <QDebug>
#include <QComboBox>
#include <QDirIterator>
#include <QGuiApplication>
#include <QLabel>
#include <QStyleHints>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QTimer>
#include <QUiLoader>
#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(AuroraeDecoFactory,
                           "aurorae.json",
                           registerPlugin<Aurorae::Decoration>();
                           registerPlugin<Aurorae::ThemeFinder>(QStringLiteral("themes"));
                           registerPlugin<Aurorae::ConfigurationModule>(QStringLiteral("kcmodule"));
                          )

namespace Aurorae
{

class Helper
{
public:
    void ref();
    void unref();
    QQmlComponent *component(const QString &theme);
    QQmlContext *rootContext();
    QQmlComponent *svgComponent() {
        return m_svgComponent.data();
    }

    static Helper &instance();
private:
    Helper() = default;
    void init();
    QQmlComponent *loadComponent(const QString &themeName);
    int m_refCount = 0;
    QScopedPointer<QQmlEngine> m_engine;
    QHash<QString, QQmlComponent*> m_components;
    QScopedPointer<QQmlComponent> m_svgComponent;
};

Helper &Helper::instance()
{
    static Helper s_helper;
    return s_helper;
}

void Helper::ref()
{
    m_refCount++;
    if (m_refCount == 1) {
        m_engine.reset(new QQmlEngine);
        init();
    }
}

void Helper::unref()
{
    m_refCount--;
    if (m_refCount == 0) {
        // cleanup
        m_svgComponent.reset();
        m_engine.reset();
        m_components.clear();
    }
}

static const QString s_defaultTheme = QStringLiteral("kwin4_decoration_qml_plastik");
static const QString s_qmlPackageFolder = QStringLiteral(KWIN_NAME "/decorations/");
/*
 * KDecoration2::BorderSize doesn't map to the indices used for the Aurorae SVG Button Sizes.
 * BorderSize defines None and NoSideBorder as index 0 and 1. These do not make sense for Button
 * Size, thus we need to perform a mapping between the enum value and the config value.
 **/
static const int s_indexMapper = 2;

QQmlComponent *Helper::component(const QString &themeName)
{
    // maybe it's an SVG theme?
    if (themeName.startsWith(QLatin1Literal("__aurorae__svg__"))) {
        if (m_svgComponent.isNull()) {
            /* use logic from KDeclarative::setupBindings():
            "addImportPath adds the path at the beginning, so to honour user's
            paths we need to traverse the list in reverse order" */
            QStringListIterator paths(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("module/imports"), QStandardPaths::LocateDirectory));
            paths.toBack();
            while (paths.hasPrevious()) {
                m_engine->addImportPath(paths.previous());
            }
            m_svgComponent.reset(new QQmlComponent(m_engine.data()));
            m_svgComponent->loadUrl(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/aurorae/aurorae.qml"))));
        }
        // verify that the theme exists
        if (!QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("aurorae/themes/%1/%1rc").arg(themeName.mid(16))).isEmpty()) {
            return m_svgComponent.data();
        }
    }
    // try finding the QML package
    auto it = m_components.constFind(themeName);
    if (it != m_components.constEnd()) {
        return it.value();
    }
    auto component = loadComponent(themeName);
    if (component) {
        m_components.insert(themeName, component);
        return component;
    }
    // try loading default component
    if (themeName != s_defaultTheme) {
        return loadComponent(s_defaultTheme);
    }
    return nullptr;
}

QQmlComponent *Helper::loadComponent(const QString &themeName)
{
    qCDebug(AURORAE) << "Trying to load QML Decoration " << themeName;
    const QString internalname = themeName.toLower();

    const auto offers = KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Decoration"), s_qmlPackageFolder,
        [internalname] (const KPluginMetaData &data) {
            return data.pluginId().compare(internalname, Qt::CaseInsensitive) == 0;
        }
    );
    if (offers.isEmpty()) {
        qCCritical(AURORAE) << "Couldn't find QML Decoration " << themeName;
        // TODO: what to do in error case?
        return nullptr;
    }
    const KPluginMetaData &service = offers.first();
    const QString pluginName = service.pluginId();
    const QString scriptName = service.value(QStringLiteral("X-Plasma-MainScript"));
    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, s_qmlPackageFolder + pluginName + QLatin1String("/contents/") + scriptName);
    if (file.isNull()) {
        qCDebug(AURORAE) << "Could not find script file for " << pluginName;
        // TODO: what to do in error case?
        return nullptr;
    }
    // setup the QML engine
    /* use logic from KDeclarative::setupBindings():
    "addImportPath adds the path at the beginning, so to honour user's
     paths we need to traverse the list in reverse order" */
    QStringListIterator paths(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("module/imports"), QStandardPaths::LocateDirectory));
    paths.toBack();
    while (paths.hasPrevious()) {
        m_engine->addImportPath(paths.previous());
    }
    QQmlComponent *component = new QQmlComponent(m_engine.data(), m_engine.data());
    component->loadUrl(QUrl::fromLocalFile(file));
    return component;
}

QQmlContext *Helper::rootContext()
{
    return m_engine->rootContext();
}

void Helper::init()
{
    // we need to first load our decoration plugin
    // once it's loaded we can provide the Borders and access them from C++ side
    // so let's try to locate our plugin:
    QString pluginPath;
    for (const QString &path : m_engine->importPathList()) {
        QDirIterator it(path, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QFileInfo fileInfo = it.fileInfo();
            if (!fileInfo.isFile()) {
                continue;
            }
            if (!fileInfo.path().endsWith(QLatin1String("/org/kde/kwin/decoration"))) {
                continue;
            }
            if (fileInfo.fileName() == QLatin1String("libdecorationplugin.so")) {
                pluginPath = fileInfo.absoluteFilePath();
                break;
            }
        }
        if (!pluginPath.isEmpty()) {
            break;
        }
    }
    m_engine->importPlugin(pluginPath, "org.kde.kwin.decoration", nullptr);
    qmlRegisterType<KWin::Borders>("org.kde.kwin.decoration", 0, 1, "Borders");

    qmlRegisterType<KDecoration2::Decoration>();
    qmlRegisterType<KDecoration2::DecoratedClient>();
    qRegisterMetaType<KDecoration2::BorderSize>();
}

static QString findTheme(const QVariantList &args)
{
    if (args.isEmpty()) {
        return QString();
    }
    const auto map = args.first().toMap();
    auto it = map.constFind(QStringLiteral("theme"));
    if (it == map.constEnd()) {
        return QString();
    }
    return it.value().toString();
}

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration2::Decoration(parent, args)
    , m_item(nullptr)
    , m_borders(nullptr)
    , m_maximizedBorders(nullptr)
    , m_extendedBorders(nullptr)
    , m_padding(nullptr)
    , m_themeName(s_defaultTheme)
{
    m_themeName = findTheme(args);
    Helper::instance().ref();
}

Decoration::~Decoration()
{
    Helper::instance().unref();
    if (m_context) {
        m_context->makeCurrent(m_offscreenSurface.data());

        delete m_renderControl;
        delete m_view.data();
        m_fbo.reset();
        delete m_item;

        m_context->doneCurrent();
    }
}

void Decoration::init()
{
    KDecoration2::Decoration::init();
    auto s = settings();
    connect(s.data(), &KDecoration2::DecorationSettings::reconfigured, this, &Decoration::configChanged);

    QQmlContext *context = new QQmlContext(Helper::instance().rootContext(), this);
    context->setContextProperty(QStringLiteral("decoration"), this);
    context->setContextProperty(QStringLiteral("decorationSettings"), s.data());
    auto component = Helper::instance().component(m_themeName);
    if (!component) {
        return;
    }
    if (component == Helper::instance().svgComponent()) {
        // load SVG theme
        const QString themeName = m_themeName.mid(16);
        KConfig config(QLatin1String("aurorae/themes/") + themeName + QLatin1Char('/') + themeName + QLatin1String("rc"),
                       KConfig::FullConfig, QStandardPaths::GenericDataLocation);
        AuroraeTheme *theme = new AuroraeTheme(this);
        theme->loadTheme(themeName, config);
        theme->setBorderSize(s->borderSize());
        connect(s.data(), &KDecoration2::DecorationSettings::borderSizeChanged, theme, &AuroraeTheme::setBorderSize);
        auto readButtonSize = [this, theme] {
            const KSharedConfigPtr conf = KSharedConfig::openConfig(QStringLiteral("auroraerc"));
            const KConfigGroup themeGroup(conf, m_themeName.mid(16));
            theme->setButtonSize((KDecoration2::BorderSize)(themeGroup.readEntry<int>("ButtonSize",
                                                                                      int(KDecoration2::BorderSize::Normal) - s_indexMapper) + s_indexMapper));
            updateBorders();
        };
        connect(this, &Decoration::configChanged, theme, readButtonSize);
        readButtonSize();
//         m_theme->setTabDragMimeType(tabDragMimeType());
        context->setContextProperty(QStringLiteral("auroraeTheme"), theme);
    }
    m_item = qobject_cast< QQuickItem* >(component->create(context));
    if (!m_item) {
        return;
    }
    m_item->setParent(this);

    QVariant visualParent = property("visualParent");
    if (visualParent.isValid()) {
        m_item->setParentItem(visualParent.value<QQuickItem*>());
        visualParent.value<QQuickItem*>()->setProperty("drawBackground", false);
    } else {
        m_renderControl = new QQuickRenderControl(this);
        m_view = new QQuickWindow(m_renderControl);
        bool usingGL = m_view->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL;
        m_view->setColor(Qt::transparent);
        m_view->setFlags(Qt::FramelessWindowHint);
        if (usingGL) {
            // first create the context
            QSurfaceFormat format;
            format.setDepthBufferSize(16);
            format.setStencilBufferSize(8);
            m_context.reset(new QOpenGLContext);
            m_context->setFormat(format);
            m_context->create();
            // and the offscreen surface
            m_offscreenSurface.reset(new QOffscreenSurface);
            m_offscreenSurface->setFormat(m_context->format());
            m_offscreenSurface->create();

        }

        //workaround for https://codereview.qt-project.org/#/c/207198/
#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
        if (!usingGL) {
            m_renderControl->sync();
        }
#endif
        // delay rendering a little bit for better performance
        m_updateTimer.reset(new QTimer);
        m_updateTimer->setSingleShot(true);
        m_updateTimer->setInterval(5);
        connect(m_updateTimer.data(), &QTimer::timeout, this,
            [this, usingGL] {
                if (usingGL) {
                    if (!m_context->makeCurrent(m_offscreenSurface.data())) {
                        return;
                    }
                    if (m_fbo.isNull() || m_fbo->size() != m_view->size()) {
                        m_fbo.reset(new QOpenGLFramebufferObject(m_view->size(), QOpenGLFramebufferObject::CombinedDepthStencil));
                        if (!m_fbo->isValid()) {
                            qCWarning(AURORAE) << "Creating FBO as render target failed";
                            m_fbo.reset();
                            return;
                        }
                    }
                    m_view->setRenderTarget(m_fbo.data());
                    m_view->resetOpenGLState();
                }

                m_buffer = m_renderControl->grab();

                m_contentRect = QRect(QPoint(0, 0), m_buffer.size());
                if (m_padding &&
                        (m_padding->left() > 0 || m_padding->top() > 0 || m_padding->right() > 0 || m_padding->bottom() > 0) &&
                        !client().data()->isMaximized()) {
                    m_contentRect = m_contentRect.adjusted(m_padding->left(), m_padding->top(), -m_padding->right(), -m_padding->bottom());
                }
                updateShadow();

                QOpenGLFramebufferObject::bindDefault();
                update();
            }
        );
        auto requestUpdate = [this] {
            if (m_updateTimer->isActive()) {
                return;
            }
            m_updateTimer->start();
        };
        connect(m_renderControl, &QQuickRenderControl::renderRequested, this, requestUpdate);
        connect(m_renderControl, &QQuickRenderControl::sceneChanged, this, requestUpdate);

        m_item->setParentItem(m_view->contentItem());

        if (usingGL) {
            m_context->makeCurrent(m_offscreenSurface.data());
            m_renderControl->initialize(m_context.data());
            m_context->doneCurrent();
        }
    }
    setupBorders(m_item);
    if (m_extendedBorders) {
        auto updateExtendedBorders = [this] {
            setResizeOnlyBorders(*m_extendedBorders);
        };
        updateExtendedBorders();
        connect(m_extendedBorders, &KWin::Borders::leftChanged, this, updateExtendedBorders);
        connect(m_extendedBorders, &KWin::Borders::rightChanged, this, updateExtendedBorders);
        connect(m_extendedBorders, &KWin::Borders::topChanged, this, updateExtendedBorders);
        connect(m_extendedBorders, &KWin::Borders::bottomChanged, this, updateExtendedBorders);
    }
    connect(client().data(), &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::updateBorders, Qt::QueuedConnection);
    connect(client().data(), &KDecoration2::DecoratedClient::shadedChanged, this, &Decoration::updateBorders);
    updateBorders();
    if (!m_view.isNull()) {
        auto resizeWindow = [this] {
            QRect rect(QPoint(0, 0), size());
            if (m_padding && !client().data()->isMaximized()) {
                rect = rect.adjusted(-m_padding->left(), -m_padding->top(), m_padding->right(), m_padding->bottom());
            }
            m_view->setGeometry(rect);
        };
        connect(this, &Decoration::bordersChanged, this, resizeWindow);
        connect(client().data(), &KDecoration2::DecoratedClient::widthChanged, this, resizeWindow);
        connect(client().data(), &KDecoration2::DecoratedClient::heightChanged, this, resizeWindow);
        connect(client().data(), &KDecoration2::DecoratedClient::maximizedChanged, this, resizeWindow);
        connect(client().data(), &KDecoration2::DecoratedClient::shadedChanged, this, resizeWindow);
        resizeWindow();
    } else {
        // create a dummy shadow for the configuration interface
        if (m_padding) {
            auto s = QSharedPointer<KDecoration2::DecorationShadow>::create();
            s->setPadding(*m_padding);
            s->setInnerShadowRect(QRect(m_padding->left(), m_padding->top(), 1, 1));
            setShadow(s);
        }
    }
}

QVariant Decoration::readConfig(const QString &key, const QVariant &defaultValue)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("auroraerc"));
    return config->group(m_themeName).readEntry(key, defaultValue);
}

void Decoration::setupBorders(QQuickItem *item)
{
    m_borders          = item->findChild<KWin::Borders*>(QStringLiteral("borders"));
    m_maximizedBorders = item->findChild<KWin::Borders*>(QStringLiteral("maximizedBorders"));
    m_extendedBorders  = item->findChild<KWin::Borders*>(QStringLiteral("extendedBorders"));
    m_padding          = item->findChild<KWin::Borders*>(QStringLiteral("padding"));
}

void Decoration::updateBorders()
{
    KWin::Borders *b = m_borders;
    if (client().data()->isMaximized() && m_maximizedBorders) {
        b = m_maximizedBorders;
    }
    if (!b) {
        return;
    }
    setBorders(*b);
}

void Decoration::paint(QPainter *painter, const QRect &repaintRegion)
{
    Q_UNUSED(repaintRegion)
    painter->fillRect(rect(), Qt::transparent);
    painter->drawImage(rect(), m_buffer, m_contentRect);
}

void Decoration::updateShadow()
{
    bool updateShadow = false;
    const auto oldShadow = shadow();
    if (m_padding &&
            (m_padding->left() > 0 || m_padding->top() > 0 || m_padding->right() > 0 || m_padding->bottom() > 0) &&
            !client().data()->isMaximized()) {
        if (oldShadow.isNull()) {
            updateShadow = true;
        } else {
            // compare padding
            if (oldShadow->padding() != *m_padding) {
                updateShadow = true;
            }
        }
        QImage img(m_buffer.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        // top
        p.drawImage(0, 0, m_buffer, 0, 0, img.width(), m_padding->top());
        // left
        p.drawImage(0, m_padding->top(), m_buffer, 0, m_padding->top(), m_padding->left(), m_buffer.height() - m_padding->top());
        // bottom
        p.drawImage(m_padding->left(), m_buffer.height() - m_padding->bottom(), m_buffer,
                    m_padding->left(), m_buffer.height() - m_padding->bottom(),
                    m_buffer.width() - m_padding->left(), m_padding->bottom());
        // right
        p.drawImage(m_buffer.width() - m_padding->right(), m_padding->top(), m_buffer,
                    m_buffer.width() - m_padding->right(), m_padding->top(),
                    m_padding->right(), m_buffer.height() - m_padding->top() - m_padding->bottom());
        if (!updateShadow) {
            updateShadow = (oldShadow->shadow() != img);
        }
        if (updateShadow) {
            auto s = QSharedPointer<KDecoration2::DecorationShadow>::create();
            s->setShadow(img);
            s->setPadding(*m_padding);
            s->setInnerShadowRect(QRect(m_padding->left(),
                                        m_padding->top(),
                                        m_buffer.width() - m_padding->left() - m_padding->right(),
                                        m_buffer.height() - m_padding->top() - m_padding->bottom()));
            setShadow(s);
        }
    } else {
        if (!oldShadow.isNull()) {
            setShadow(QSharedPointer<KDecoration2::DecorationShadow>());
        }
    }
}


QMouseEvent Decoration::translatedMouseEvent(QMouseEvent *orig)
{
    if (!m_padding || client().data()->isMaximized()) {
        orig->setAccepted(false);
        return *orig;
    }
    QMouseEvent event(orig->type(), orig->localPos() + QPointF(m_padding->left(), m_padding->top()), orig->button(), orig->buttons(), orig->modifiers());
    event.setAccepted(false);
    return event;
}

void Decoration::hoverEnterEvent(QHoverEvent *event)
{
    if (m_view) {
        event->setAccepted(false);
        QCoreApplication::sendEvent(m_view.data(), event);
    }
    KDecoration2::Decoration::hoverEnterEvent(event);
}

void Decoration::hoverLeaveEvent(QHoverEvent *event)
{
    if (m_view) {
        event->setAccepted(false);
        QCoreApplication::sendEvent(m_view.data(), event);
    }
    KDecoration2::Decoration::hoverLeaveEvent(event);
}

void Decoration::hoverMoveEvent(QHoverEvent *event)
{
    if (m_view) {
        QMouseEvent mouseEvent(QEvent::MouseMove, event->posF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QMouseEvent ev = translatedMouseEvent(&mouseEvent);
        QCoreApplication::sendEvent(m_view.data(), &ev);
        event->setAccepted(ev.isAccepted());
    }
    KDecoration2::Decoration::hoverMoveEvent(event);
}

void Decoration::mouseMoveEvent(QMouseEvent *event)
{
    if (m_view) {
        QMouseEvent ev = translatedMouseEvent(event);
        QCoreApplication::sendEvent(m_view.data(), &ev);
        event->setAccepted(ev.isAccepted());
    }
    KDecoration2::Decoration::mouseMoveEvent(event);
}

void Decoration::mousePressEvent(QMouseEvent *event)
{
    if (m_view) {
        QMouseEvent ev = translatedMouseEvent(event);
        QCoreApplication::sendEvent(m_view.data(), &ev);
        if (ev.button() == Qt::LeftButton) {
            if (!m_doubleClickTimer.hasExpired(QGuiApplication::styleHints()->mouseDoubleClickInterval())) {
                QMouseEvent dc(QEvent::MouseButtonDblClick, ev.localPos(), ev.windowPos(), ev.screenPos(), ev.button(), ev.buttons(), ev.modifiers());
                QCoreApplication::sendEvent(m_view.data(), &dc);
            }
        }
        m_doubleClickTimer.invalidate();
        event->setAccepted(ev.isAccepted());
    }
    KDecoration2::Decoration::mousePressEvent(event);
}

void Decoration::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_view) {
        QMouseEvent ev = translatedMouseEvent(event);
        QCoreApplication::sendEvent(m_view.data(), &ev);
        event->setAccepted(ev.isAccepted());
        if (ev.isAccepted() && ev.button() == Qt::LeftButton) {
            m_doubleClickTimer.start();
        }
    }
    KDecoration2::Decoration::mouseReleaseEvent(event);
}

void Decoration::installTitleItem(QQuickItem *item)
{
    auto update = [this, item] {
        QRect rect = item->mapRectToScene(item->childrenRect()).toRect();
        if (rect.isNull()) {
            rect = item->parentItem()->mapRectToScene(QRectF(item->x(), item->y(), item->width(), item->height())).toRect();
        }
        setTitleBar(rect);
    };
    update();
    connect(item, &QQuickItem::widthChanged, this, update);
    connect(item, &QQuickItem::heightChanged, this, update);
    connect(item, &QQuickItem::xChanged, this, update);
    connect(item, &QQuickItem::yChanged, this, update);
}

KDecoration2::DecoratedClient *Decoration::clientPointer() const
{
    return client().data();
}

ThemeFinder::ThemeFinder(QObject *parent, const QVariantList &args)
    : QObject(parent)
{
    Q_UNUSED(args)
    init();
}

void ThemeFinder::init()
{
    findAllQmlThemes();
    findAllSvgThemes();
}

void ThemeFinder::findAllQmlThemes()
{
    const auto offers = KPackage::PackageLoader::self()->findPackages(QStringLiteral("KWin/Decoration"), s_qmlPackageFolder);
    for (const auto &offer : offers) {
        m_themes.insert(offer.name(), offer.pluginId());
    }
}

void ThemeFinder::findAllSvgThemes()
{
    QStringList themes;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("aurorae/themes/"), QStandardPaths::LocateDirectory);
    QStringList themeDirectories;
    for (const QString &dir : dirs) {
        QDir directory = QDir(dir);
        for (const QString &themeDir : directory.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
            themeDirectories << dir + themeDir;
        }
    }
    for (const QString &dir : themeDirectories) {
        for (const QString & file : QDir(dir).entryList(QStringList() << QStringLiteral("metadata.desktop"))) {
            themes.append(dir + '/' + file);
        }
    }
    for (const QString & theme : themes) {
        int themeSepIndex = theme.lastIndexOf('/', -1);
        QString themeRoot = theme.left(themeSepIndex);
        int themeNameSepIndex = themeRoot.lastIndexOf('/', -1);
        QString packageName = themeRoot.right(themeRoot.length() - themeNameSepIndex - 1);

        KDesktopFile df(theme);
        QString name = df.readName();
        if (name.isEmpty()) {
            name = packageName;
        }

        m_themes.insert(name, QString(QLatin1String("__aurorae__svg__") + packageName));
    }
}

static const QString s_configUiPath = QStringLiteral("kwin/decorations/%1/contents/ui/config.ui");
static const QString s_configXmlPath = QStringLiteral("kwin/decorations/%1/contents/config/main.xml");

bool ThemeFinder::hasConfiguration(const QString &theme) const
{
    if (theme.startsWith(QLatin1String("__aurorae__svg__"))) {
        return true;
    }
    const QString ui = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                              s_configUiPath.arg(theme));
    const QString xml = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                               s_configXmlPath.arg(theme));
    return !(ui.isEmpty() || xml.isEmpty());
}

ConfigurationModule::ConfigurationModule(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_theme(findTheme(args))
    , m_buttonSize(int(KDecoration2::BorderSize::Normal) - s_indexMapper)
{
    setLayout(new QVBoxLayout(this));
    init();
}

void ConfigurationModule::init()
{
    if (m_theme.startsWith(QLatin1String("__aurorae__svg__"))) {
        // load the generic setting module
        initSvg();
    } else {
        initQml();
    }
}

void ConfigurationModule::initSvg()
{
    QWidget *form = new QWidget(this);
    form->setLayout(new QHBoxLayout(form));
    QComboBox *sizes = new QComboBox(form);
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Tiny"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Normal"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Large"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Very Large"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Huge"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Very Huge"));
    sizes->addItem(i18nc("@item:inlistbox Button size:", "Oversized"));
    sizes->setObjectName(QStringLiteral("kcfg_ButtonSize"));

    QLabel *label = new QLabel(i18n("Button size:"), form);
    label->setBuddy(sizes);
    form->layout()->addWidget(label);
    form->layout()->addWidget(sizes);

    layout()->addWidget(form);

    KCoreConfigSkeleton *skel = new KCoreConfigSkeleton(KSharedConfig::openConfig(QStringLiteral("auroraerc")), this);
    skel->setCurrentGroup(m_theme.mid(16));
    skel->addItemInt(QStringLiteral("ButtonSize"),
                     m_buttonSize,
                     int(KDecoration2::BorderSize::Normal) - s_indexMapper,
                     QStringLiteral("ButtonSize"));
    addConfig(skel, form);
}

void ConfigurationModule::initQml()
{
    const QString ui = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                              s_configUiPath.arg(m_theme));
    const QString xml = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                               s_configXmlPath.arg(m_theme));
    if (ui.isEmpty() || xml.isEmpty()) {
        return;
    }
    KLocalizedTranslator *translator = new KLocalizedTranslator(this);
    QCoreApplication::instance()->installTranslator(translator);
    const KDesktopFile metaData(QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                      QStringLiteral("kwin/decorations/%1/metadata.desktop").arg(m_theme)));
    const QString translationDomain = metaData.desktopGroup().readEntry("X-KWin-Config-TranslationDomain", QString());
    if (!translationDomain.isEmpty()) {
        translator->setTranslationDomain(translationDomain);
    }
    // load the KConfigSkeleton
    QFile configFile(xml);
    KSharedConfigPtr auroraeConfig = KSharedConfig::openConfig("auroraerc");
    KConfigGroup configGroup = auroraeConfig->group(m_theme);
    m_skeleton = new KConfigLoader(configGroup, &configFile, this);
    // load the ui file
    QUiLoader *loader = new QUiLoader(this);
    loader->setLanguageChangeEnabled(true);
    QFile uiFile(ui);
    uiFile.open(QFile::ReadOnly);
    QWidget *customConfigForm = loader->load(&uiFile, this);
    translator->addContextToMonitor(customConfigForm->objectName());
    uiFile.close();
    layout()->addWidget(customConfigForm);
    // connect the ui file with the skeleton
    addConfig(m_skeleton, customConfigForm);

    // send a custom event to the translator to retranslate using our translator
    QEvent le(QEvent::LanguageChange);
    QCoreApplication::sendEvent(customConfigForm, &le);
}

}

#include "aurorae.moc"
