/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "presentwindows.h"

#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>
#include <kcolorscheme.h>
#include <kconfiggroup.h>
#include <kdebug.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <kwinglutils.h>
#endif

#include <QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QGraphicsLinearLayout>
#include <Plasma/PushButton>
#include <Plasma/WindowEffects>

#include <math.h>
#include <assert.h>
#include <limits.h>
#include <QTimer>
#include <QVector4D>

namespace KWin
{

KWIN_EFFECT(presentwindows, PresentWindowsEffect)

PresentWindowsEffect::PresentWindowsEffect()
    : m_proxy(this)
    , m_activated(false)
    , m_ignoreMinimized(false)
    , m_decalOpacity(0.0)
    , m_hasKeyboardGrab(false)
    , m_tabBoxEnabled(false)
    , m_mode(ModeCurrentDesktop)
    , m_managerWindow(NULL)
    , m_highlightedWindow(NULL)
    , m_filterFrame(effects->effectFrame(EffectFrameStyled, false))
    , m_closeView(NULL)
{
    m_atomDesktop = XInternAtom(display(), "_KDE_PRESENT_WINDOWS_DESKTOP", False);
    m_atomWindows = XInternAtom(display(), "_KDE_PRESENT_WINDOWS_GROUP", False);
    effects->registerPropertyType(m_atomDesktop, true);
    effects->registerPropertyType(m_atomWindows, true);

    // Announce support by creating a dummy version on the root window
    unsigned char dummy = 0;
    XChangeProperty(display(), rootWindow(), m_atomDesktop, m_atomDesktop, 8, PropModeReplace, &dummy, 1);
    XChangeProperty(display(), rootWindow(), m_atomWindows, m_atomWindows, 8, PropModeReplace, &dummy, 1);

    QFont font;
    font.setPointSize(font.pointSize() * 2);
    font.setBold(true);
    m_filterFrame->setFont(font);

    KActionCollection* actionCollection = new KActionCollection(this);
    KAction* a = (KAction*)actionCollection->addAction("Expose");
    a->setText(i18n("Toggle Present Windows (Current desktop)"));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F9));
    shortcut = a->globalShortcut();
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggleActive()));
    connect(a, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(globalShortcutChanged(QKeySequence)));
    KAction* b = (KAction*)actionCollection->addAction("ExposeAll");
    b->setText(i18n("Toggle Present Windows (All desktops)"));
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));
    shortcutAll = b->globalShortcut();
    connect(b, SIGNAL(triggered(bool)), this, SLOT(toggleActiveAllDesktops()));
    connect(b, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(globalShortcutChangedAll(QKeySequence)));
    KAction* c = (KAction*)actionCollection->addAction("ExposeClass");
    c->setText(i18n("Toggle Present Windows (Window class)"));
    c->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F7));
    connect(c, SIGNAL(triggered(bool)), this, SLOT(toggleActiveClass()));
    connect(c, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(globalShortcutChangedClass(QKeySequence)));
    shortcutClass = c->globalShortcut();
    reconfigure(ReconfigureAll);
}

PresentWindowsEffect::~PresentWindowsEffect()
{
    XDeleteProperty(display(), rootWindow(), m_atomDesktop);
    effects->registerPropertyType(m_atomDesktop, false);
    XDeleteProperty(display(), rootWindow(), m_atomWindows);
    effects->registerPropertyType(m_atomWindows, false);
    foreach (ElectricBorder border, m_borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, m_borderActivateAll) {
        effects->unreserveElectricBorder(border);
    }
    delete m_filterFrame;
    delete m_closeView;
}

void PresentWindowsEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("PresentWindows");
    foreach (ElectricBorder border, m_borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, m_borderActivateAll) {
        effects->unreserveElectricBorder(border);
    }
    m_borderActivate.clear();
    m_borderActivateAll.clear();
    QList<int> borderList = QList<int>();
    borderList.append(int(ElectricNone));
    borderList = conf.readEntry("BorderActivate", borderList);
    foreach (int i, borderList) {
        m_borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }
    borderList.clear();
    borderList.append(int(ElectricTopLeft));
    borderList = conf.readEntry("BorderActivateAll", borderList);
    foreach (int i, borderList) {
        m_borderActivateAll.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }
    m_layoutMode = conf.readEntry("LayoutMode", int(LayoutNatural));
    m_showCaptions = conf.readEntry("DrawWindowCaptions", true);
    m_showIcons = conf.readEntry("DrawWindowIcons", true);
    m_doNotCloseWindows = !conf.readEntry("AllowClosingWindows", true);
    m_tabBoxAllowed = conf.readEntry("TabBox", false);
    m_tabBoxAlternativeAllowed = conf.readEntry("TabBoxAlternative", false);
    m_ignoreMinimized = conf.readEntry("IgnoreMinimized", false);
    m_accuracy = conf.readEntry("Accuracy", 1) * 20;
    m_fillGaps = conf.readEntry("FillGaps", true);
    m_fadeDuration = double(animationTime(150));
    m_showPanel = conf.readEntry("ShowPanel", false);
    m_leftButtonWindow = (WindowMouseAction)conf.readEntry("LeftButtonWindow", (int)WindowActivateAction);
    m_middleButtonWindow = (WindowMouseAction)conf.readEntry("MiddleButtonWindow", (int)WindowNoAction);
    m_rightButtonWindow = (WindowMouseAction)conf.readEntry("RightButtonWindow", (int)WindowExitAction);
    m_leftButtonDesktop = (DesktopMouseAction)conf.readEntry("LeftButtonDesktop", (int)DesktopExitAction);
    m_middleButtonDesktop = (DesktopMouseAction)conf.readEntry("MiddleButtonDesktop", (int)DesktopNoAction);
    m_rightButtonDesktop = (DesktopMouseAction)conf.readEntry("RightButtonDesktop", (int)DesktopNoAction);
}

void* PresentWindowsEffect::proxy()
{
    return &m_proxy;
}

void PresentWindowsEffect::toggleActiveClass()
{
    if (!m_activated) {
        if (!effects->activeWindow())
            return;
        m_mode = ModeWindowClass;
        m_class = effects->activeWindow()->windowClass();
    }
    setActive(!m_activated);
}

//-----------------------------------------------------------------------------
// Screen painting

void PresentWindowsEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    m_motionManager.calculate(time);

    // We need to mark the screen as having been transformed otherwise there will be no repainting
    if (m_activated || m_motionManager.managingWindows())
        data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    if (m_activated)
        m_decalOpacity = qMin(1.0, m_decalOpacity + time / m_fadeDuration);
    else
        m_decalOpacity = qMax(0.0, m_decalOpacity - time / m_fadeDuration);

    effects->prePaintScreen(data, time);
}

void PresentWindowsEffect::paintScreen(int mask, QRegion region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    // Display the filter box
    if (!m_windowFilter.isEmpty())
        m_filterFrame->render(region);
}

void PresentWindowsEffect::postPaintScreen()
{
    if (m_motionManager.areWindowsMoving())
        effects->addRepaintFull();
    else if (!m_activated && m_motionManager.managingWindows()) {
        // We have finished moving them back, stop processing
        m_motionManager.unmanageAll();

        DataHash::iterator i = m_windowData.begin();
        while (i != m_windowData.end()) {
            delete i.value().textFrame;
            delete i.value().iconFrame;
            i++;
        }
        m_windowData.clear();

        foreach (EffectWindow * w, effects->stackingOrder()) {
            if (w->isDock()) {
                w->setData(WindowForceBlurRole, QVariant(false));
            }
        }
        effects->setActiveFullScreenEffect(NULL);
    }

    // Update windows that are changing brightness or opacity
    DataHash::const_iterator i;
    for (i = m_windowData.constBegin(); i != m_windowData.constEnd(); ++i) {
        if (i.value().opacity > 0.0 && i.value().opacity < 1.0)
            i.key()->addRepaintFull();
        if (i.value().highlight > 0.0 && i.value().highlight < 1.0)
            i.key()->addRepaintFull();
    }

    effects->postPaintScreen();
}

//-----------------------------------------------------------------------------
// Window painting

void PresentWindowsEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    // TODO: We should also check to see if any windows are fading just in case fading takes longer
    //       than moving the windows when the effect is deactivated.
    if ((m_activated || m_motionManager.areWindowsMoving()) && m_windowData.contains(w)) {
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);   // Display always
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        if (m_windowData[w].visible)
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_CLIENT_GROUP);

        // Calculate window's opacity
        // TODO: Minimized windows or windows not on the current desktop are only 75% visible?
        if (m_windowData[w].visible) {
            if (m_windowData[w].deleted)
                m_windowData[w].opacity = qMax(0.0, m_windowData[w].opacity - time / m_fadeDuration);
            else
                m_windowData[w].opacity = qMin(/*( w->isMinimized() || !w->isOnCurrentDesktop() ) ? 0.75 :*/ 1.0,
                                          m_windowData[w].opacity + time / m_fadeDuration);
        } else
            m_windowData[w].opacity = qMax(0.0, m_windowData[w].opacity - time / m_fadeDuration);
        if (m_windowData[w].opacity == 0.0) {
            // don't disable painting for panels if show panel is set
            if (!w->isDock() || (w->isDock() && !m_showPanel))
                w->disablePainting(EffectWindow::PAINT_DISABLED);
        } else if (m_windowData[w].opacity != 1.0)
            data.setTranslucent();

        // Calculate window's brightness
        if (w == m_highlightedWindow || w == m_closeWindow || !m_activated)
            m_windowData[w].highlight = qMin(1.0, m_windowData[w].highlight + time / m_fadeDuration);
        else
            m_windowData[w].highlight = qMax(0.0, m_windowData[w].highlight - time / m_fadeDuration);

        // Closed windows
        if (m_windowData[w].deleted) {
            data.setTranslucent();
            if (m_windowData[w].opacity <= 0.0 && m_windowData[w].referenced) {
                // it's possible that another effect has referenced the window
                // we have to keep the window in the list to prevent flickering
                m_windowData[w].referenced = false;
                w->unrefWindow();
            } else
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
        }

        // desktop windows on other desktops (Plasma activity per desktop) should not be painted
        if (w->isDesktop() && !w->isOnCurrentDesktop())
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);

        if (m_motionManager.isManaging(w))
            data.setTransformed(); // We will be moving this window
    }
    effects->prePaintWindow(w, data, time);
}

void PresentWindowsEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if ((m_activated || m_motionManager.areWindowsMoving()) && m_windowData.contains(w)) {
        if (w->isDock() && m_showPanel) {
            // in case the panel should be shown just display it without any changes
            effects->paintWindow(w, mask, region, data);
            return;
        }

        // Apply opacity and brightness
        data.opacity *= m_windowData[w].opacity;
        data.brightness *= interpolate(0.7, 1.0, m_windowData[w].highlight);

        if (m_motionManager.isManaging(w)) {
            m_motionManager.apply(w, data);

            if (!m_motionManager.areWindowsMoving()) {
                mask |= PAINT_WINDOW_LANCZOS;
            }
            effects->paintWindow(w, mask, region, data);

            QRect rect = m_motionManager.transformedGeometry(w).toRect();
            if (m_showIcons) {
                QPoint point(rect.x() + rect.width() * 0.95,
                             rect.y() + rect.height() * 0.95);
                m_windowData[w].iconFrame->setPosition(point);
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
                if (effects->compositingType() == KWin::OpenGLCompositing && data.shader) {
                    const float a = 0.9 * data.opacity * m_decalOpacity * 0.75;
                    data.shader->setUniform(GLShader::TextureWidth, 1.0f);
                    data.shader->setUniform(GLShader::TextureHeight, 1.0f);
                    data.shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
                }
#endif
                m_windowData[w].iconFrame->render(region, 0.9 * data.opacity * m_decalOpacity, 0.75);
            }
            if (m_showCaptions) {
                QPoint point(rect.x() + rect.width() / 2,
                             rect.y() + rect.height() / 2);
                m_windowData[w].textFrame->setPosition(point);
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
                if (effects->compositingType() == KWin::OpenGLCompositing && data.shader) {
                    const float a = 0.9 * data.opacity * m_decalOpacity * 0.75;
                    data.shader->setUniform(GLShader::TextureWidth, 1.0f);
                    data.shader->setUniform(GLShader::TextureHeight, 1.0f);
                    data.shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
                }
#endif
                m_windowData[w].textFrame->render(region, 0.9 * data.opacity * m_decalOpacity, 0.75);
            }
        } else
            effects->paintWindow(w, mask, region, data);
    } else
        effects->paintWindow(w, mask, region, data);
}

//-----------------------------------------------------------------------------
// User interaction

void PresentWindowsEffect::windowAdded(EffectWindow *w)
{
    if (!m_activated)
        return;
    m_windowData[w].visible = isVisibleWindow(w);
    m_windowData[w].opacity = 0.0;
    m_windowData[w].highlight = 0.0;
    m_windowData[w].textFrame = effects->effectFrame(EffectFrameUnstyled, false);
    QFont font;
    font.setBold(true);
    font.setPointSize(12);
    m_windowData[w].textFrame->setFont(font);
    m_windowData[w].iconFrame = effects->effectFrame(EffectFrameUnstyled, false);
    m_windowData[w].iconFrame->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_windowData[w].iconFrame->setIcon(w->icon());
    if (isSelectableWindow(w)) {
        m_motionManager.manage(w);
        rearrangeWindows();
    }
    if (w == effects->findWindow(m_closeView->winId())) {
        m_windowData[w].visible = true;
        m_windowData[w].highlight = 1.0;
        m_closeWindow = w;
        w->setData(WindowForceBlurRole, QVariant(true));
    }
}

void PresentWindowsEffect::windowClosed(EffectWindow *w)
{
    if (m_managerWindow == w)
        m_managerWindow = NULL;
    if (!m_windowData.contains(w))
        return;
    m_windowData[w].deleted = true;
    m_windowData[w].referenced = true;
    w->refWindow();
    if (m_highlightedWindow == w)
        setHighlightedWindow(findFirstWindow());
    if (m_closeWindow == w) {
        m_closeWindow = 0;
        return; // don't rearrange
    }
    rearrangeWindows();
}

void PresentWindowsEffect::windowDeleted(EffectWindow *w)
{
    if (!m_windowData.contains(w))
        return;
    delete m_windowData[w].textFrame;
    delete m_windowData[w].iconFrame;
    m_windowData.remove(w);
    m_motionManager.unmanage(w);
}

void PresentWindowsEffect::windowGeometryShapeChanged(EffectWindow* w, const QRect& old)
{
    Q_UNUSED(old)
    if (!m_activated)
        return;
    if (!m_windowData.contains(w))
        return;
    rearrangeWindows();
}

bool PresentWindowsEffect::borderActivated(ElectricBorder border)
{
    if (!m_borderActivate.contains(border) && !m_borderActivateAll.contains(border))
        return false;
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return true;
    if (m_borderActivate.contains(border))
        toggleActive();
    else
        toggleActiveAllDesktops();
    return true;
}

