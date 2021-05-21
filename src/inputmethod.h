/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_VIRTUAL_KEYBOARD_H
#define KWIN_VIRTUAL_KEYBOARD_H

#include <QObject>

#include <kwinglobals.h>
#include <kwin_export.h>

#include <QPointer>
#include <QTimer>
#include <KWaylandServer/textinput_v2_interface.h>

class KStatusNotifierItem;
class QProcess;

namespace KWin
{

class AbstractClient;

/**
 * This class implements the zwp_input_method_unstable_v1, which is currently used to provide
 * the Virtual Keyboard using supported input method client (maliit-keyboard e.g.)
 **/
class KWIN_EXPORT InputMethod : public QObject
{
    Q_OBJECT
public:
    ~InputMethod() override;

    void init();
    void setEnabled(bool enable);
    bool isEnabled() const {
        return m_enabled;
    }
    bool isActive() const;
    void setActive(bool active);
    void hide();
    void show();

    void setInputMethodCommand(const QString &path);

Q_SIGNALS:
    void activeChanged(bool active);
    void enabledChanged(bool enabled);

private Q_SLOTS:
    void clientAdded(AbstractClient* client);

    // textinput interface slots
    void handleFocusedSurfaceChanged();
    void surroundingTextChanged();
    void contentTypeChanged();
    void textInputInterfaceV2EnabledChanged();
    void textInputInterfaceV3EnabledChanged();
    void stateCommitted(uint32_t serial);
    void textInputInterfaceV2StateUpdated(quint32 serial, KWaylandServer::TextInputV2Interface::UpdateReason reason);

    // inputcontext slots
    void setPreeditString(uint32_t serial, const QString &text, const QString &commit);
    void setPreeditCursor(qint32 index);

private:
    void updateSni();
    void updateInputPanelState();
    void adoptInputMethodContext();
    void commitString(qint32 serial, const QString &text);
    void keysymReceived(quint32 serial, quint32 time, quint32 sym, bool pressed, Qt::KeyboardModifiers modifiers);
    void deleteSurroundingText(int32_t index, uint32_t length);
    void setCursorPosition(qint32 index, qint32 anchor);
    void setLanguage(uint32_t serial, const QString &language);
    void setTextDirection(uint32_t serial, Qt::LayoutDirection direction);
    void startInputMethod();
    void stopInputMethod();
    void setTrackedClient(AbstractClient *trackedClient);

    struct {
        QString text = QString();
        quint32 begin = 0;
        quint32 end = 0;
    } preedit;

    bool m_enabled = false;
    quint32 m_serial = 0;
    KStatusNotifierItem *m_sni = nullptr;
    QPointer<AbstractClient> m_inputClient;
    QPointer<AbstractClient> m_trackedClient;

    QProcess *m_inputMethodProcess = nullptr;
    QTimer m_inputMethodCrashTimer;
    uint m_inputMethodCrashes = 0;
    QString m_inputMethodCommand;

    KWIN_SINGLETON(InputMethod)
};

}

#endif
