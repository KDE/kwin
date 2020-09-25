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

#include <abstract_client.h>

class KStatusNotifierItem;

namespace KWin
{

class KWIN_EXPORT VirtualKeyboard : public QObject
{
    Q_OBJECT
public:
    ~VirtualKeyboard() override;

    void init();
    void hide();
    void show();

Q_SIGNALS:
    void enabledChanged(bool enabled);

private Q_SLOTS:
    void clientAdded(AbstractClient* client);

    // textinput interface slots
    void handleFocusedSurfaceChanged();
    void surroundingTextChanged();
    void contentTypeChanged();
    void requestReset();
    void textInputInterfaceV2EnabledChanged();
    void textInputInterfaceV3EnabledChanged();
    void stateCommitted(uint32_t serial);

    // inputcontext slots
    void setPreeditString(uint32_t serial, const QString &text, const QString &commit);
    void setPreeditCursor(qint32 index);

private:
    void setEnabled(bool enable);
    void updateSni();
    void updateInputPanelState();
    void adoptInputMethodContext();

    struct {
        QString text = QString();
        quint32 begin = 0;
        quint32 end = 0;
    } preedit;

    bool m_enabled = false;
    KStatusNotifierItem *m_sni = nullptr;
    QPointer<AbstractClient> m_inputClient;
    QPointer<AbstractClient> m_trackedClient;

    KWIN_SINGLETON(VirtualKeyboard)
};

}

#endif
