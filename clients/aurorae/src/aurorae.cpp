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

#include <QApplication>
#include <QDebug>
#include <QMutex>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QWidget>

#include <KConfig>
#include <KConfigGroup>
#include <KPluginInfo>
#include <KServiceTypeTrader>

namespace Aurorae
{

AuroraeFactory::AuroraeFactory(QObject *parent)
        : KDecorationFactory(parent)
        , m_theme(new AuroraeTheme(this))
        , m_engine(new QQmlEngine(this))
        , m_component(new QQmlComponent(m_engine, this))
        , m_engineType(AuroraeEngine)
{
    init();
    connect(options(), &KDecorationOptions::buttonsChanged, this, &AuroraeFactory::buttonsChanged);
    connect(options(), &KDecorationOptions::fontsChanged, this, &AuroraeFactory::titleFontChanged);
    connect(options(), &KDecorationOptions::configChanged, this, &AuroraeFactory::updateConfiguration);
}

void AuroraeFactory::init()
{
    qRegisterMetaType<uint>("Qt::MouseButtons");

    KConfig conf(QStringLiteral("auroraerc"));
    KConfigGroup group(&conf, "Engine");
    if (!group.hasKey("EngineType") && !group.hasKey("ThemeName")) {
        // neither engine type and theme name are configured, use the only available theme
        initQML(group);
    } else if (group.hasKey("EngineType")) {
        const QString engineType = group.readEntry("EngineType", "aurorae").toLower();
        if (engineType == QStringLiteral("qml")) {
            initQML(group);
        } else {
            // fallback to classic Aurorae Themes
            initAurorae(conf, group);
        }
    } else {
        // fallback to classic Aurorae Themes
        initAurorae(conf, group);
    }
}

void AuroraeFactory::initAurorae(KConfig &conf, KConfigGroup &group)
{
    m_engineType = AuroraeEngine;
    const QString themeName = group.readEntry("ThemeName");
    if (themeName.isEmpty()) {
        // no theme configured, fall back to Plastik QML theme
        initQML(group);
        return;
    }
    KConfig config(QStringLiteral("aurorae/themes/") + themeName + QStringLiteral("/") + themeName + QStringLiteral("rc"),
                   KConfig::FullConfig, QStandardPaths::GenericDataLocation);
    KConfigGroup themeGroup(&conf, themeName);
    m_theme->loadTheme(themeName, config);
    m_theme->setBorderSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("BorderSize", KDecorationDefines::BorderNormal));
    m_theme->setButtonSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("ButtonSize", KDecorationDefines::BorderNormal));
    m_theme->setTabDragMimeType(tabDragMimeType());
    // setup the QML engine
    /* use logic from KDeclarative::setupBindings():
    "addImportPath adds the path at the beginning, so to honour user's
     paths we need to traverse the list in reverse order" */
    QStringListIterator paths(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("module/imports"), QStandardPaths::LocateDirectory));
    paths.toBack();
    while (paths.hasPrevious()) {
        m_engine->addImportPath(paths.previous());
    }
    m_component->loadUrl(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/aurorae/aurorae.qml"))));
    m_engine->rootContext()->setContextProperty(QStringLiteral("auroraeTheme"), m_theme);
    m_themeName = themeName;
}

