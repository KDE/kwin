/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_GLOBALSHORTCUTS_H
#define KWIN_GLOBALSHORTCUTS_H
// KDE
#include <KSharedConfig>
// Qt
#include <QKeySequence>

class QAction;

namespace KWin
{

class GlobalShortcut;

/**
 * @brief Manager for the global shortcut system inside KWin.
 *
 * This class is responsible for holding all the global shortcuts and to process a key press event.
 * That is trigger a shortcut if there is a match.
 *
 * For internal shortcut handling (those which are delivered inside KWin) QActions are used and
 * triggered if the shortcut matches. For external shortcut handling a DBus interface is used.
 **/
class GlobalShortcutsManager : public QObject
{
    Q_OBJECT
public:
    explicit GlobalShortcutsManager(QObject *parent = nullptr);
    virtual ~GlobalShortcutsManager();
    /**
     * @brief Registers an internal global shortcut
     *
     * @param action The action to trigger if the shortcut is pressed
     * @param shortcut The key sequence which triggers this shortcut
     */
    void registerShortcut(QAction *action, const QKeySequence &shortcut);

    /**
     * @brief Processes a key event to decide whether a shortcut needs to be triggered.
     *
     * If a shortcut triggered this method returns @c true to indicate to the caller that the event
     * should not be further processed. If there is no shortcut which triggered for the key, then
     * @c false is returned.
     *
     * @param modifiers The current hold modifiers
     * @param key The keysymbol which has been pressed
     * @return @c true if a shortcut triggered, @c false otherwise
     */
    bool processKey(Qt::KeyboardModifiers modifiers, uint32_t key);
private:
    void objectDeleted(QObject *object);
    QKeySequence getShortcutForAction(const QString &componentName, const QString &actionName, const QKeySequence &defaultShortcut);
    QHash<Qt::KeyboardModifiers, QHash<uint32_t, GlobalShortcut*> > m_shortcuts;
    KSharedConfigPtr m_config;
};

class GlobalShortcut
{
public:
    virtual ~GlobalShortcut();

    const QKeySequence &shortcut() const;
    virtual void invoke() = 0;

protected:
    GlobalShortcut(const QKeySequence &shortcut);

private:
    QKeySequence m_shortcut;
};

class InternalGlobalShortcut : public GlobalShortcut
{
public:
    InternalGlobalShortcut(const QKeySequence &shortcut, QAction *action);
    virtual ~InternalGlobalShortcut();

    void invoke() override;

    QAction *action() const;
private:
    QAction *m_action;
};

inline
QAction *InternalGlobalShortcut::action() const
{
    return m_action;
}

inline
const QKeySequence &GlobalShortcut::shortcut() const
{
    return m_shortcut;
}

} // namespace

#endif