void PresentWindowsEffect::windowInputMouseEvent(Window w, QEvent *e)
{
    assert(w == m_input);
    Q_UNUSED(w);

    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    if (m_closeView->geometry().contains(me->pos())) {
        if (!m_closeView->isVisible()) {
            updateCloseWindow();
        }
        if (m_closeView->isVisible()) {
            const QPoint widgetPos = m_closeView->mapFromGlobal(me->pos());
            const QPointF scenePos = m_closeView->mapToScene(widgetPos);
            QMouseEvent event(me->type(), widgetPos, me->pos(), me->button(), me->buttons(), me->modifiers());
            m_closeView->windowInputMouseEvent(&event);
            return;
        }
    }
    // Which window are we hovering over? Always trigger as we don't always get move events before clicking
    // We cannot use m_motionManager.windowAtPoint() as the window might not be visible
    EffectWindowList windows = m_motionManager.managedWindows();
    bool hovering = false;
    for (int i = 0; i < windows.size(); i++) {
        assert(m_windowData.contains(windows.at(i)));
        if (m_motionManager.transformedGeometry(windows.at(i)).contains(cursorPos()) &&
                m_windowData[windows.at(i)].visible && !m_windowData[windows.at(i)].deleted) {
            hovering = true;
            if (windows.at(i) && m_highlightedWindow != windows.at(i))
                setHighlightedWindow(windows.at(i));
            break;
        }
    }
    if (m_highlightedWindow && m_motionManager.transformedGeometry(m_highlightedWindow).contains(me->pos()))
        updateCloseWindow();
    else
        m_closeView->hide();

    if (e->type() != QEvent::MouseButtonPress)
        return;

    if (me->button() == Qt::LeftButton) {
        if (hovering) {
            // mouse is hovering above a window - use MouseActionsWindow
            mouseActionWindow(m_leftButtonWindow);
        } else {
            // mouse is hovering above desktop - use MouseActionsDesktop
            mouseActionDesktop(m_leftButtonDesktop);
        }
    }
    if (me->button() == Qt::MidButton) {
        if (hovering) {
            // mouse is hovering above a window - use MouseActionsWindow
            mouseActionWindow(m_middleButtonWindow);
        } else {
            // mouse is hovering above desktop - use MouseActionsDesktop
            mouseActionDesktop(m_middleButtonDesktop);
        }
    }
    if (me->button() == Qt::RightButton) {
        if (hovering) {
            // mouse is hovering above a window - use MouseActionsWindow
            mouseActionWindow(m_rightButtonWindow);
        } else {
            // mouse is hovering above desktop - use MouseActionsDesktop
            mouseActionDesktop(m_rightButtonDesktop);
        }
    }
}

void PresentWindowsEffect::mouseActionWindow(WindowMouseAction& action)
{
    switch(action) {
    case WindowActivateAction:
        if (m_highlightedWindow)
            effects->activateWindow(m_highlightedWindow);
        setActive(false);
        break;
    case WindowExitAction:
        setActive(false);
        break;
    case WindowToCurrentDesktopAction:
        if (m_highlightedWindow)
            effects->windowToDesktop(m_highlightedWindow, effects->currentDesktop());
        break;
    case WindowToAllDesktopsAction:
        if (m_highlightedWindow) {
            if (m_highlightedWindow->isOnAllDesktops())
                effects->windowToDesktop(m_highlightedWindow, effects->currentDesktop());
            else
                effects->windowToDesktop(m_highlightedWindow, NET::OnAllDesktops);
        }
        break;
    case WindowMinimizeAction:
        if (m_highlightedWindow) {
            if (m_highlightedWindow->isMinimized())
                m_highlightedWindow->unminimize();
            else
                m_highlightedWindow->minimize();
        }
        break;
    case WindowCloseAction:
        if (m_highlightedWindow) {
            m_highlightedWindow->closeWindow();
        }
        break;
    default:
        break;
    }
}

void PresentWindowsEffect::mouseActionDesktop(DesktopMouseAction& action)
{
    switch(action) {
    case DesktopActivateAction:
        if (m_highlightedWindow)
            effects->activateWindow(m_highlightedWindow);
        setActive(false);
        break;
    case DesktopExitAction:
        setActive(false);
        break;
    case DesktopShowDesktopAction:
        effects->setShowingDesktop(true);
        setActive(false);
    default:
        break;
    }
}


void PresentWindowsEffect::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (m_mode == ModeCurrentDesktop && shortcut.contains(e->key() + e->modifiers())) {
            toggleActive();
            return;
        }
        if (m_mode == ModeAllDesktops && shortcutAll.contains(e->key() + e->modifiers())) {
            toggleActiveAllDesktops();
            return;
        }
        if (m_mode == ModeWindowClass && shortcutClass.contains(e->key() + e->modifiers())) {
            toggleActiveClass();
            return;
        }

        switch(e->key()) {
            // Wrap only if not auto-repeating
        case Qt::Key_Left:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, -1, 0, !e->isAutoRepeat()));
            break;
        case Qt::Key_Right:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, 1, 0, !e->isAutoRepeat()));
            break;
        case Qt::Key_Up:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, -1, !e->isAutoRepeat()));
            break;
        case Qt::Key_Down:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, 1, !e->isAutoRepeat()));
            break;
        case Qt::Key_Home:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, -1000, 0, false));
            break;
        case Qt::Key_End:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, 1000, 0, false));
            break;
        case Qt::Key_PageUp:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, -1000, false));
            break;
        case Qt::Key_PageDown:
            if (m_highlightedWindow)
                setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, 1000, false));
            break;
        case Qt::Key_Backspace:
            if (!m_windowFilter.isEmpty()) {
                m_windowFilter.remove(m_windowFilter.length() - 1, 1);
                updateFilterFrame();
                rearrangeWindows();
            }
            return;
        case Qt::Key_Escape:
            setActive(false);
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_highlightedWindow)
                effects->activateWindow(m_highlightedWindow);
            setActive(false);
            return;
        case Qt::Key_Tab:
            return; // Nothing at the moment
        case Qt::Key_Delete:
            if (!m_windowFilter.isEmpty()) {
                m_windowFilter.clear();
                updateFilterFrame();
                rearrangeWindows();
            }
            break;
        case 0:
            return; // HACK: Workaround for Qt bug on unbound keys (#178547)
        default:
            if (!e->text().isEmpty()) {
                m_windowFilter.append(e->text());
                updateFilterFrame();
                rearrangeWindows();
                return;
            }
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// Tab box

void PresentWindowsEffect::tabBoxAdded(int mode)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    if (m_activated)
        return;
    if (((mode == TabBoxWindowsMode && m_tabBoxAllowed) ||
            (mode == TabBoxWindowsAlternativeMode && m_tabBoxAlternativeAllowed)) &&
            effects->currentTabBoxWindowList().count() > 0) {
        m_tabBoxEnabled = true;
        setActive(true);
        if (m_activated)
            effects->refTabBox();
        else
            m_tabBoxEnabled = false;
    }
}

void PresentWindowsEffect::tabBoxClosed()
{
    if (m_activated) {
        effects->unrefTabBox();
        setActive(false, true);
        m_tabBoxEnabled = false;
    }
}

void PresentWindowsEffect::tabBoxUpdated()
{
    if (m_activated)
        setHighlightedWindow(effects->currentTabBoxWindow());
}

void PresentWindowsEffect::tabBoxKeyEvent(QKeyEvent* event)
{
    if (!m_activated)
        return;
    // not using the "normal" grabbedKeyboardEvent as we don't want to filter in tabbox
    if (event->type() == QEvent::KeyPress) {
        switch(event->key()) {
            // Wrap only if not auto-repeating
        case Qt::Key_Left:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, -1, 0, !event->isAutoRepeat()));
            break;
        case Qt::Key_Right:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 1, 0, !event->isAutoRepeat()));
            break;
        case Qt::Key_Up:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, -1, !event->isAutoRepeat()));
            break;
        case Qt::Key_Down:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, 1, !event->isAutoRepeat()));
            break;
        case Qt::Key_Home:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, -1000, 0, false));
            break;
        case Qt::Key_End:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 1000, 0, false));
            break;
        case Qt::Key_PageUp:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, -1000, false));
            break;
        case Qt::Key_PageDown:
            setHighlightedWindow(relativeWindow(m_highlightedWindow, 0, 1000, false));
            break;
        default:
            // nothing
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// Atom handling
void PresentWindowsEffect::propertyNotify(EffectWindow* w, long a)
{
    if (!w || (a != m_atomDesktop && a != m_atomWindows))
        return; // Not our atom

    if (a == m_atomDesktop) {
        QByteArray byteData = w->readProperty(m_atomDesktop, m_atomDesktop, 32);
        if (byteData.length() < 1) {
            // Property was removed, end present windows
            setActive(false);
            return;
        }
        long* data = reinterpret_cast<long*>(byteData.data());

        if (!data[0]) {
            // Purposely ending present windows by issuing a NULL target
            setActive(false);
            return;
        }
        // present windows is active so don't do anything
        if (m_activated)
            return;

        int desktop = data[0];
        if (desktop > effects->numberOfDesktops())
            return;
        if (desktop == -1)
            toggleActiveAllDesktops();
        else {
            m_mode = ModeSelectedDesktop;
            m_desktop = desktop;
            m_managerWindow = w;
            setActive(true);
        }
    } else if (a == m_atomWindows) {
        QByteArray byteData = w->readProperty(m_atomWindows, m_atomWindows, 32);
        if (byteData.length() < 1) {
            // Property was removed, end present windows
            setActive(false);
            return;
        }
        long* data = reinterpret_cast<long*>(byteData.data());

        if (!data[0]) {
            // Purposely ending present windows by issuing a NULL target
            setActive(false);
            return;
        }
        // present windows is active so don't do anything
        if (m_activated)
            return;

        // for security clear selected windows
        m_selectedWindows.clear();
        int length = byteData.length() / sizeof(data[0]);
        for (int i = 0; i < length; i++) {
            EffectWindow* foundWin = effects->findWindow(data[i]);
            if (!foundWin) {
                kDebug(1212) << "Invalid window targetted for present windows. Requested:" << data[i];
                continue;
            }
            m_selectedWindows.append(foundWin);
        }
        m_mode = ModeWindowGroup;
        m_managerWindow = w;
        setActive(true);
    }
}

