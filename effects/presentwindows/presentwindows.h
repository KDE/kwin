/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_PRESENTWINDOWS_H
#define KWIN_PRESENTWINDOWS_H

#include "presentwindows_proxy.h"

#include <kwineffects.h>
#include <kwineffectquickview.h>

#include <QElapsedTimer>

class QMouseEvent;
class QQuickView;

namespace KWin
{
class CloseWindowView : public EffectQuickScene
{
    Q_OBJECT
public:
    explicit CloseWindowView(QObject *parent = nullptr);
    void disarm();
Q_SIGNALS:
    void requestClose();
private Q_SLOTS:
    void clicked();
private:
    QElapsedTimer m_armTimer;
};

/**
 * Expose-like effect which shows all windows on current desktop side-by-side,
 *  letting the user select active window.
 */
class PresentWindowsEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int layoutMode READ layoutMode)
    Q_PROPERTY(bool showCaptions READ isShowCaptions)
    Q_PROPERTY(bool showIcons READ isShowIcons)
    Q_PROPERTY(bool doNotCloseWindows READ isDoNotCloseWindows)
    Q_PROPERTY(bool ignoreMinimized READ isIgnoreMinimized)
    Q_PROPERTY(int accuracy READ accuracy)
    Q_PROPERTY(bool fillGaps READ isFillGaps)
    Q_PROPERTY(int fadeDuration READ fadeDuration)
    Q_PROPERTY(bool showPanel READ isShowPanel)
    Q_PROPERTY(int leftButtonWindow READ leftButtonWindow)
    Q_PROPERTY(int rightButtonWindow READ rightButtonWindow)
    Q_PROPERTY(int middleButtonWindow READ middleButtonWindow)
    Q_PROPERTY(int leftButtonDesktop READ leftButtonDesktop)
    Q_PROPERTY(int middleButtonDesktop READ middleButtonDesktop)
    Q_PROPERTY(int rightButtonDesktop READ rightButtonDesktop)
    // TODO: electric borders
private:
    // Structures
    struct WindowData {
        bool visible;
        bool deleted;
        bool referenced;
        double opacity;
        double highlight;
        EffectFrame* textFrame;
        EffectFrame* iconFrame;
    };
    typedef QHash<EffectWindow*, WindowData> DataHash;
    struct GridSize {
        int columns;
        int rows;
    };

public:
    PresentWindowsEffect();
    ~PresentWindowsEffect() override;

    void reconfigure(ReconfigureFlags) override;
    void* proxy() override;

    // Screen painting
    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    // Window painting
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    // User interaction
    bool borderActivated(ElectricBorder border) override;
    void windowInputMouseEvent(QEvent *e) override;
    void grabbedKeyboardEvent(QKeyEvent *e) override;
    bool isActive() const override;

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    int requestedEffectChainPosition() const override {
        return 70;
    }

    enum { LayoutNatural, LayoutRegularGrid, LayoutFlexibleGrid }; // Layout modes
    enum PresentWindowsMode {
        ModeAllDesktops, // Shows windows of all desktops
        ModeCurrentDesktop, // Shows windows on current desktop
        ModeSelectedDesktop, // Shows windows of selected desktop via property (m_desktop)
        ModeWindowGroup, // Shows windows selected via property
        ModeWindowClass // Shows all windows of same class as selected class
    };
    enum WindowMouseAction {
        WindowNoAction = 0, // Nothing
        WindowActivateAction = 1, // Activates the window and deactivates the effect
        WindowExitAction = 2, // Deactivates the effect without activating new window
        WindowToCurrentDesktopAction = 3, // Brings window to current desktop
        WindowToAllDesktopsAction = 4, // Brings window to all desktops
        WindowMinimizeAction = 5, // Minimizes the window
        WindowCloseAction = 6 // Closes the window
    };
    enum DesktopMouseAction {
        DesktopNoAction = 0, // nothing
        DesktopActivateAction = 1, // Activates the window and deactivates the effect
        DesktopExitAction = 2, // Deactivates the effect without activating new window
        DesktopShowDesktopAction = 3 // Minimizes all windows
    };

    // for properties
    int layoutMode() const {
        return m_layoutMode;
    }
    bool isShowCaptions() const {
        return m_showCaptions;
    }
    bool isShowIcons() const {
        return m_showIcons;
    }
    bool isDoNotCloseWindows() const {
        return m_doNotCloseWindows;
    }
    bool isIgnoreMinimized() const {
        return m_ignoreMinimized;
    }
    int accuracy() const {
        return m_accuracy;
    }
    bool isFillGaps() const {
        return m_fillGaps;
    }
    int fadeDuration() const {
        return m_fadeDuration;
    }
    bool isShowPanel() const {
        return m_showPanel;
    }
    int leftButtonWindow() const {
        return m_leftButtonWindow;
    }
    int rightButtonWindow() const {
        return m_rightButtonWindow;
    }
    int middleButtonWindow() const {
        return m_middleButtonWindow;
    }
    int leftButtonDesktop() const {
        return m_leftButtonDesktop;
    }
    int middleButtonDesktop() const {
        return m_middleButtonDesktop;
    }
    int rightButtonDesktop() const {
        return m_rightButtonDesktop;
    }
