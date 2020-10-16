/*
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aurorae.h"
#include "auroraetheme.h"
#include "config-kwin.h"
#include "kwineffectquickview.h"
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
 */
static const int s_indexMapper = 2;

QQmlComponent *Helper::component(const QString &themeName)
{
    // maybe it's an SVG theme?
    if (themeName.startsWith(QLatin1String("__aurorae__svg__"))) {
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
    , m_view(nullptr)
{
    m_themeName = findTheme(args);
    Helper::instance().ref();
    Helper::instance().rootContext()->setContextProperty(QStringLiteral("decorationSettings"), settings().data());
}

Decoration::~Decoration()
{
    delete m_qmlContext;
    delete m_view;
    Helper::instance().unref();
}

void Decoration::init()
{
    KDecoration2::Decoration::init();
    auto s = settings();
    connect(s.data(), &KDecoration2::DecorationSettings::reconfigured, this, &Decoration::configChanged);

    m_qmlContext = new QQmlContext(Helper::instance().rootContext(), this);
    m_qmlContext->setContextProperty(QStringLiteral("decoration"), this);
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
        };
        connect(this, &Decoration::configChanged, theme, readButtonSize);
        readButtonSize();
//         m_theme->setTabDragMimeType(tabDragMimeType());
        m_qmlContext->setContextProperty(QStringLiteral("auroraeTheme"), theme);
    }
    m_item = qobject_cast< QQuickItem* >(component->create(m_qmlContext));
    if (!m_item) {
        if (component->isError()) {
            const auto errors = component->errors();
            for (const auto &error: errors) {
                qCWarning(AURORAE) << error;
            }
        }
        return;
    }

    m_item->setParent(m_qmlContext);

    QVariant visualParent = property("visualParent");
    if (visualParent.isValid()) {
        m_item->setParentItem(visualParent.value<QQuickItem*>());
        visualParent.value<QQuickItem*>()->setProperty("drawBackground", false);
    } else {
        m_view = new KWin::EffectQuickView(this, KWin::EffectQuickView::ExportMode::Image);
        m_item->setParentItem(m_view->contentItem());
        auto updateSize = [this]() { m_item->setSize(m_view->contentItem()->size()); };
        updateSize();
        connect(m_view->contentItem(), &QQuickItem::widthChanged, m_item, updateSize);
        connect(m_view->contentItem(), &QQuickItem::heightChanged, m_item, updateSize);
        connect(m_view, &KWin::EffectQuickView::repaintNeeded, this, &Decoration::updateBuffer);
    }
    setupBorders(m_item);


    // TODO: Is there a more efficient way to react to border changes?
    auto trackBorders = [this](KWin::Borders *borders) {
        if (!borders) {
            return;
        }
        connect(borders, &KWin::Borders::leftChanged, this, &Decoration::updateBorders);
        connect(borders, &KWin::Borders::rightChanged, this, &Decoration::updateBorders);
        connect(borders, &KWin::Borders::topChanged, this, &Decoration::updateBorders);
        connect(borders, &KWin::Borders::bottomChanged, this, &Decoration::updateBorders);
    };
    trackBorders(m_borders);
    trackBorders(m_maximizedBorders);
    if (m_extendedBorders) {
        updateExtendedBorders();
        connect(m_extendedBorders, &KWin::Borders::leftChanged, this, &Decoration::updateExtendedBorders);
        connect(m_extendedBorders, &KWin::Borders::rightChanged, this, &Decoration::updateExtendedBorders);
        connect(m_extendedBorders, &KWin::Borders::topChanged, this, &Decoration::updateExtendedBorders);
        connect(m_extendedBorders, &KWin::Borders::bottomChanged, this, &Decoration::updateExtendedBorders);
    }

    auto decorationClient = clientPointer();
    connect(decorationClient, &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::updateBorders, Qt::QueuedConnection);
    connect(decorationClient, &KDecoration2::DecoratedClient::shadedChanged, this, &Decoration::updateBorders);
    updateBorders();
    if (m_view) {
        auto resizeWindow = [this] {
            QRect rect(QPoint(0, 0), size());
            if (m_padding && !clientPointer()->isMaximized()) {
                rect = rect.adjusted(-m_padding->left(), -m_padding->top(), m_padding->right(), m_padding->bottom());
            }
            m_view->setGeometry(rect);
        };
        connect(this, &Decoration::bordersChanged, this, resizeWindow);
        connect(decorationClient, &KDecoration2::DecoratedClient::widthChanged, this, resizeWindow);
        connect(decorationClient, &KDecoration2::DecoratedClient::heightChanged, this, resizeWindow);
        connect(decorationClient, &KDecoration2::DecoratedClient::maximizedChanged, this, resizeWindow);
        connect(decorationClient, &KDecoration2::DecoratedClient::shadedChanged, this, resizeWindow);
        resizeWindow();
        updateBuffer();
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
    if (clientPointer()->isMaximized() && m_maximizedBorders) {
        b = m_maximizedBorders;
    }
    if (!b) {
        return;
    }
    setBorders(*b);

    updateExtendedBorders();
}

void Decoration::paint(QPainter *painter, const QRect &repaintRegion)
{
    Q_UNUSED(repaintRegion)
    if (!m_view) {
        return;
    }
    painter->fillRect(rect(), Qt::transparent);
    painter->drawImage(rect(), m_view->bufferAsImage(), m_contentRect);
}

void Decoration::updateShadow()
{
    if (!m_view) {
        return;
    }
    bool updateShadow = false;
    const auto oldShadow = shadow();
    if (m_padding &&
            (m_padding->left() > 0 || m_padding->top() > 0 || m_padding->right() > 0 || m_padding->bottom() > 0) &&
            !clientPointer()->isMaximized()) {
        if (oldShadow.isNull()) {
            updateShadow = true;
        } else {
            // compare padding
            if (oldShadow->padding() != *m_padding) {
                updateShadow = true;
            }
        }
        const QImage m_buffer = m_view->bufferAsImage();

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

void Decoration::hoverEnterEvent(QHoverEvent *event)
{
    if (m_view) {
        event->setAccepted(false);
        m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::hoverEnterEvent(event);
}

void Decoration::hoverLeaveEvent(QHoverEvent *event)
{
    if (m_view) {
       m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::hoverLeaveEvent(event);
}

void Decoration::hoverMoveEvent(QHoverEvent *event)
{
    if (m_view) {
        // turn a hover event into a mouse because we don't follow hovers as we don't think we have focus
        QMouseEvent cloneEvent(QEvent::MouseMove, event->posF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        event->setAccepted(false);
        m_view->forwardMouseEvent(&cloneEvent);
        event->setAccepted(cloneEvent.isAccepted());
    }
    KDecoration2::Decoration::hoverMoveEvent(event);
}

void Decoration::mouseMoveEvent(QMouseEvent *event)
{
    if (m_view) {
        m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::mouseMoveEvent(event);
}

void Decoration::mousePressEvent(QMouseEvent *event)
{
    if (m_view) {
        m_view->forwardMouseEvent(event);
        if (event->button() == Qt::LeftButton) {
            if (!m_doubleClickTimer.hasExpired(QGuiApplication::styleHints()->mouseDoubleClickInterval())) {
                QMouseEvent dc(QEvent::MouseButtonDblClick, event->localPos(), event->windowPos(), event->screenPos(), event->button(), event->buttons(), event->modifiers());
                m_view->forwardMouseEvent(&dc);
            }
        }
        m_doubleClickTimer.invalidate();
    }
    KDecoration2::Decoration::mousePressEvent(event);
}

void Decoration::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_view) {
        m_view->forwardMouseEvent(event);
        if (event->isAccepted() && event->button() == Qt::LeftButton) {
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

void Decoration::updateExtendedBorders()
{
    // extended sizes
    const int extSize = settings()->largeSpacing();
    int extLeft = m_extendedBorders->left();
    int extRight = m_extendedBorders->right();
    int extBottom = m_extendedBorders->bottom();

    if (settings()->borderSize() == KDecoration2::BorderSize::None) {
        if (!clientPointer()->isMaximizedHorizontally()) {
            extLeft = qMax(m_extendedBorders->left(), extSize);
            extRight = qMax(m_extendedBorders->right(), extSize);
        }
        if (!clientPointer()->isMaximizedVertically()) {
            extBottom = qMax(m_extendedBorders->bottom(), extSize);
        }

    } else if (settings()->borderSize() == KDecoration2::BorderSize::NoSides && !clientPointer()->isMaximizedHorizontally() ) {
        extLeft = qMax(m_extendedBorders->left(), extSize);
        extRight = qMax(m_extendedBorders->right(), extSize);
    }

    setResizeOnlyBorders(QMargins(extLeft, 0, extRight, extBottom));
}

void Decoration::updateBuffer()
{
    m_contentRect = QRect(QPoint(0, 0), m_view->bufferAsImage().size());
    if (m_padding &&
            (m_padding->left() > 0 || m_padding->top() > 0 || m_padding->right() > 0 || m_padding->bottom() > 0) &&
            !clientPointer()->isMaximized()) {
        m_contentRect = m_contentRect.adjusted(m_padding->left(), m_padding->top(), -m_padding->right(), -m_padding->bottom());
    }
    updateShadow();
    update();
}

KDecoration2::DecoratedClient *Decoration::clientPointer() const
{
    return client().toStrongRef().data();
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