//-----------------------------------------------------------------------------
// Window rearranging

void PresentWindowsEffect::rearrangeWindows()
{
    if (!m_activated)
        return;

    effects->addRepaintFull(); // Trigger the first repaint
    m_closeView->hide();

    // Work out which windows are on which screens
    EffectWindowList windowlist;
    QList<EffectWindowList> windowlists;
    for (int i = 0; i < effects->numScreens(); i++)
        windowlists.append(EffectWindowList());

    if (m_windowFilter.isEmpty()) {
        if (m_tabBoxEnabled)
            // Assume we correctly set things up, should be identical to
            // m_motionManager except just in a slightly different order.
            windowlist = effects->currentTabBoxWindowList();
        else
            windowlist = m_motionManager.managedWindows();
        foreach (EffectWindow * w, m_motionManager.managedWindows()) {
            if (m_windowData[w].deleted)
                continue; // don't include closed windows
            windowlists[w->screen()].append(w);
            assert(m_windowData.contains(w));
            m_windowData[w].visible = true;
        }
    } else {
        // Can we move this filtering somewhere else?
        foreach (EffectWindow * w, m_motionManager.managedWindows()) {
            assert(m_windowData.contains(w));
            if (m_windowData[w].deleted)
                continue; // don't include closed windows
            if (w->caption().contains(m_windowFilter, Qt::CaseInsensitive) ||
                    w->windowClass().contains(m_windowFilter, Qt::CaseInsensitive) ||
                    w->windowRole().contains(m_windowFilter, Qt::CaseInsensitive)) {
                windowlist.append(w);
                windowlists[w->screen()].append(w);
                m_windowData[w].visible = true;
            } else
                m_windowData[w].visible = false;
        }
    }
    if (windowlist.isEmpty()) {
        setHighlightedWindow(NULL);   // TODO: Having a NULL highlighted window isn't really safe
        return;
    }

    // We filtered out the highlighted window
    if (m_highlightedWindow) {
        assert(m_windowData.contains(m_highlightedWindow));
        if (m_windowData[m_highlightedWindow].visible == false)
            setHighlightedWindow(findFirstWindow());
    } else if (m_tabBoxEnabled)
        setHighlightedWindow(effects->currentTabBoxWindow());
    else
        setHighlightedWindow(findFirstWindow());

    int screens = m_tabBoxEnabled ? 1 : effects->numScreens();
    for (int screen = 0; screen < screens; screen++) {
        EffectWindowList windows;
        if (m_tabBoxEnabled) {
            // Kind of cheating here
            screen = effects->activeScreen();
            windows = windowlist;
        } else
            windows = windowlists[screen];

        // Don't rearrange if the grid is the same size as what it was before to prevent
        // windows moving to a better spot if one was filtered out.
        if (m_layoutMode == LayoutRegularGrid &&
                m_gridSizes[screen].columns * m_gridSizes[screen].rows &&
                windows.size() < m_gridSizes[screen].columns * m_gridSizes[screen].rows &&
                windows.size() > (m_gridSizes[screen].columns - 1) * m_gridSizes[screen].rows &&
                windows.size() > m_gridSizes[screen].columns *(m_gridSizes[screen].rows - 1) &&
                !m_tabBoxEnabled)
            continue;

        // No point continuing if there is no windows to process
        if (!windows.count())
            continue;

        calculateWindowTransformations(windows, screen, m_motionManager);
    }

    // Resize text frames if required
    QFontMetrics* metrics = NULL; // All fonts are the same
    foreach (EffectWindow * w, m_motionManager.managedWindows()) {
        if (!metrics)
            metrics = new QFontMetrics(m_windowData[w].textFrame->font());
        QRect geom = m_motionManager.targetGeometry(w).toRect();
        QString string = metrics->elidedText(w->caption(), Qt::ElideRight, geom.width() * 0.9);
        if (string != m_windowData[w].textFrame->text())
            m_windowData[w].textFrame->setText(string);
    }
    delete metrics;
}

void PresentWindowsEffect::calculateWindowTransformations(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager, bool external)
{
    if (m_layoutMode == LayoutRegularGrid || m_tabBoxEnabled)   // Force the grid for window switching
        calculateWindowTransformationsClosest(windowlist, screen, motionManager);
    else if (m_layoutMode == LayoutFlexibleGrid)
        calculateWindowTransformationsKompose(windowlist, screen, motionManager);
    else
        calculateWindowTransformationsNatural(windowlist, screen, motionManager);

    // If called externally we don't need to remember this data
    if (external)
        m_windowData.clear();
}

void PresentWindowsEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager)
{
    // This layout mode requires at least one window visible
    if (windowlist.count() == 0)
        return;

    QRect area = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (m_showPanel)   // reserve space for the panel
        area = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());
    int columns = int(ceil(sqrt(double(windowlist.count()))));
    int rows = int(ceil(windowlist.count() / double(columns)));

    // Remember the size for later
    // If we are using this layout externally we don't need to remember m_gridSizes.
    if (m_gridSizes.size() != 0) {
        m_gridSizes[screen].columns = columns;
        m_gridSizes[screen].rows = rows;
    }

    // Assign slots
    foreach (EffectWindow * w, windowlist)
    m_windowData[w].slot = -1;
    if (m_tabBoxEnabled) {
        // Rearrange in the correct order. As rearrangeWindows() is only ever
        // called once so we can use effects->currentTabBoxWindow() here.
        int selectedWindow = qMax(0, windowlist.indexOf(effects->currentTabBoxWindow()));
        int slot = 0;
        for (int i = selectedWindow; i < windowlist.count(); i++)
            m_windowData[windowlist[i]].slot = slot++;
        for (int i = selectedWindow - 1; i >= 0; i--)
            m_windowData[windowlist[i]].slot = slot++;
    } else {
        for (;;) {
            // Assign each window to the closest available slot
            assignSlots(windowlist, area, columns, rows);
            // Leave only the closest window in each slot, remove further conflicts
            getBestAssignments(windowlist);
            bool allAssigned = true;
            foreach (EffectWindow * w, windowlist)
            if (m_windowData[w].slot == -1) {
                allAssigned = false;
                break;
            }
            if (allAssigned)
                break;
        }
    }

    int slotWidth = area.width() / columns;
    int slotHeight = area.height() / rows;
    foreach (EffectWindow * w, windowlist) {
        assert(m_windowData[w].slot != -1);

        // Work out where the slot is
        QRect target(
            area.x() + (m_windowData[w].slot % columns) * slotWidth,
            area.y() + (m_windowData[w].slot / columns) * slotHeight,
            slotWidth, slotHeight);
        target.adjust(10, 10, -10, -10);   // Borders
        double scale;
        if (target.width() / double(w->width()) < target.height() / double(w->height())) {
            // Center vertically
            scale = target.width() / double(w->width());
            target.moveTop(target.top() + (target.height() - int(w->height() * scale)) / 2);
            target.setHeight(int(w->height() * scale));
        } else {
            // Center horizontally
            scale = target.height() / double(w->height());
            target.moveLeft(target.left() + (target.width() - int(w->width() * scale)) / 2);
            target.setWidth(int(w->width() * scale));
        }
        // Don't scale the windows too much
        if (scale > 2.0 || (scale > 1.0 && (w->width() > 300 || w->height() > 300))) {
            scale = (w->width() > 300 || w->height() > 300) ? 1.0 : 2.0;
            target = QRect(
                         target.center().x() - int(w->width() * scale) / 2,
                         target.center().y() - int(w->height() * scale) / 2,
                         scale * w->width(), scale * w->height());
        }
        motionManager.moveWindow(w, target);
    }
}