void AuroraeFactory::initQML(const KConfigGroup &group)
{
    // try finding the QML package
    const QString themeName = group.readEntry("ThemeName", "kwin4_decoration_qml_plastik");
    qCDebug(AURORAE) << "Trying to load QML Decoration " << themeName;
    const QString internalname = themeName.toLower();

    QString constraint = QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(internalname);
    KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("KWin/Decoration"), constraint);
    if (offers.isEmpty()) {
        qCCritical(AURORAE) << "Couldn't find QML Decoration " << themeName << endl;
        // TODO: what to do in error case?
        return;
    }
    KService::Ptr service = offers.first();
    KPluginInfo plugininfo(service);
    const QString pluginName = service->property(QStringLiteral("X-KDE-PluginInfo-Name")).toString();
    const QString scriptName = service->property(QStringLiteral("X-Plasma-MainScript")).toString();
    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral(KWIN_NAME) + QStringLiteral("/decorations/") + pluginName + QStringLiteral("/contents/") + scriptName);
    if (file.isNull()) {
        qCDebug(AURORAE) << "Could not find script file for " << pluginName;
        // TODO: what to do in error case?
        return;
    }
    m_engineType = QMLEngine;
    // setup the QML engine
    /* use logic from KDeclarative::setupBindings():
    "addImportPath adds the path at the beginning, so to honour user's
     paths we need to traverse the list in reverse order" */
    QStringListIterator paths(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("module/imports"), QStandardPaths::LocateDirectory));
    paths.toBack();
    while (paths.hasPrevious()) {
        m_engine->addImportPath(paths.previous());
    }
    m_component->loadUrl(QUrl::fromLocalFile(file));
    m_themeName = themeName;
}

AuroraeFactory::~AuroraeFactory()
{
    s_instance = NULL;
}

AuroraeFactory *AuroraeFactory::instance()
{
    if (!s_instance) {
        s_instance = new AuroraeFactory;
    }

    return s_instance;
}

void AuroraeFactory::updateConfiguration()
{
    const KConfig conf(QStringLiteral("auroraerc"));
    const KConfigGroup group(&conf, "Engine");
    const QString themeName = group.readEntry("ThemeName", "example-deco");
    const KConfig config(QStringLiteral("aurorae/themes/") + themeName + QStringLiteral("/") + themeName + QStringLiteral("rc"),
                         KConfig::FullConfig, QStandardPaths::GenericDataLocation);
    const KConfigGroup themeGroup(&conf, themeName);
    if (themeName != m_themeName) {
        m_engine->clearComponentCache();
        init();
        emit recreateDecorations();
    }
    if (m_engineType == AuroraeEngine) {
        m_theme->setBorderSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("BorderSize", KDecorationDefines::BorderNormal));
        m_theme->setButtonSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("ButtonSize", KDecorationDefines::BorderNormal));
    }
    emit configChanged();
}

bool AuroraeFactory::supports(Ability ability) const
{
    switch (ability) {
    case AbilityAnnounceButtons:
    case AbilityUsesAlphaChannel:
    case AbilityAnnounceAlphaChannel:
    case AbilityButtonMenu:
    case AbilityButtonSpacer:
    case AbilityExtendIntoClientArea:
    case AbilityButtonMinimize:
    case AbilityButtonMaximize:
    case AbilityButtonClose:
    case AbilityButtonAboveOthers:
    case AbilityButtonBelowOthers:
    case AbilityButtonShade:
    case AbilityButtonOnAllDesktops:
    case AbilityButtonHelp:
    case AbilityButtonApplicationMenu:
    case AbilityProvidesShadow:
        return true; // TODO: correct value from theme
    case AbilityTabbing:
        return false;
    case AbilityUsesBlurBehind:
        return true;
    default:
        return false;
    }
}

KDecoration *AuroraeFactory::createDecoration(KDecorationBridge *bridge)
{
    AuroraeClient *client = new AuroraeClient(bridge, this);
    return client;
}

QList< KDecorationDefines::BorderSize > AuroraeFactory::borderSizes() const
{
    return QList< BorderSize >() << BorderTiny << BorderNormal <<
        BorderLarge << BorderVeryLarge <<  BorderHuge <<
        BorderVeryHuge << BorderOversized;
}

QQuickItem *AuroraeFactory::createQmlDecoration(Aurorae::AuroraeClient *client)
{
    QQmlContext *context = new QQmlContext(m_engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("decoration"), client);
    return qobject_cast< QQuickItem* >(m_component->create(context));
}

