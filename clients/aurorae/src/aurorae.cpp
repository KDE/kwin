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
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeView>

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
    // HACK: we need to add the GraphicsView as a child widget to a normal widget
    // the GraphicsView eats the mouse release event and by that kwin core starts to move
    // the decoration each time the decoration is clicked
    // therefore we use two widgets and inject an own mouse release event to the parent widget
    // when the graphics view eats a mouse event
    createMainWidget();
    widget()->setAttribute(Qt::WA_TranslucentBackground);
    widget()->setAttribute(Qt::WA_NoSystemBackground);
    m_view = new QDeclarativeView(widget());
    m_view->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    m_view->setAttribute(Qt::WA_TranslucentBackground);
    m_view->setWindowFlags(Qt::X11BypassWindowManagerHint);
    QPalette pal = m_view->palette();
    pal.setColor(m_view->backgroundRole(), Qt::transparent);
    m_view->setPalette(pal);
    QPalette pal2 = widget()->palette();
    pal2.setColor(widget()->backgroundRole(), Qt::transparent);
    widget()->setPalette(pal2);

    // setup the QML engine
    foreach (const QString &importPath, KGlobal::dirs()->findDirs("module", "imports")) {
        m_view->engine()->addImportPath(importPath);
    }
    m_view->rootContext()->setContextProperty("decoration", this);
    m_view->rootContext()->setContextProperty("auroraeTheme", AuroraeFactory::instance()->theme());
    m_view->setSource(QUrl(KStandardDirs::locate("data", "kwin/aurorae/aurorae.qml")));
    AuroraeFactory::instance()->theme()->setCompositingActive(compositingActive());
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
    m_view->resize(s);
    widget()->resize(s);
}

void AuroraeClient::shadeChange()
{
    emit shadeChanged();
}

void AuroraeClient::borders(int &left, int &right, int &top, int &bottom) const
{
    if (m_view->status() == QDeclarativeView::Error) {
        left = right = top = bottom = 0;
        return;
    }
    const bool maximized = maximizeMode() == MaximizeFull && !options()->moveResizeMaximizedWindows();
    if (maximized) {
        left = m_view->rootObject()->property("borderLeftMaximized").toInt();
        right = m_view->rootObject()->property("borderRightMaximized").toInt();
        top = m_view->rootObject()->property("borderTopMaximized").toInt();
        bottom = m_view->rootObject()->property("borderBottomMaximized").toInt();
    } else {
        left = m_view->rootObject()->property("borderLeft").toInt();
        right = m_view->rootObject()->property("borderRight").toInt();
        top = m_view->rootObject()->property("borderTop").toInt();
        bottom = m_view->rootObject()->property("borderBottom").toInt();
    }
}

void AuroraeClient::padding(int &left, int &right, int &top, int &bottom) const
{
    if (m_view->status() == QDeclarativeView::Error) {
        left = right = top = bottom = 0;
        return;
    }
    left = m_view->rootObject()->property("paddingLeft").toInt();
    right = m_view->rootObject()->property("paddingRight").toInt();
    top = m_view->rootObject()->property("paddingTop").toInt();
    bottom = m_view->rootObject()->property("paddingBottom").toInt();
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
        AuroraeFactory::instance()->theme()->setCompositingActive(compositingActive());
    }
    if (changed & SettingButtons) {
        // TODO: update buttons
    }
    if (changed & SettingFont) {
        // TODO: set font
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

QString AuroraeClient::rightButtons() const
{
    // TODO: make independent of Aurorae
    return options()->customButtonPositions() ? options()->titleButtonsRight() : AuroraeFactory::instance()->theme()->defaultButtonsRight();
}

QString AuroraeClient::leftButtons() const
{
    // TODO: make independent of Aurorae
    return options()->customButtonPositions() ? options()->titleButtonsLeft() : AuroraeFactory::instance()->theme()->defaultButtonsLeft();
}

} // namespace Aurorae

extern "C"
{
    KDE_EXPORT KDecorationFactory *create_factory() {
        return Aurorae::AuroraeFactory::instance();
    }
}


#include "aurorae.moc"