void PresentWindowsEffect::calculateWindowTransformationsKompose(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager)
{
    // This layout mode requires at least one window visible
    if (windowlist.count() == 0)
        return;

    QRect availRect = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (m_showPanel)   // reserve space for the panel
        availRect = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());
    qSort(windowlist);   // The location of the windows should not depend on the stacking order

    // Following code is taken from Kompose 0.5.4, src/komposelayout.cpp

    int spacing = 10;
    int rows, columns;
    double parentRatio = availRect.width() / (double)availRect.height();
    // Use more columns than rows when parent's width > parent's height
    if (parentRatio > 1) {
        columns = (int)ceil(sqrt((double)windowlist.count()));
        rows = (int)ceil((double)windowlist.count() / (double)columns);
    } else {
        rows = (int)ceil(sqrt((double)windowlist.count()));
        columns = (int)ceil((double)windowlist.count() / (double)rows);
    }
    //kDebug(1212) << "Using " << rows << " rows & " << columns << " columns for " << windowlist.count() << " clients";

    // Calculate width & height
    int w = (availRect.width() - (columns + 1) * spacing) / columns;
    int h = (availRect.height() - (rows + 1) * spacing) / rows;

    EffectWindowList::iterator it(windowlist.begin());
    QList<QRect> geometryRects;
    QList<int> maxRowHeights;
    // Process rows
    for (int i = 0; i < rows; ++i) {
        int xOffsetFromLastCol = 0;
        int maxHeightInRow = 0;
        // Process columns
        for (int j = 0; j < columns; ++j) {
            EffectWindow* window;

            // Check for end of List
            if (it == windowlist.end())
                break;
            window = *it;

            // Calculate width and height of widget
            double ratio = aspectRatio(window);

            int widgetw = 100;
            int widgeth = 100;
            int usableW = w;
            int usableH = h;

            // use width of two boxes if there is no right neighbour
            if (window == windowlist.last() && j != columns - 1) {
                usableW = 2 * w;
            }
            ++it; // We need access to the neighbour in the following
            // expand if right neighbour has ratio < 1
            if (j != columns - 1 && it != windowlist.end() && aspectRatio(*it) < 1) {
                int addW = w - widthForHeight(*it, h);
                if (addW > 0) {
                    usableW = w + addW;
                }
            }

            if (ratio == -1) {
                widgetw = w;
                widgeth = h;
            } else {
                double widthByHeight = widthForHeight(window, usableH);
                double heightByWidth = heightForWidth(window, usableW);
                if ((ratio >= 1.0 && heightByWidth <= usableH) ||
                        (ratio < 1.0 && widthByHeight > usableW)) {
                    widgetw = usableW;
                    widgeth = (int)heightByWidth;
                } else if ((ratio < 1.0 && widthByHeight <= usableW) ||
                          (ratio >= 1.0 && heightByWidth > usableH)) {
                    widgeth = usableH;
                    widgetw = (int)widthByHeight;
                }
                // Don't upscale large-ish windows
                if (widgetw > window->width() && (window->width() > 300 || window->height() > 300)) {
                    widgetw = window->width();
                    widgeth = window->height();
                }
            }

            // Set the Widget's size

            int alignmentXoffset = 0;
            int alignmentYoffset = 0;
            if (i == 0 && h > widgeth)
                alignmentYoffset = h - widgeth;
            if (j == 0 && w > widgetw)
                alignmentXoffset = w - widgetw;
            QRect geom(availRect.x() + j *(w + spacing) + spacing + alignmentXoffset + xOffsetFromLastCol,
                       availRect.y() + i *(h + spacing) + spacing + alignmentYoffset,
                       widgetw, widgeth);
            geometryRects.append(geom);

            // Set the x offset for the next column
            if (alignmentXoffset == 0)
                xOffsetFromLastCol += widgetw - w;
            if (maxHeightInRow < widgeth)
                maxHeightInRow = widgeth;
        }
        maxRowHeights.append(maxHeightInRow);
    }

    int topOffset = 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            int pos = i * columns + j;
            if (pos >= windowlist.count())
                break;

            EffectWindow* window = windowlist[pos];
            QRect target = geometryRects[pos];
            target.setY(target.y() + topOffset);
            m_windowData[window].slot = pos;
            motionManager.moveWindow(window, target);

            //kDebug(1212) << "Window '" << window->caption() << "' gets moved to (" <<
            //        mWindowData[window].area.left() << "; " << mWindowData[window].area.right() <<
            //        "), scale: " << mWindowData[window].scale << endl;
        }
        if (maxRowHeights[i] - h > 0)
            topOffset += maxRowHeights[i] - h;
    }
}