public Q_SLOTS:
    void setActive(bool active);
    void toggleActive()  {
        m_mode = ModeCurrentDesktop;
        setActive(!m_activated);
    }
    void toggleActiveAllDesktops()  {
        m_mode = ModeAllDesktops;
        setActive(!m_activated);
    }
    void toggleActiveClass();

    // slots for global shortcut changed
    // needed to toggle the effect
    void globalShortcutChanged(QAction *action, const QKeySequence &seq);
    // EffectsHandler
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w, const QRect &old);
    // atoms
    void slotPropertyNotify(KWin::EffectWindow* w, long atom);

private Q_SLOTS:
    void closeWindow();

protected:
    // Window rearranging
    void rearrangeWindows();
    void reCreateGrids();
    void calculateWindowTransformations(EffectWindowList windowlist, int screen,
                                        WindowMotionManager& motionManager, bool external = false);
    void calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);
    void calculateWindowTransformationsKompose(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);
    void calculateWindowTransformationsNatural(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);

    // Helper functions for window rearranging
    inline double aspectRatio(EffectWindow *w) {
        return w->width() / double(w->height());
    }
    inline int widthForHeight(EffectWindow *w, int height) {
        return int((height / double(w->height())) * w->width());
    }
    inline int heightForWidth(EffectWindow *w, int width) {
        return int((width / double(w->width())) * w->height());
    }
    bool isOverlappingAny(EffectWindow *w, const QHash<EffectWindow*, QRect> &targets, const QRegion &border);

    // Filter box
    void updateFilterFrame();

    // Helper functions
    bool isSelectableWindow(EffectWindow *w);
    bool isVisibleWindow(EffectWindow *w);
    void setHighlightedWindow(EffectWindow *w);
    EffectWindow* relativeWindow(EffectWindow *w, int xdiff, int ydiff, bool wrap) const;
    EffectWindow* findFirstWindow() const;
    void updateCloseWindow();

    // Helper functions for mouse actions
    void mouseActionWindow(WindowMouseAction& action);
    void mouseActionDesktop(DesktopMouseAction& action);
    void inputEventUpdate(const QPoint &pos, QEvent::Type type = QEvent::None, Qt::MouseButton button = Qt::NoButton);

private:
    PresentWindowsEffectProxy m_proxy;
    friend class PresentWindowsEffectProxy;

    // User configuration settings
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_borderActivateAll;
    QList<ElectricBorder> m_borderActivateClass;
    int m_layoutMode;
    bool m_showCaptions;
    bool m_showIcons;
    bool m_doNotCloseWindows;
    int m_accuracy;
    bool m_fillGaps;
    double m_fadeDuration;
    bool m_showPanel;

    // Activation
    bool m_activated;
    bool m_ignoreMinimized;
    double m_decalOpacity;
    bool m_hasKeyboardGrab;
    PresentWindowsMode m_mode;
    int m_desktop;
    EffectWindowList m_selectedWindows;
    EffectWindow *m_managerWindow;
    QString m_class;
    bool m_needInitialSelection;

    // Window data
    WindowMotionManager m_motionManager;
    DataHash m_windowData;
    EffectWindow *m_highlightedWindow;

    // Grid layout info
    QList<GridSize> m_gridSizes;

    // Filter box
    EffectFrame* m_filterFrame;
    QString m_windowFilter;

    // Shortcut - needed to toggle the effect
    QList<QKeySequence> shortcut;
    QList<QKeySequence> shortcutAll;
    QList<QKeySequence> shortcutClass;

    // Atoms
    // Present windows for all windows of given desktop
    // -1 for all desktops
    long m_atomDesktop;
    // Present windows for group of window ids
    long m_atomWindows;

    // Mouse Actions
    WindowMouseAction m_leftButtonWindow;
    WindowMouseAction m_middleButtonWindow;
    WindowMouseAction m_rightButtonWindow;
    DesktopMouseAction m_leftButtonDesktop;
    DesktopMouseAction m_middleButtonDesktop;
    DesktopMouseAction m_rightButtonDesktop;

    CloseWindowView* m_closeView;
    Qt::Corner m_closeButtonCorner;
    struct {
        qint32 id = 0;
        bool active = false;
    } m_touch;

    QAction *m_exposeAction;
    QAction *m_exposeAllAction;
    QAction *m_exposeClassAction;
};

} // namespace

#endif