AuroraeFactory *AuroraeFactory::s_instance = NULL;

/*******************************************************
* Client
*******************************************************/
AuroraeClient::AuroraeClient(KDecorationBridge *bridge, KDecorationFactory *factory)
    : KDecoration(bridge, factory)
    , m_view(NULL)
    , m_item(AuroraeFactory::instance()->createQmlDecoration(this))
    , m_mutex(new QMutex(QMutex::Recursive))
{
    connect(AuroraeFactory::instance(), SIGNAL(buttonsChanged()), SIGNAL(buttonsChanged()));
    connect(AuroraeFactory::instance(), SIGNAL(configChanged()), SIGNAL(configChanged()));
    connect(AuroraeFactory::instance(), SIGNAL(titleFontChanged()), SIGNAL(fontChanged()));
    connect(m_item.data(), SIGNAL(alphaChanged()), SLOT(slotAlphaChanged()));
    connect(this, SIGNAL(appMenuAvailable()), SIGNAL(appMenuAvailableChanged()));
    connect(this, SIGNAL(appMenuUnavailable()), SIGNAL(appMenuAvailableChanged()));
}

AuroraeClient::~AuroraeClient()
{
}

void AuroraeClient::init()
{
    m_view = new QQuickWindow();
    m_view->setFlags(initialWFlags());
    m_view->setColor(Qt::transparent);
    setMainWindow(m_view);
    if (compositingActive()) {
        connect(m_view, &QQuickWindow::beforeRendering, [this]() {
            int left, right, top, bottom;
            left = right = top = bottom = 0;
            padding(left, right, top, bottom);
            if (m_fbo.isNull() || m_fbo->size() != QSize(width() + left + right, height() + top + bottom)) {
                m_fbo.reset(new QOpenGLFramebufferObject(QSize(width() + left + right, height() + top + bottom),
                                                         QOpenGLFramebufferObject::CombinedDepthStencil));
                if (!m_fbo->isValid()) {
                    qCWarning(AURORAE) << "Creating FBO as render target failed";
                    m_fbo.reset();
                    return;
                }
            }
            m_view->setRenderTarget(m_fbo.data());
        });
        connect(m_view, &QQuickWindow::afterRendering, [this]{
            QMutexLocker locker(m_mutex.data());
            m_buffer = m_fbo->toImage();
        });
        connect(m_view, &QQuickWindow::afterRendering, this,
                static_cast<void (KDecoration::*)(void)>(&KDecoration::update), Qt::QueuedConnection);
    }
    if (m_item) {
        m_item->setParentItem(m_view->contentItem());
    }
    slotAlphaChanged();

    AuroraeFactory::instance()->theme()->setCompositingActive(compositingActive());
}

bool AuroraeClient::eventFilter(QObject *object, QEvent *event)
{
    // we need to filter the wheel events on the decoration
    // QML does not yet provide a way to accept wheel events, this will change with Qt 5
    // TODO: remove in KDE5
    // see BUG: 304248
    if (object != widget() || event->type() != QEvent::Wheel) {
        return KDecoration::eventFilter(object, event);
    }
    QWheelEvent *wheel = static_cast<QWheelEvent*>(event);
    if (mousePosition(wheel->pos()) == PositionCenter) {
        titlebarMouseWheelOperation(wheel->delta());
        return true;
    }
    return false;
}

void AuroraeClient::resize(const QSize &s)
{
    if (m_item) {
        m_item->setWidth(s.width());
        m_item->setHeight(s.height());
    }
    m_view->resize(s);
}

void AuroraeClient::borders(int &left, int &right, int &top, int &bottom) const
{
    if (!m_item) {
        left = right = top = bottom = 0;
        return;
    }
    QObject *borders = NULL;
    if (maximizeMode() == MaximizeFull) {
        borders = m_item->findChild<QObject*>(QStringLiteral("maximizedBorders"));
    } else {
        borders = m_item->findChild<QObject*>(QStringLiteral("borders"));
    }
    sizesFromBorders(borders, left, right, top, bottom);
}

