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
    Q_PROPERTY(int layout READ layout NOTIFY layoutChanged)
    Q_PROPERTY(bool ignoreMinimized READ ignoreMinimized NOTIFY ignoreMinimizedChanged)
    Q_PROPERTY(PresentWindowsMode mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString searchText MEMBER m_searchText NOTIFY searchTextChanged)

public:
    enum PresentWindowsMode {
        ModeAllDesktops, // Shows windows of all desktops
        ModeCurrentDesktop, // Shows windows on current desktop
        ModeWindowGroup, // Shows windows selected via property
        ModeWindowClass // Shows all windows of same class as selected class
    };
    Q_ENUM(PresentWindowsMode)

    WindowViewEffect();
    ~WindowViewEffect() override;

    int layout() const;
    void setLayout(int layout);

    bool ignoreMinimized() const;

    void reconfigure(ReconfigureFlags) override;
    int requestedEffectChainPosition() const override;
    void grabbedKeyboardEvent(QKeyEvent *e) override;
    bool borderActivated(ElectricBorder border) override;

    void setMode(PresentWindowsMode mode);
    void toggleMode(PresentWindowsMode mode);
    PresentWindowsMode mode() const;

public Q_SLOTS:
    void activate(const QStringList &windowIds);
    void activated();

Q_SIGNALS:
    void modeChanged();
    void layoutChanged();
    void ignoreMinimizedChanged();
    void searchTextChanged();

protected:
    QVariantMap initialProperties(EffectScreen *screen) override;

private:

    QList<QUuid> m_windowIds;

    // User configuration settings
    QAction *m_exposeAction = nullptr;
    QAction *m_exposeAllAction = nullptr;
    QAction *m_exposeClassAction = nullptr;
    // Shortcut - needed to toggle the effect
    QList<QKeySequence> m_shortcut;
    QList<QKeySequence> m_shortcutAll;
    QList<QKeySequence> m_shortcutClass;
    QList<ElectricBorder> m_borderActivate;
    QList<ElectricBorder> m_borderActivateAll;
    QList<ElectricBorder> m_borderActivateClass;
    QString m_searchText;
    PresentWindowsMode m_mode;
    int m_layout = 1;
};

} // namespace KWin
