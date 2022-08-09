/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinquickeffect.h>

#include <QKeySequence>

class QAction;

namespace KWin
{

class WindowViewEffect : public QuickSceneEffect
{
    Q_OBJECT
    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(int layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool ignoreMinimized READ ignoreMinimized NOTIFY ignoreMinimizedChanged)
    Q_PROPERTY(PresentWindowsMode mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(qreal partialActivationFactor READ partialActivationFactor NOTIFY partialActivationFactorChanged)
    Q_PROPERTY(bool gestureInProgress READ gestureInProgress NOTIFY gestureInProgressChanged)
    Q_PROPERTY(QString searchText MEMBER m_searchText NOTIFY searchTextChanged)

public:
    enum PresentWindowsMode {
        ModeAllDesktops, // Shows windows of all desktops
        ModeCurrentDesktop, // Shows windows on current desktop
        ModeWindowGroup, // Shows windows selected via property
        ModeWindowClass, // Shows all windows of same class as selected class
        ModeWindowClassCurrentDesktop, // Shows windows of same class on current desktop
    };
    Q_ENUM(PresentWindowsMode)

    enum class Status {
        Inactive,
        Activating,
        Deactivating,
        Active
    };

    WindowViewEffect();
    ~WindowViewEffect() override;

    int animationDuration() const;
    void setAnimationDuration(int duration);

    int layout() const;
    void setLayout(int layout);

    bool ignoreMinimized() const;

    void reconfigure(ReconfigureFlags) override;
    int requestedEffectChainPosition() const override;
    void grabbedKeyboardEvent(QKeyEvent *e) override;
    bool borderActivated(ElectricBorder border) override;

    qreal partialActivationFactor() const;
    void setPartialActivationFactor(qreal factor);

    bool gestureInProgress() const;
    void setGestureInProgress(bool gesture);

    void setMode(PresentWindowsMode mode);
    void toggleMode(PresentWindowsMode mode);
    PresentWindowsMode mode() const;

public Q_SLOTS:
    void activate(const QStringList &windowIds);
    void activate();
    void deactivate(int timeout);

    void partialActivate(qreal factor);
    void cancelPartialActivate();
    void partialDeactivate(qreal factor);
    void cancelPartialDeactivate();

Q_SIGNALS:
    void animationDurationChanged();
    void partialActivationFactorChanged();
    void gestureInProgressChanged();
    void modeChanged();
    void layoutChanged();
    void ignoreMinimizedChanged();
    void searchTextChanged();

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

private:
    void realDeactivate();

    QTimer *m_shutdownTimer;
    QList<QUuid> m_windowIds;

    // User configuration settings
    QAction *m_exposeAction = nullptr;
    QAction *m_exposeAllAction = nullptr;
    QAction *m_exposeClassAction = nullptr;
    QAction *m_exposeClassCurrentDesktopAction = nullptr;
    QAction *m_realtimeToggleAction = nullptr;
    // Shortcut - needed to toggle the effect
    QList<QKeySequence> m_shortcut;
    QList<QKeySequence> m_shortcutAll;
    QList<QKeySequence> m_shortcutClass;
    QList<QKeySequence> m_shortcutClassCurrentDesktop;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_borderActivateAll;
    QList<ElectricBorder> m_borderActivateClass;
    QList<ElectricBorder> m_borderActivateClassCurrentDesktop;
    QList<ElectricBorder> m_touchBorderActivate;
    QList<ElectricBorder> m_touchBorderActivateAll;
    QList<ElectricBorder> m_touchBorderActivateClass;
    QList<ElectricBorder> m_touchBorderActivateClassCurrentDesktop;
    QString m_searchText;
    Status m_status = Status::Inactive;
    qreal m_partialActivationFactor = 0;
    PresentWindowsMode m_mode;
    int m_animationDuration = 400;
    int m_layout = 1;
    bool m_gestureInProgress = false;
};

} // namespace KWin