void PresentWindowsEffect::calculateWindowTransformationsNatural(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager)
{
    // If windows do not overlap they scale into nothingness, fix by resetting. To reproduce
    // just have a single window on a Xinerama screen or have two windows that do not touch.
    // TODO: Work out why this happens, is most likely a bug in the manager.
    foreach (EffectWindow * w, windowlist)
    if (motionManager.transformedGeometry(w) == w->geometry())
        motionManager.reset(w);

    if (windowlist.count() == 1) {
        // Just move the window to its original location to save time
        if (effects->clientArea(FullScreenArea, windowlist[0]).contains(windowlist[0]->geometry())) {
            motionManager.moveWindow(windowlist[0], windowlist[0]->geometry());
            return;
        }
    }

    // As we are using pseudo-random movement (See "slot") we need to make sure the list
    // is always sorted the same way no matter which window is currently active.
    qSort(windowlist);

    QRect area = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (m_showPanel)   // reserve space for the panel
        area = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());
    QRect bounds = area;
    int direction = 0;
    QHash<EffectWindow*, QRect> targets;
    QHash<EffectWindow*, int> directions;
    foreach (EffectWindow * w, windowlist) {
        bounds = bounds.united(w->geometry());
        targets[w] = w->geometry();
        // Reuse the unused "slot" as a preferred direction attribute. This is used when the window
        // is on the edge of the screen to try to use as much screen real estate as possible.
        directions[w] = direction;
        direction++;
        if (direction == 4)
            direction = 0;
    }

    // Iterate over all windows, if two overlap push them apart _slightly_ as we try to
    // brute-force the most optimal positions over many iterations.
    bool overlap;
    do {
        overlap = false;
        foreach (EffectWindow * w, windowlist) {
            foreach (EffectWindow * e, windowlist) {
                if (w != e && targets[w].adjusted(-5, -5, 5, 5).intersects(
                            targets[e].adjusted(-5, -5, 5, 5))) {
                    overlap = true;

                    // Determine pushing direction
                    QPoint diff(targets[e].center() - targets[w].center());
                    // Prevent dividing by zero and non-movement
                    if (diff.x() == 0 && diff.y() == 0)
                        diff.setX(1);
                    // Try to keep screen aspect ratio
                    //if ( bounds.height() / bounds.width() > area.height() / area.width() )
                    //    diff.setY( diff.y() / 2 );
                    //else
                    //    diff.setX( diff.x() / 2 );
                    // Approximate a vector of between 10px and 20px in magnitude in the same direction
                    diff *= m_accuracy / double(diff.manhattanLength());
                    // Move both windows apart
                    targets[w].translate(-diff);
                    targets[e].translate(diff);

                    // Try to keep the bounding rect the same aspect as the screen so that more
                    // screen real estate is utilised. We do this by splitting the screen into nine
                    // equal sections, if the window center is in any of the corner sections pull the
                    // window towards the outer corner. If it is in any of the other edge sections
                    // alternate between each corner on that edge. We don't want to determine it
                    // randomly as it will not produce consistant locations when using the filter.
                    // Only move one window so we don't cause large amounts of unnecessary zooming
                    // in some situations. We need to do this even when expanding later just in case
                    // all windows are the same size.
                    // (We are using an old bounding rect for this, hopefully it doesn't matter)
                    int xSection = (targets[w].x() - bounds.x()) / (bounds.width() / 3);
                    int ySection = (targets[w].y() - bounds.y()) / (bounds.height() / 3);
                    diff = QPoint(0, 0);
                    if (xSection != 1 || ySection != 1) { // Remove this if you want the center to pull as well
                        if (xSection == 1)
                            xSection = (directions[w] / 2 ? 2 : 0);
                        if (ySection == 1)
                            ySection = (directions[w] % 2 ? 2 : 0);
                    }
                    if (xSection == 0 && ySection == 0)
                        diff = QPoint(bounds.topLeft() - targets[w].center());
                    if (xSection == 2 && ySection == 0)
                        diff = QPoint(bounds.topRight() - targets[w].center());
                    if (xSection == 2 && ySection == 2)
                        diff = QPoint(bounds.bottomRight() - targets[w].center());
                    if (xSection == 0 && ySection == 2)
                        diff = QPoint(bounds.bottomLeft() - targets[w].center());
                    if (diff.x() != 0 || diff.y() != 0) {
                        diff *= m_accuracy / double(diff.manhattanLength());
                        targets[w].translate(diff);
                    }

                    // Update bounding rect
                    bounds = bounds.united(targets[w]);
                    bounds = bounds.united(targets[e]);
                }
            }
        }
    } while (overlap);

    // Work out scaling by getting the most top-left and most bottom-right window coords.
    // The 20's and 10's are so that the windows don't touch the edge of the screen.
    double scale;
    if (bounds == area)
        scale = 1.0; // Don't add borders to the screen
    else if (area.width() / double(bounds.width()) < area.height() / double(bounds.height()))
        scale = (area.width() - 20) / double(bounds.width());
    else
        scale = (area.height() - 20) / double(bounds.height());
    // Make bounding rect fill the screen size for later steps
    bounds = QRect(
                 bounds.x() - (area.width() - 20 - bounds.width() * scale) / 2 - 10 / scale,
                 bounds.y() - (area.height() - 20 - bounds.height() * scale) / 2 - 10 / scale,
                 area.width() / scale,
                 area.height() / scale
             );

    // Move all windows back onto the screen and set their scale
    foreach (EffectWindow * w, windowlist)
    targets[w] = QRect(
                     (targets[w].x() - bounds.x()) * scale + area.x(),
                     (targets[w].y() - bounds.y()) * scale + area.y(),
                     targets[w].width() * scale,
                     targets[w].height() * scale
                 );

    // Try to fill the gaps by enlarging windows if they have the space
    if (m_fillGaps) {
        // Don't expand onto or over the border
        QRegion borderRegion(area.adjusted(-200, -200, 200, 200));
        borderRegion ^= area.adjusted(10 / scale, 10 / scale, -10 / scale, -10 / scale);

        bool moved;
        do {
            moved = false;
            foreach (EffectWindow * w, windowlist) {
                QRect oldRect;
                // This may cause some slight distortion if the windows are enlarged a large amount
                int widthDiff = m_accuracy;
                int heightDiff = heightForWidth(w, targets[w].width() + widthDiff) - targets[w].height();
                int xDiff = widthDiff / 2;  // Also move a bit in the direction of the enlarge, allows the
                int yDiff = heightDiff / 2; // center windows to be enlarged if there is gaps on the side.

                // Attempt enlarging to the top-right
                oldRect = targets[w];
                targets[w] = QRect(
                                 targets[w].x() + xDiff,
                                 targets[w].y() - yDiff - heightDiff,
                                 targets[w].width() + widthDiff,
                                 targets[w].height() + heightDiff
                             );
                if (isOverlappingAny(w, targets, borderRegion))
                    targets[w] = oldRect;
                else
                    moved = true;

                // Attempt enlarging to the bottom-right
                oldRect = targets[w];
                targets[w] = QRect(
                                 targets[w].x() + xDiff,
                                 targets[w].y() + yDiff,
                                 targets[w].width() + widthDiff,
                                 targets[w].height() + heightDiff
                             );
                if (isOverlappingAny(w, targets, borderRegion))
                    targets[w] = oldRect;
                else
                    moved = true;

                // Attempt enlarging to the bottom-left
                oldRect = targets[w];
                targets[w] = QRect(
                                 targets[w].x() - xDiff - widthDiff,
                                 targets[w].y() + yDiff,
                                 targets[w].width() + widthDiff,
                                 targets[w].height() + heightDiff
                             );
                if (isOverlappingAny(w, targets, borderRegion))
                    targets[w] = oldRect;
                else
                    moved = true;

                // Attempt enlarging to the top-left
                oldRect = targets[w];
                targets[w] = QRect(
                                 targets[w].x() - xDiff - widthDiff,
                                 targets[w].y() - yDiff - heightDiff,
                                 targets[w].width() + widthDiff,
                                 targets[w].height() + heightDiff
                             );
                if (isOverlappingAny(w, targets, borderRegion))
                    targets[w] = oldRect;
                else
                    moved = true;
            }
        } while (moved);

        // The expanding code above can actually enlarge windows over 1.0/2.0 scale, we don't like this
        // We can't add this to the loop above as it would cause a never-ending loop so we have to make
        // do with the less-than-optimal space usage with using this method.
        foreach (EffectWindow * w, windowlist) {
            double scale = targets[w].width() / double(w->width());
            if (scale > 2.0 || (scale > 1.0 && (w->width() > 300 || w->height() > 300))) {
                scale = (w->width() > 300 || w->height() > 300) ? 1.0 : 2.0;
                targets[w] = QRect(
                                 targets[w].center().x() - int(w->width() * scale) / 2,
                                 targets[w].center().y() - int(w->height() * scale) / 2,
                                 w->width() * scale,
                                 w->height() * scale);
            }
        }
    }

    // Notify the motion manager of the targets
    foreach (EffectWindow * w, windowlist)
    motionManager.moveWindow(w, targets[w]);
}

//-----------------------------------------------------------------------------
// Helper functions for window rearranging

void PresentWindowsEffect::assignSlots(EffectWindowList windowlist, const QRect &area, int columns, int rows)
{
    QVector< bool > taken;
    taken.fill(false, columns * rows);
    foreach (EffectWindow * w, windowlist)
    if (m_windowData[w].slot != -1)
        taken[ m_windowData[w].slot ] = true;
    int slotWidth = area.width() / columns;
    int slotHeight = area.height() / rows;
    foreach (EffectWindow * w, windowlist) {
        WindowData *wData = &m_windowData[w];
        if (wData->slot != -1)
            continue; // it already has a slot
        QPoint pos = w->geometry().center();
        if (pos.x() < area.left())
            pos.setX(area.left());
        if (pos.x() > area.right())
            pos.setX(area.right());
        if (pos.y() < area.top())
            pos.setY(area.top());
        if (pos.y() > area.bottom())
            pos.setY(area.bottom());
        int distance = INT_MAX;
        for (int x = 0; x < columns; x++)
            for (int y = 0; y < rows; y++) {
                int slot = x + y * columns;
                if (taken[slot])
                    continue;
                int xdiff = pos.x() - (area.x() + slotWidth * x + slotWidth / 2);
                int ydiff = pos.y() - (area.y() + slotHeight * y + slotHeight / 2);
                int dist = int(sqrt(double(xdiff * xdiff + ydiff * ydiff)));
                if (dist < distance) {
                    distance = dist;
                    wData->slot = slot;
                    wData->slot_distance = distance;
                }
            }
    }
}

void PresentWindowsEffect::getBestAssignments(EffectWindowList windowlist)
{
    foreach (EffectWindow * w1, windowlist) {
        WindowData *windowData1 = &m_windowData[w1];
        foreach (EffectWindow * w2, windowlist) {
            WindowData *windowData2 = &m_windowData[w2];
            if (w1 != w2 && windowData1->slot == windowData2->slot &&
                    windowData1->slot_distance >= windowData2->slot_distance)
                windowData1->slot = -1;
        }
    }
}

