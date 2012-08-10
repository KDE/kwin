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

#include <QApplication>
#include <QtDeclarative/QDeclarativeComponent>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeItem>
#include <QtGui/QGraphicsView>

#include <KConfig>
#include <KConfigGroup>
#include <KStandardDirs>
#include <Plasma/FrameSvg>

namespace Aurorae
{

AuroraeFactory::AuroraeFactory()
        : QObject()
        , KDecorationFactoryUnstable()
        , m_theme(new AuroraeTheme(this))
        , m_engine(new QDeclarativeEngine(this))
        , m_component(new QDeclarativeComponent(m_engine, this))
{
    init();
}

void AuroraeFactory::init()
{
    qRegisterMetaType<uint>("Qt::MouseButtons");

    KConfig conf("auroraerc");
    KConfigGroup group(&conf, "Engine");

    const QString themeName = group.readEntry("ThemeName", "example-deco");
    KConfig config("aurorae/themes/" + themeName + '/' + themeName + "rc", KConfig::FullConfig, "data");
    KConfigGroup themeGroup(&conf, themeName);
    m_theme->loadTheme(themeName, config);
    m_theme->setBorderSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("BorderSize", KDecorationDefines::BorderNormal));
    m_theme->setButtonSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("ButtonSize", KDecorationDefines::BorderNormal));
    m_theme->setTabDragMimeType(tabDragMimeType());
    // setup the QML engine
    foreach (const QString &importPath, KGlobal::dirs()->findDirs("module", "imports")) {
        m_engine->addImportPath(importPath);
    }
    m_component->loadUrl(QUrl(KStandardDirs::locate("data", "kwin/aurorae/aurorae.qml")));
    m_engine->rootContext()->setContextProperty("auroraeTheme", m_theme);
    m_engine->rootContext()->setContextProperty("options", this);
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

bool AuroraeFactory::reset(unsigned long changed)
{
    if (changed & SettingButtons) {
        emit buttonsChanged();
    }
    const KConfig conf("auroraerc");
    const KConfigGroup group(&conf, "Engine");
    const QString themeName = group.readEntry("ThemeName", "example-deco");
    const KConfig config("aurorae/themes/" + themeName + '/' + themeName + "rc", KConfig::FullConfig, "data");
    const KConfigGroup themeGroup(&conf, themeName);
    if (themeName != m_theme->themeName()) {
        m_theme->loadTheme(themeName, config);
        resetDecorations(changed);
    }
    m_theme->setBorderSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("BorderSize", KDecorationDefines::BorderNormal));
    m_theme->setButtonSize((KDecorationDefines::BorderSize)themeGroup.readEntry<int>("ButtonSize", KDecorationDefines::BorderNormal));
    return false; // need hard reset
}