void AuroraeClient::padding(int &left, int &right, int &top, int &bottom) const
{
    if (!m_item) {
        left = right = top = bottom = 0;
        return;
    }
    if (maximizeMode() == MaximizeFull) {
        left = right = top = bottom = 0;
        return;
    }
    sizesFromBorders(m_item->findChild<QObject*>(QStringLiteral("padding")), left, right, top, bottom);
}

void AuroraeClient::sizesFromBorders(const QObject *borders, int &left, int &right, int &top, int &bottom) const
{
    if (!borders) {
        return;
    }
    left = borders->property("left").toInt();
    right = borders->property("right").toInt();
    top = borders->property("top").toInt();
    bottom = borders->property("bottom").toInt();
}

QSize AuroraeClient::minimumSize() const
{
    return m_view->minimumSize();
}

KDecorationDefines::Position AuroraeClient::mousePosition(const QPoint &point) const
{
    // based on the code from deKorator
    int pos = PositionCenter;
    if (isShade() || isMaximized()) {
        return Position(pos);
    }

    int borderLeft, borderTop, borderRight, borderBottom;
    borders(borderLeft, borderRight, borderTop, borderBottom);
    int paddingLeft, paddingTop, paddingRight, paddingBottom;
    padding(paddingLeft, paddingRight, paddingTop, paddingBottom);
    int titleEdgeLeft, titleEdgeRight, titleEdgeTop, titleEdgeBottom;
    AuroraeFactory::instance()->theme()->titleEdges(titleEdgeLeft, titleEdgeTop, titleEdgeRight, titleEdgeBottom, false);
    switch (AuroraeFactory::instance()->theme()->decorationPosition()) {
    case DecorationTop:
        borderTop = titleEdgeTop;
        break;
    case DecorationLeft:
        borderLeft = titleEdgeLeft;
        break;
    case DecorationRight:
        borderRight = titleEdgeRight;
        break;
    case DecorationBottom:
        borderBottom = titleEdgeBottom;
        break;
    default:
        break; // nothing
    }
    if (point.x() >= (m_view->width() -  borderRight - paddingRight)) {
        pos |= PositionRight;
    } else if (point.x() <= borderLeft + paddingLeft) {
        pos |= PositionLeft;
    }

    if (point.y() >= m_view->height() - borderBottom - paddingBottom) {
        pos |= PositionBottom;
    } else if (point.y() <= borderTop + paddingTop ) {
        pos |= PositionTop;
    }

    return Position(pos);
}

void AuroraeClient::menuClicked()
{
    showWindowMenu(QCursor::pos());
}

void AuroraeClient::appMenuClicked()
{
    showApplicationMenu(QCursor::pos());
}

void AuroraeClient::toggleShade()
{
    setShade(!isShade());
}

void AuroraeClient::toggleKeepAbove()
{
    setKeepAbove(!keepAbove());
}

void AuroraeClient::toggleKeepBelow()
{
    setKeepBelow(!keepBelow());
}

void AuroraeClient::titlePressed(int button, int buttons)
{
    titlePressed(static_cast<Qt::MouseButton>(button), static_cast<Qt::MouseButtons>(buttons));
}

void AuroraeClient::titlePressed(Qt::MouseButton button, Qt::MouseButtons buttons)
{
    const QPoint cursor = QCursor::pos();
    QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonPress, m_view->mapFromGlobal(cursor),
                                         cursor, button, buttons, Qt::NoModifier);
    processMousePressEvent(event);
    delete event;
    event = 0;
}

void AuroraeClient::themeChanged()
{
    m_item.reset(AuroraeFactory::instance()->createQmlDecoration(this));
    if (!m_item) {
        return;
    }

    m_item->setParentItem(m_view->contentItem());
    connect(m_item.data(), SIGNAL(alphaChanged()), SLOT(slotAlphaChanged()));
    slotAlphaChanged();
}