bool PresentWindowsEffect::isOverlappingAny(EffectWindow *w, const QHash<EffectWindow*, QRect> &targets, const QRegion &border)
{
    if (border.intersects(targets[w]))
        return true;
    // Is there a better way to do this?
    QHash<EffectWindow*, QRect>::const_iterator i;
    for (i = targets.constBegin(); i != targets.constEnd(); ++i) {
        if (w == i.key())
            continue;
        if (targets[w].adjusted(-5, -5, 5, 5).intersects(
                    i.value().adjusted(-5, -5, 5, 5)))
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Activation

void PresentWindowsEffect::setActive(bool active, bool closingTab)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    if (m_activated == active)
        return;
    if (m_activated && m_tabBoxEnabled && !closingTab) {
        effects->closeTabBox();
        return;
    }
    m_activated = active;
    if (m_activated) {
        m_decalOpacity = 0.0;
        m_highlightedWindow = NULL;
        m_windowFilter.clear();

        m_closeView = new CloseWindowView();
        connect(m_closeView, SIGNAL(close()), SLOT(closeWindow()));

        // Add every single window to m_windowData (Just calling [w] creates it)
        foreach (EffectWindow * w, effects->stackingOrder()) {
            if (m_windowData.contains(w))    // Happens if we reactivate before the ending animation finishes
                continue;
            m_windowData[w].visible = isVisibleWindow(w);
            m_windowData[w].deleted = false;
            m_windowData[w].referenced = false;
            m_windowData[w].opacity = 0.0;
            if (w->isOnCurrentDesktop() && !w->isMinimized())
                m_windowData[w].opacity = 1.0;
            m_windowData[w].highlight = 1.0;
            m_windowData[w].textFrame = effects->effectFrame(EffectFrameUnstyled, false);
            QFont font;
            font.setBold(true);
            font.setPointSize(12);
            m_windowData[w].textFrame->setFont(font);
            m_windowData[w].iconFrame = effects->effectFrame(EffectFrameUnstyled, false);
            m_windowData[w].iconFrame->setAlignment(Qt::AlignRight | Qt::AlignBottom);
            m_windowData[w].iconFrame->setIcon(w->icon());
        }

        if (m_tabBoxEnabled) {
            foreach (EffectWindow * w, effects->currentTabBoxWindowList()) {
                if (!w)
                    continue;
                m_motionManager.manage(w);
                assert(m_windowData.contains(w));
                m_windowData[w].visible = effects->currentTabBoxWindowList().contains(w);
            }
            // Hide windows not in the list
            foreach (EffectWindow * w, effects->stackingOrder())
            m_windowData[w].visible = isVisibleWindow(w) &&
                                      (!isSelectableWindow(w) || effects->currentTabBoxWindowList().contains(w));
        } else {
            // Filter out special windows such as panels and taskbars
            foreach (EffectWindow * w, effects->stackingOrder())
            if (isSelectableWindow(w))
                m_motionManager.manage(w);
        }
        if (m_motionManager.managedWindows().isEmpty() ||
                ((m_motionManager.managedWindows().count() == 1) && m_motionManager.managedWindows().first()->isOnCurrentDesktop()  &&
                 (m_ignoreMinimized || !m_motionManager.managedWindows().first()->isMinimized()))) {
            // No point triggering if there is nothing to do
            m_activated = false;

            DataHash::iterator i = m_windowData.begin();
            while (i != m_windowData.end()) {
                delete i.value().textFrame;
                delete i.value().iconFrame;
                i++;
            }
            m_windowData.clear();

            m_motionManager.unmanageAll();
            return;
        }

        // Create temporary input window to catch mouse events
        m_input = effects->createFullScreenInputWindow(this, Qt::PointingHandCursor);
        m_hasKeyboardGrab = effects->grabKeyboard(this);
        effects->setActiveFullScreenEffect(this);

        m_gridSizes.clear();
        for (int i = 0; i < effects->numScreens(); i++)
            m_gridSizes.append(GridSize());

        rearrangeWindows();
        if (m_tabBoxEnabled)
            setHighlightedWindow(effects->currentTabBoxWindow());
        else
            setHighlightedWindow(effects->activeWindow());

        foreach (EffectWindow * w, effects->stackingOrder()) {
            if (w->isDock()) {
                w->setData(WindowForceBlurRole, QVariant(true));
            }
        }
    } else {
        // Fade in/out all windows
        EffectWindow *activeWindow = effects->activeWindow();
        if (m_tabBoxEnabled)
            activeWindow = effects->currentTabBoxWindow();
        int desktop = effects->currentDesktop();
        if (activeWindow && !activeWindow->isOnAllDesktops())
            desktop = activeWindow->desktop();
        foreach (EffectWindow * w, effects->stackingOrder()) {
            assert(m_windowData.contains(w));
            m_windowData[w].visible = (w->isOnDesktop(desktop) || w->isOnAllDesktops()) &&
                                      !w->isMinimized() && (w->visibleInClientGroup() || m_windowData[w].visible);
        }
        delete m_closeView;
        m_closeView = 0;

        // Move all windows back to their original position
        foreach (EffectWindow * w, m_motionManager.managedWindows())
        m_motionManager.moveWindow(w, w->geometry());
        m_filterFrame->free();
        m_windowFilter.clear();
        m_selectedWindows.clear();

        effects->destroyInputWindow(m_input);
        if (m_hasKeyboardGrab)
            effects->ungrabKeyboard();
        m_hasKeyboardGrab = false;

        // destroy atom on manager window
        if (m_managerWindow) {
            if (m_mode == ModeSelectedDesktop)
                m_managerWindow->deleteProperty(m_atomDesktop);
            else if (m_mode == ModeWindowGroup)
                m_managerWindow->deleteProperty(m_atomWindows);
            m_managerWindow = NULL;
        }
    }
    effects->addRepaintFull(); // Trigger the first repaint
}

//-----------------------------------------------------------------------------
// Filter box

void PresentWindowsEffect::updateFilterFrame()
{
    QRect area = effects->clientArea(ScreenArea, effects->activeScreen(), effects->currentDesktop());
    m_filterFrame->setPosition(QPoint(area.x() + area.width() / 2, area.y() + area.height() / 2));
    m_filterFrame->setText(i18n("Filter:\n%1", m_windowFilter));
}

//-----------------------------------------------------------------------------
// Helper functions

bool PresentWindowsEffect::isSelectableWindow(EffectWindow *w)
{
    if (w->isSpecialWindow() || w->isUtility())
        return false;
    if (w->isDeleted())
        return false;
    if (!w->acceptsFocus())
        return false;
    if (!w->visibleInClientGroup())
        return false;
    if (w->isSkipSwitcher())
        return false;
    if (w == effects->findWindow(m_closeView->winId()))
        return false;
    if (m_tabBoxEnabled)
        return true;
    if (m_ignoreMinimized && w->isMinimized())
        return false;
    switch(m_mode) {
    default:
    case ModeAllDesktops:
        return true;
    case ModeCurrentDesktop:
        return w->isOnCurrentDesktop();
    case ModeSelectedDesktop:
        return w->isOnDesktop(m_desktop);
    case ModeWindowGroup:
        return m_selectedWindows.contains(w);
    case ModeWindowClass:
        return m_class == w->windowClass();
    }
}

bool PresentWindowsEffect::isVisibleWindow(EffectWindow *w)
{
    if (w->isDesktop())
        return true;
    return isSelectableWindow(w);
}

void PresentWindowsEffect::setHighlightedWindow(EffectWindow *w)
{
    if (w == m_highlightedWindow || (w != NULL && !m_motionManager.isManaging(w)))
        return;

    m_closeView->hide();
    if (m_highlightedWindow)
        m_highlightedWindow->addRepaintFull(); // Trigger the first repaint
    m_highlightedWindow = w;
    if (m_highlightedWindow)
        m_highlightedWindow->addRepaintFull(); // Trigger the first repaint

    if (m_tabBoxEnabled && m_highlightedWindow)
        effects->setTabBoxWindow(w);
    updateCloseWindow();
}

void PresentWindowsEffect::updateCloseWindow()
{
    if (m_doNotCloseWindows)
        return;
    if (m_closeView->isVisible())
        return;
    if (!m_highlightedWindow) {
        m_closeView->hide();
        return;
    }
    const QRectF rect = m_motionManager.targetGeometry(m_highlightedWindow);
    m_closeView->setGeometry(rect.x() + rect.width() - m_closeView->sceneRect().width(), rect.y(),
                             m_closeView->sceneRect().width(), m_closeView->sceneRect().height());
    if (rect.contains(effects->cursorPos()))
        m_closeView->delayedShow();
    else
        m_closeView->hide();
}

void PresentWindowsEffect::closeWindow()
{
    if (m_highlightedWindow)
        m_highlightedWindow->closeWindow();
}

EffectWindow* PresentWindowsEffect::relativeWindow(EffectWindow *w, int xdiff, int ydiff, bool wrap) const
{
    // TODO: Is it possible to select hidden windows?
    EffectWindow* next;
    QRect area = effects->clientArea(FullArea, 0, effects->currentDesktop());
    QRect detectRect;

    // Detect across the width of the desktop
    if (xdiff != 0) {
        if (xdiff > 0) {
            // Detect right
            for (int i = 0; i < xdiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(0, wArea.y(), area.width(), wArea.height());
                next = NULL;
                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    if (!m_windowData[e].visible)
                        continue;
                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.x() > wArea.x()) {
                        if (next == NULL)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.x() < nArea.x())
                                next = e;
                        }
                    }
                }
                if (next == NULL) {
                    if (wrap)   // We are at the right-most window, now get the left-most one to wrap
                        return relativeWindow(w, -1000, 0, false);
                    break; // No more windows to the right
                }
                w = next;
            }
            return w;
        } else {
            // Detect left
            for (int i = 0; i < -xdiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(0, wArea.y(), area.width(), wArea.height());
                next = NULL;
                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    if (!m_windowData[e].visible)
                        continue;
                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.x() + eArea.width() < wArea.x() + wArea.width()) {
                        if (next == NULL)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.x() + eArea.width() > nArea.x() + nArea.width())
                                next = e;
                        }
                    }
                }
                if (next == NULL) {
                    if (wrap)   // We are at the left-most window, now get the right-most one to wrap
                        return relativeWindow(w, 1000, 0, false);
                    break; // No more windows to the left
                }
                w = next;
            }
            return w;
        }
    }

    // Detect across the height of the desktop
    if (ydiff != 0) {
        if (ydiff > 0) {
            // Detect down
            for (int i = 0; i < ydiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(wArea.x(), 0, wArea.width(), area.height());
                next = NULL;
                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    if (!m_windowData[e].visible)
                        continue;
                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.y() > wArea.y()) {
                        if (next == NULL)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.y() < nArea.y())
                                next = e;
                        }
                    }
                }
                if (next == NULL) {
                    if (wrap)   // We are at the bottom-most window, now get the top-most one to wrap
                        return relativeWindow(w, 0, -1000, false);
                    break; // No more windows to the bottom
                }
                w = next;
            }
            return w;
        } else {
            // Detect up
            for (int i = 0; i < -ydiff; i++) {
                QRectF wArea = m_motionManager.transformedGeometry(w);
                detectRect = QRect(wArea.x(), 0, wArea.width(), area.height());
                next = NULL;
                foreach (EffectWindow * e, m_motionManager.managedWindows()) {
                    if (!m_windowData[e].visible)
                        continue;
                    QRectF eArea = m_motionManager.transformedGeometry(e);
                    if (eArea.intersects(detectRect) &&
                            eArea.y() + eArea.height() < wArea.y() + wArea.height()) {
                        if (next == NULL)
                            next = e;
                        else {
                            QRectF nArea = m_motionManager.transformedGeometry(next);
                            if (eArea.y() + eArea.height() > nArea.y() + nArea.height())
                                next = e;
                        }
                    }
                }
                if (next == NULL) {
                    if (wrap)   // We are at the top-most window, now get the bottom-most one to wrap
                        return relativeWindow(w, 0, 1000, false);
                    break; // No more windows to the top
                }
                w = next;
            }
            return w;
        }
    }

    abort(); // Should never get here
}

