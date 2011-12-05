/********************************************************************
Copyright (C) 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "auroraescene.h"
#include "auroraetheme.h"

#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>

#include <KConfig>
#include <KConfigGroup>
#include <Plasma/FrameSvg>

namespace Aurorae
{

AuroraeFactory::AuroraeFactory()
        : QObject()
        , KDecorationFactoryUnstable()
        , m_theme(new AuroraeTheme(this))
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
    m_theme->setShowTooltips(options()->showTooltips());
    m_theme->setTabDragMimeType(clientGroupItemDragMimeType());
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
    init();
    resetDecorations(changed);
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
        return true;
    case AbilityButtonMinimize:
        return m_theme->hasButton(MinimizeButton);
    case AbilityButtonMaximize:
        return m_theme->hasButton(MaximizeButton) || m_theme->hasButton(RestoreButton);
    case AbilityButtonClose:
        return m_theme->hasButton(CloseButton);
    case AbilityButtonAboveOthers:
        return m_theme->hasButton(KeepAboveButton);
    case AbilityButtonBelowOthers:
        return m_theme->hasButton(KeepBelowButton);
    case AbilityButtonShade:
        return m_theme->hasButton(ShadeButton);
    case AbilityButtonOnAllDesktops:
        return m_theme->hasButton(AllDesktopsButton);
    case AbilityButtonHelp:
        return m_theme->hasButton(HelpButton);
    case AbilityProvidesShadow:
        return true; // TODO: correct value from theme
    case AbilityClientGrouping:
        return true;
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


AuroraeFactory *AuroraeFactory::s_instance = NULL;

/*******************************************************
* Client
*******************************************************/
AuroraeClient::AuroraeClient(KDecorationBridge *bridge, KDecorationFactory *factory)
    : KDecorationUnstable(bridge, factory)
    , m_clickInProgress(false)
{
    m_scene = new AuroraeScene(AuroraeFactory::instance()->theme(),
                               options()->customButtonPositions() ? options()->titleButtonsLeft() : AuroraeFactory::instance()->theme()->defaultButtonsLeft(),
                               options()->customButtonPositions() ? options()->titleButtonsRight() : AuroraeFactory::instance()->theme()->defaultButtonsRight(),
                               providesContextHelp(), NULL);
    connect(m_scene, SIGNAL(closeWindow()), SLOT(closeWindow()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(maximize(Qt::MouseButtons)), SLOT(maximize(Qt::MouseButtons)), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(showContextHelp()), SLOT(showContextHelp()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(minimizeWindow()), SLOT(minimize()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(menuClicked()), SLOT(menuClicked()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(menuDblClicked()), SLOT(closeWindow()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(toggleOnAllDesktops()), SLOT(toggleOnAllDesktops()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(toggleShade()), SLOT(toggleShade()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(toggleKeepAbove()), SLOT(toggleKeepAbove()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(toggleKeepBelow()), SLOT(toggleKeepBelow()), Qt::QueuedConnection);
    connect(m_scene, SIGNAL(titlePressed(Qt::MouseButton,Qt::MouseButtons)),
            SLOT(titlePressed(Qt::MouseButton,Qt::MouseButtons)));
    connect(m_scene, SIGNAL(titleReleased(Qt::MouseButton,Qt::MouseButtons)),
            SLOT(titleReleased(Qt::MouseButton,Qt::MouseButtons)));
    connect(m_scene, SIGNAL(titleDoubleClicked()), SLOT(titlebarDblClickOperation()));
    connect(m_scene, SIGNAL(titleMouseMoved(Qt::MouseButton,Qt::MouseButtons)), 
            SLOT(titleMouseMoved(Qt::MouseButton,Qt::MouseButtons)));
    connect(m_scene, SIGNAL(wheelEvent(int)), SLOT(titlebarMouseWheelOperation(int)));
    connect(m_scene, SIGNAL(tabMouseButtonPress(QGraphicsSceneMouseEvent*,int)),
            SLOT(tabMouseButtonPress(QGraphicsSceneMouseEvent*,int)));
    connect(m_scene, SIGNAL(tabMouseButtonRelease(QGraphicsSceneMouseEvent*,int)),
            SLOT(tabMouseButtonRelease(QGraphicsSceneMouseEvent*,int)));
    connect(m_scene, SIGNAL(tabRemoved(int)), SLOT(tabRemoved(int)));
    connect(m_scene, SIGNAL(tabMoved(int,int)), SLOT(tabMoved(int,int)));
    connect(m_scene, SIGNAL(tabMovedToGroup(long int,int)), SLOT(tabMovedToGroup(long int,int)));
    connect(this, SIGNAL(keepAboveChanged(bool)), SLOT(keepAboveChanged(bool)));
    connect(this, SIGNAL(keepBelowChanged(bool)), SLOT(keepBelowChanged(bool)));
}

AuroraeClient::~AuroraeClient()
{
    m_view->setParent(NULL);
    m_view->deleteLater();
    m_scene->deleteLater();
}

void AuroraeClient::init()
{
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
    m_view->setFrameShape(QFrame::NoFrame);
    QPalette pal = m_view->palette();
    pal.setColor(m_view->backgroundRole(), Qt::transparent);
    m_view->setPalette(pal);
    QPalette pal2 = widget()->palette();
    pal2.setColor(widget()->backgroundRole(), Qt::transparent);
    widget()->setPalette(pal2);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // scene initialisation
    m_scene->setActive(isActive(), false);
    m_scene->setIcon(icon());
    m_scene->setAllDesktops(isOnAllDesktops());
    m_scene->setMaximizeMode(options()->moveResizeMaximizedWindows() ? MaximizeRestore : maximizeMode());
    m_scene->setShade(isShade());
    m_scene->setKeepAbove(keepAbove());
    m_scene->setKeepBelow(keepBelow());
    m_scene->setFont(KDecoration::options()->font(true), true);
    m_scene->setFont(KDecoration::options()->font(false), false);
    AuroraeFactory::instance()->theme()->setCompositingActive(compositingActive());
}

void AuroraeClient::activeChange()
{
    if (m_scene->isActive() != isActive()) {
        m_scene->setActive(isActive());
    }
}

void AuroraeClient::captionChange()
{
    checkTabs(true);
}

void AuroraeClient::iconChange()
{
    m_scene->setIcon(icon());
}

void AuroraeClient::desktopChange()
{
    m_scene->setAllDesktops(isOnAllDesktops());
}

void AuroraeClient::maximizeChange()
{
    if (!options()->moveResizeMaximizedWindows()) {
        m_scene->setMaximizeMode(maximizeMode());
    }
}

void AuroraeClient::resize(const QSize &s)
{
    m_scene->setSceneRect(QRectF(QPoint(0, 0), s));
    m_scene->updateLayout();
    m_view->resize(s);
    widget()->resize(s);
    updateWindowShape();
}

void AuroraeClient::shadeChange()
{
    m_scene->setShade(isShade());
}

void AuroraeClient::borders(int &left, int &right, int &top, int &bottom) const
{
    const bool maximized = maximizeMode() == MaximizeFull && !options()->moveResizeMaximizedWindows();
    AuroraeFactory::instance()->theme()->borders(left, top, right, bottom, maximized);
}

void AuroraeClient::padding(int &left, int &right, int &top, int &bottom) const
{
    AuroraeFactory::instance()->theme()->padding(left, top, right, bottom);
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
    if (changed & SettingCompositing) {
        updateWindowShape();
        AuroraeFactory::instance()->theme()->setCompositingActive(compositingActive());
    }
    if (changed & SettingButtons) {
        m_scene->setButtons(options()->customButtonPositions() ? options()->titleButtonsLeft() : AuroraeFactory::instance()->theme()->defaultButtonsLeft(),
                            options()->customButtonPositions() ? options()->titleButtonsRight() : AuroraeFactory::instance()->theme()->defaultButtonsRight());
    }
    if (changed & SettingFont) {
        m_scene->setFont(KDecoration::options()->font(true), true);
        m_scene->setFont(KDecoration::options()->font(false), false);
    }
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

void AuroraeClient::keepAboveChanged(bool above)
{
    if (above && m_scene->isKeepBelow()) {
        m_scene->setKeepBelow(false);
    }
    m_scene->setKeepAbove(above);
}

void AuroraeClient::keepBelowChanged(bool below)
{
    if (below && m_scene->isKeepAbove()) {
        m_scene->setKeepAbove(false);
    }
    m_scene->setKeepBelow(below);
}

void AuroraeClient::toggleKeepAbove()
{
    setKeepAbove(!keepAbove());
}

void AuroraeClient::toggleKeepBelow()
{
    setKeepBelow(!keepBelow());
}

void AuroraeClient::updateWindowShape()
{
    bool maximized = maximizeMode()==KDecorationDefines::MaximizeFull && !options()->moveResizeMaximizedWindows();
    int w=widget()->width();
    int h=widget()->height();

    if (maximized || compositingActive()) {
        QRegion mask(0,0,w,h);
        setMask(mask);
        return;
    }

    int pl, pt, pr, pb;
    padding(pl, pr, pt, pb);
    Plasma::FrameSvg *deco = AuroraeFactory::instance()->theme()->decoration();
    if (!deco->hasElementPrefix("decoration-opaque")) {
        // opaque element is missing: set generic mask
        w = w - pl - pr;
        h = h - pt - pb;
        QRegion mask(pl, pt, w, h);
        setMask(mask);
        return;
    }
    deco->setElementPrefix("decoration-opaque");
    deco->resizeFrame(QSize(w-pl-pr, h-pt-pb));
    QRegion mask = deco->mask().translated(pl, pt);
    setMask(mask);
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

void AuroraeClient::checkTabs(bool force)
{
    if (m_scene->tabCount() == 1 && clientGroupItems().count() == 1 && !force) {
        return;
    }
    while (m_scene->tabCount() < clientGroupItems().count()) {
        m_scene->addTab(QString());
    }
    while (m_scene->tabCount() > clientGroupItems().count()) {
        m_scene->removeLastTab();
    }
    QList<AuroraeTabData> data;
    foreach (const ClientGroupItem &item, clientGroupItems()) {
        data << AuroraeTabData(item.title(), item.icon());
    }
    m_scene->setAllTabData(data);
    m_scene->setFocusedTab(visibleClientGroupItem());
}

bool AuroraeClient::eventFilter(QObject *o, QEvent *e)
{
    if (o != widget()) {
        return false;
    }
    if (e->type() == QEvent::Paint) {
        checkTabs();
    }
    return false;
}

void AuroraeClient::tabMouseButtonPress(QGraphicsSceneMouseEvent *e, int index)
{
    if (buttonToWindowOperation(e->buttons()) == OperationsOp) {
        displayClientMenu(index, e->screenPos());
        return;
    } else if (buttonToWindowOperation(e->buttons()) == ClientGroupDragOp) {
        m_scene->setUniqueTabDragId(index, itemId(index));
    }
    titlePressed(e->button(), e->buttons());
    m_clickInProgress = true;
}

void AuroraeClient::tabMouseButtonRelease(QGraphicsSceneMouseEvent *e, int index)
{
    if (m_clickInProgress) {
        setVisibleClientGroupItem(index);
    }
    titleReleased(e->button(), e->buttons());
    m_clickInProgress = false;
}

void AuroraeClient::tabRemoved(int index)
{
    removeFromClientGroup(index);
}

void AuroraeClient::tabMoved(int index, int before)
{
    moveItemInClientGroup(index, before);
}

void AuroraeClient::tabMovedToGroup(long int uid, int before)
{
    moveItemToClientGroup(uid, before);
}

} // namespace Aurorae

extern "C"
{
    KDE_EXPORT KDecorationFactory *create_factory() {
        return Aurorae::AuroraeFactory::instance();
    }
}


#include "aurorae.moc"