int AuroraeClient::doubleClickInterval() const
{
    return QApplication::doubleClickInterval();
}

void AuroraeClient::closeWindow()
{
    QMetaObject::invokeMethod(qobject_cast< KDecoration* >(this), "doCloseWindow", Qt::QueuedConnection);
}

void AuroraeClient::doCloseWindow()
{
    KDecoration::closeWindow();
}

void AuroraeClient::maximize(int button)
{
    // a maximized window does not need to have a window decoration
    // in that case we need to delay handling by one cycle
    // BUG: 304870
    QMetaObject::invokeMethod(qobject_cast< KDecoration* >(this),
                              "doMaximzie",
                              Qt::QueuedConnection,
                              Q_ARG(int, button));
}

void AuroraeClient::doMaximzie(int button)
{
    KDecoration::maximize(static_cast<Qt::MouseButton>(button));
}

void AuroraeClient::titlebarDblClickOperation()
{
    // the double click operation can result in a window being maximized
    // see maximize
    QMetaObject::invokeMethod(qobject_cast< KDecoration* >(this), "doTitlebarDblClickOperation", Qt::QueuedConnection);
}

void AuroraeClient::doTitlebarDblClickOperation()
{
    KDecoration::titlebarDblClickOperation();
}

QVariant AuroraeClient::readConfig(const QString &key, const QVariant &defaultValue)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("auroraerc"));
    return config->group(AuroraeFactory::instance()->currentThemeName()).readEntry(key, defaultValue);
}

void AuroraeClient::slotAlphaChanged()
{
    if (!m_item) {
        setAlphaEnabled(false);
        return;
    }
    QVariant alphaProperty = m_item->property("alpha");
    if (alphaProperty.isValid() && alphaProperty.canConvert<bool>()) {
        setAlphaEnabled(alphaProperty.toBool());
    } else {
        // by default all Aurorae themes use the alpha channel
        setAlphaEnabled(true);
    }
}

QRegion AuroraeClient::region(KDecorationDefines::Region r)
{
    if (r != ExtendedBorderRegion) {
        return QRegion();
    }
    if (!m_item) {
        return QRegion();
    }
    if (isMaximized()) {
        // empty region for maximized windows
        return QRegion();
    }
    int left, right, top, bottom;
    left = right = top = bottom = 0;
    sizesFromBorders(m_item->findChild<QObject*>(QStringLiteral("extendedBorders")), left, right, top, bottom);
    if (top == 0 && right == 0 && bottom == 0 && left == 0) {
        // no extended borders
        return QRegion();
    }

    int paddingLeft, paddingRight, paddingTop, paddingBottom;
    paddingLeft = paddingRight = paddingTop = paddingBottom = 0;
    padding(paddingLeft, paddingRight, paddingTop, paddingBottom);
    QRect rect = this->rect().adjusted(paddingLeft, paddingTop, -paddingRight, -paddingBottom);
    rect.translate(-paddingLeft, -paddingTop);

    return QRegion(rect.adjusted(-left, -top, right, bottom)).subtract(rect);
}

bool AuroraeClient::animationsSupported() const
{
    return compositingActive();
}

void AuroraeClient::render(QPaintDevice *device, const QRegion &sourceRegion)
{
    QMutexLocker locker(m_mutex.data());
    QPainter painter(device);
    painter.setClipRegion(sourceRegion);
    painter.drawImage(QPoint(0, 0), m_buffer);
}

} // namespace Aurorae

extern "C"
{
    KDECORATIONS_EXPORT KDecorationFactory *create_factory() {
        return Aurorae::AuroraeFactory::instance();
    }
    KDECORATIONS_EXPORT int decoration_version() {
        return KWIN_DECORATION_API_VERSION;
    }
}


#include "aurorae.moc"