EffectWindow* PresentWindowsEffect::findFirstWindow() const
{
    EffectWindow *topLeft = NULL;
    QRectF topLeftGeometry;
    foreach (EffectWindow * w, m_motionManager.managedWindows()) {
        QRectF geometry = m_motionManager.transformedGeometry(w);
        if (m_windowData[w].visible == false)
            continue; // Not visible
        if (m_windowData[w].deleted)
            continue; // Window has been closed
        if (topLeft == NULL) {
            topLeft = w;
            topLeftGeometry = geometry;
        } else if (geometry.x() < topLeftGeometry.x() || geometry.y() < topLeftGeometry.y()) {
            topLeft = w;
            topLeftGeometry = geometry;
        }
    }
    return topLeft;
}

void PresentWindowsEffect::globalShortcutChanged(const QKeySequence& seq)
{
    shortcut = KShortcut(seq);
}

void PresentWindowsEffect::globalShortcutChangedAll(const QKeySequence& seq)
{
    shortcutAll = KShortcut(seq);
}

void PresentWindowsEffect::globalShortcutChangedClass(const QKeySequence& seq)
{
    shortcutClass = KShortcut(seq);
}

/************************************************
* CloseWindowView
************************************************/
CloseWindowView::CloseWindowView(QWidget* parent)
    : QGraphicsView(parent)
    , m_delayedShowTimer(new QTimer(this))
{
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFrameShape(QFrame::NoFrame);
    QPalette pal = palette();
    pal.setColor(backgroundRole(), Qt::transparent);
    setPalette(pal);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // setup the scene
    QGraphicsScene* scene = new QGraphicsScene(this);
    m_closeButton = new Plasma::PushButton();
    m_closeButton->setIcon(KIcon("window-close"));
    scene->addItem(m_closeButton);
    connect(m_closeButton, SIGNAL(clicked()), SIGNAL(close()));

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout;
    layout->addItem(m_closeButton);

    QGraphicsWidget *form = new QGraphicsWidget;
    form->setLayout(layout);
    form->setGeometry(0, 0, 32, 32);
    scene->addItem(form);

    m_frame = new Plasma::FrameSvg(this);
    m_frame->setImagePath("dialogs/background");
    m_frame->setCacheAllRenderedFrames(true);
    m_frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);
    qreal left, top, right, bottom;
    m_frame->getMargins(left, top, right, bottom);
    qreal width = form->size().width() + left + right;
    qreal height = form->size().height() + top + bottom;
    m_frame->resizeFrame(QSizeF(width, height));
    Plasma::WindowEffects::enableBlurBehind(winId(), true, m_frame->mask());
    Plasma::WindowEffects::overrideShadow(winId(), true);
    form->setPos(left, top);
    scene->setSceneRect(QRectF(QPointF(0, 0), QSizeF(width, height)));
    setScene(scene);

    // setup the timer
    m_delayedShowTimer->setSingleShot(true);
    m_delayedShowTimer->setInterval(500);
    connect(m_delayedShowTimer, SIGNAL(timeout()), SLOT(show()));
}

void CloseWindowView::windowInputMouseEvent(QMouseEvent* e)
{
    if (m_delayedShowTimer->isActive())
        return;
    if (e->type() == QEvent::MouseMove) {
        mouseMoveEvent(e);
    } else if (e->type() == QEvent::MouseButtonPress) {
        mousePressEvent(e);
    } else if (e->type() == QEvent::MouseButtonDblClick) {
        mouseDoubleClickEvent(e);
    } else if (e->type() == QEvent::MouseButtonRelease) {
        mouseReleaseEvent(e);
    }
}

void CloseWindowView::drawBackground(QPainter* painter, const QRectF& rect)
{
    Q_UNUSED(rect)
    painter->setRenderHint(QPainter::Antialiasing);
    m_frame->paintFrame(painter);
}

void CloseWindowView::hide()
{
    m_delayedShowTimer->stop();
    QWidget::hide();
}

void CloseWindowView::delayedShow()
{
    if (isVisible() || m_delayedShowTimer->isActive())
        return;
    m_delayedShowTimer->start();
}


} // namespace

#include "presentwindows.moc"