bool AuroraeFactory::supports(Ability ability) const
{
    switch (ability) {
    case AbilityAnnounceButtons:
    case AbilityUsesAlphaChannel:
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

QDeclarativeItem *AuroraeFactory::createQmlDecoration(Aurorae::AuroraeClient *client)
{
    QDeclarativeContext *context = new QDeclarativeContext(m_engine->rootContext(), this);
    context->setContextProperty("decoration", client);
    return qobject_cast< QDeclarativeItem* >(m_component->create(context));
}

QString AuroraeFactory::rightButtons()
{
    return options()->titleButtonsRight();
}

QString AuroraeFactory::leftButtons()
{
    return options()->titleButtonsLeft();
}

bool AuroraeFactory::customButtonPositions()
{
    return options()->customButtonPositions();
}

AuroraeFactory *AuroraeFactory::s_instance = NULL;

/*******************************************************
* Client
*******************************************************/
AuroraeClient::AuroraeClient(KDecorationBridge *bridge, KDecorationFactory *factory)
    : KDecorationUnstable(bridge, factory)
    , m_view(NULL)
    , m_scene(new QGraphicsScene(this))
    , m_item(AuroraeFactory::instance()->createQmlDecoration(this))
{
    connect(this, SIGNAL(keepAboveChanged(bool)), SIGNAL(keepAboveChangedWrapper()));
    connect(this, SIGNAL(keepBelowChanged(bool)), SIGNAL(keepBelowChangedWrapper()));
}

AuroraeClient::~AuroraeClient()
{
    m_view->setParent(NULL);
    m_view->deleteLater();
}

void AuroraeClient::init()
{
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    // HACK: we need to add the GraphicsView as a child widget to a normal widget
    // the GraphicsView eats the mouse release event and by that kwin core starts to move
    // the decoration each time the decoration is clicked
    // therefore we use two widgets and inject an own mouse release event to the parent widget
    // when the graphics view eats a mouse event
    createMainWidget();
    widget()->setAttribute(Qt::WA_TranslucentBackground);
    widget()->setAttribute(Qt::WA_NoSystemBackground);
    widget()->installEventFilter(this);
    m_view = new QGraphicsView(m_scene, widget());
    m_view->setAttribute(Qt::WA_TranslucentBackground);
    m_view->setWindowFlags(Qt::X11BypassWindowManagerHint);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setOptimizationFlags(QGraphicsView::DontSavePainterState);
    m_view->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    QPalette pal = m_view->palette();
    pal.setColor(m_view->backgroundRole(), Qt::transparent);
    m_view->setPalette(pal);
    QPalette pal2 = widget()->palette();
    pal2.setColor(widget()->backgroundRole(), Qt::transparent);
    widget()->setPalette(pal2);
    m_scene->addItem(m_item);

    AuroraeFactory::instance()->theme()->setCompositingActive(compositingActive());
    connect(AuroraeFactory::instance()->theme(), SIGNAL(themeChanged()), SLOT(themeChanged()));
}

bool AuroraeClient::eventFilter(QObject *object, QEvent *event)
{
    // we need to filter the wheel events on the decoration
    // QML does not yet provide a way to accept wheel events, this will change with Qt 5
    // TODO: remove in KDE5
    // see BUG: 304248
    if (object != widget() || event->type() != QEvent::Wheel) {
        return KDecorationUnstable::eventFilter(object, event);
    }
    QWheelEvent *wheel = static_cast<QWheelEvent*>(event);
    if (mousePosition(wheel->pos()) == PositionCenter) {
        titlebarMouseWheelOperation(wheel->delta());
        return true;
    }
    return false;
}

void AuroraeClient::activeChange()
{
    emit activeChanged();
}

void AuroraeClient::captionChange()
{
    emit captionChanged();
}

void AuroraeClient::iconChange()
{
    emit iconChanged();
}

void AuroraeClient::desktopChange()
{
    emit desktopChanged();
}

void AuroraeClient::maximizeChange()
{
    if (!options()->moveResizeMaximizedWindows()) {
        emit maximizeChanged();
    }
}

void AuroraeClient::resize(const QSize &s)
{
    if (m_item) {
        m_item->setWidth(s.width());
        m_item->setHeight(s.height());
    }
    m_scene->setSceneRect(QRectF(QPoint(0, 0), s));
    m_view->resize(s);
    widget()->resize(s);
}

void AuroraeClient::shadeChange()
{
    emit shadeChanged();
}

void AuroraeClient::borders(int &left, int &right, int &top, int &bottom) const
{
    if (!m_item) {
        left = right = top = bottom = 0;
        return;
    }
    const bool maximized = maximizeMode() == MaximizeFull && !options()->moveResizeMaximizedWindows();
    if (maximized) {
        left = m_item->property("borderLeftMaximized").toInt();
        right = m_item->property("borderRightMaximized").toInt();
        top = m_item->property("borderTopMaximized").toInt();
        bottom = m_item->property("borderBottomMaximized").toInt();
    } else {
        left = m_item->property("borderLeft").toInt();
        right = m_item->property("borderRight").toInt();
        top = m_item->property("borderTop").toInt();
        bottom = m_item->property("borderBottom").toInt();
    }
}

void AuroraeClient::padding(int &left, int &right, int &top, int &bottom) const
{
    if (!m_item) {
        left = right = top = bottom = 0;
        return;
    }
    left = m_item->property("paddingLeft").toInt();
    right = m_item->property("paddingRight").toInt();
    top = m_item->property("paddingTop").toInt();
    bottom = m_item->property("paddingBottom").toInt();
}

QSize AuroraeClient::minimumSize() const
{
    return widget()->minimumSize();
}

KDecorationDefines::Position AuroraeClient::mousePosition(const QPoint &point) const
{
    // based on the code from deKorator
    int pos = PositionCenter;
    if (isShade()) {
        return Position(pos);
    }

    int borderLeft, borderTop, borderRight, borderBottom;
    borders(borderLeft, borderRight, borderTop, borderBottom);
    int paddingLeft, paddingTop, paddingRight, paddingBottom;
    padding(paddingLeft, paddingRight, paddingTop, paddingBottom);
    const bool maximized = maximizeMode() == MaximizeFull && !options()->moveResizeMaximizedWindows();
    int titleEdgeLeft, titleEdgeRight, titleEdgeTop, titleEdgeBottom;
    AuroraeFactory::instance()->theme()->titleEdges(titleEdgeLeft, titleEdgeTop, titleEdgeRight, titleEdgeBottom, maximized);
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

void AuroraeClient::reset(long unsigned int changed)
{
    KDecoration::reset(changed);
}

void AuroraeClient::menuClicked()
{
    showWindowMenu(QCursor::pos());
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

bool AuroraeClient::isMaximized() const
{
    return maximizeMode()==KDecorationDefines::MaximizeFull && !options()->moveResizeMaximizedWindows();
}

void AuroraeClient::titlePressed(int button, int buttons)
{
    titlePressed(static_cast<Qt::MouseButton>(button), static_cast<Qt::MouseButtons>(buttons));
}

void AuroraeClient::titleReleased(int button, int buttons)
{
    titleReleased(static_cast<Qt::MouseButton>(button), static_cast<Qt::MouseButtons>(buttons));
}

void AuroraeClient::titleMouseMoved(int button, int buttons)
{
    titleMouseMoved(static_cast<Qt::MouseButton>(button), static_cast<Qt::MouseButtons>(buttons));
}

void AuroraeClient::titlePressed(Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonPress, widget()->mapFromGlobal(QCursor::pos()),
                                         QCursor::pos(), button, buttons, Qt::NoModifier);
    processMousePressEvent(event);
    delete event;
    event = 0;
}

void AuroraeClient::titleReleased(Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonRelease, widget()->mapFromGlobal(QCursor::pos()),
                                         QCursor::pos(), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(widget(), event);
    delete event;
    event = 0;
}

void AuroraeClient::titleMouseMoved(Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent *event = new QMouseEvent(QEvent::MouseMove, widget()->mapFromGlobal(QCursor::pos()),
                                         QCursor::pos(), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(widget(), event);
    delete event;
    event = 0;
}

void AuroraeClient::themeChanged()
{
    m_scene->clear();
    m_item = AuroraeFactory::instance()->createQmlDecoration(this);
    if (m_item) {
        m_item->setWidth(m_scene->sceneRect().width());
        m_item->setHeight(m_scene->sceneRect().height());
    }
    m_scene->addItem(m_item);
}

int AuroraeClient::doubleClickInterval() const
{
    return QApplication::doubleClickInterval();
}

void AuroraeClient::closeWindow()
{
    QMetaObject::invokeMethod(qobject_cast< KDecorationUnstable* >(this), "doCloseWindow", Qt::QueuedConnection);
}

void AuroraeClient::doCloseWindow()
{
    KDecorationUnstable::closeWindow();
}

void AuroraeClient::maximize(int button)
{
    // a maximized window does not need to have a window decoration
    // in that case we need to delay handling by one cycle
    // BUG: 304870
    QMetaObject::invokeMethod(qobject_cast< KDecorationUnstable* >(this),
                              "doMaximzie",
                              Qt::QueuedConnection,
                              Q_ARG(int, button));
}

void AuroraeClient::doMaximzie(int button)
{
    KDecorationUnstable::maximize(static_cast<Qt::MouseButton>(button));
}

void AuroraeClient::titlebarDblClickOperation()
{
    // the double click operation can result in a window being maximized
    // see maximize
    QMetaObject::invokeMethod(qobject_cast< KDecorationUnstable* >(this), "doTitlebarDblClickOperation", Qt::QueuedConnection);
}

void AuroraeClient::doTitlebarDblClickOperation()
{
    KDecorationUnstable::titlebarDblClickOperation();
}

} // namespace Aurorae

extern "C"
{
    KDE_EXPORT KDecorationFactory *create_factory() {
        return Aurorae::AuroraeFactory::instance();
    }
    KWIN_EXPORT int decoration_version() {
        return KWIN_DECORATION_API_VERSION;
    }
}


#include "aurorae.moc"
