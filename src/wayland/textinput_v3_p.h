/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "clientconnection.h"
#include "textinput_v3.h"
#include "wayland/surface.h"

#include <QHash>
#include <QList>
#include <QPointer>

#include <qwayland-server-text-input-unstable-v3.h>

namespace KWin
{

struct TextInputV3State
{
    void resetPending();
    void resetPendingPreedit();

    struct
    {
        Rect cursorRectangle;
        TextInputChangeCause surroundingTextChangeCause = TextInputChangeCause::InputMethod;
        TextInputContentHints contentHints = TextInputContentHint::None;
        TextInputContentPurpose contentPurpose = TextInputContentPurpose::Normal;
        bool enabled = false;
        QString surroundingText;
        qint32 surroundingTextCursorPosition = 0;
        qint32 surroundingTextSelectionAnchor = 0;
        QString preeditText;
        quint32 preeditCursorBegin = 0;
        quint32 preeditCursorEnd = 0;
    } pending;

    Rect cursorRectangle;
    TextInputContentHints contentHints = TextInputContentHint::None;
    TextInputContentPurpose contentPurpose = TextInputContentPurpose::Normal;

    QString surroundingText;
    qint32 surroundingTextCursorPosition = 0;
    qint32 surroundingTextSelectionAnchor = 0;
    TextInputChangeCause surroundingTextChangeCause = TextInputChangeCause::InputMethod;

    QString preeditText;
    quint32 preeditCursorBegin = 0;
    quint32 preeditCursorEnd = 0;
    quint32 serial = 0;
    bool enabled = false;
};

class TextInputManagerV3InterfacePrivate : public QtWaylandServer::zwp_text_input_manager_v3
{
public:
    TextInputManagerV3InterfacePrivate(TextInputManagerV3Interface *_q, Display *display);

    TextInputManagerV3Interface *q;

protected:
    void zwp_text_input_manager_v3_destroy(Resource *resource) override;
    void zwp_text_input_manager_v3_get_text_input(Resource *resource, uint32_t id, wl_resource *seat) override;
};

class TextInputV3InterfacePrivate : public QtWaylandServer::zwp_text_input_v3
{
public:
    TextInputV3InterfacePrivate(SeatInterface *seat, TextInputV3Interface *_q);

    // events
    void sendEnter(SurfaceInterface *surface);
    void sendLeave(SurfaceInterface *surface);
    void sendPreEdit(const QString &text, const quint32 cursorBegin, const quint32 cursorEnd);
    void commitString(const QString &text);
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);
    void done();

    void updateEnabled();

    QList<TextInputV3InterfacePrivate::Resource *> textInputsForClient(ClientConnection *client) const;
    QList<TextInputV3InterfacePrivate::Resource *> enabledTextInputsForClient(ClientConnection *client) const;

    TextInputV3State *stateForResource(Resource *resource)
    {
        auto it = stateMap.find(resource);
        if (it == stateMap.end()) {
            return nullptr;
        }
        return &it.value();
    }

    const TextInputV3State *stateForResource(Resource *resource) const
    {
        auto it = stateMap.find(resource);
        if (it == stateMap.end()) {
            return nullptr;
        }
        return &it.value();
    }

    TextInputV3State *stateForFocusedSurface()
    {
        if (!surface) {
            return nullptr;
        }
        auto resources = textInputsForClient(surface->client());
        for (auto resource : resources) {
            auto state = stateForResource(resource);
            if (state && state->enabled) {
                return state;
            }
        }
        return nullptr;
    }

    static TextInputV3InterfacePrivate *get(TextInputV3Interface *inputInterface)
    {
        return inputInterface->d.get();
    }

    SeatInterface *seat = nullptr;
    QPointer<SurfaceInterface> surface;

    QHash<Resource *, TextInputV3State> stateMap;

    TextInputV3Interface *q;
    bool isEnabled = false;

protected:
    void zwp_text_input_v3_bind_resource(Resource *resource) override;
    void zwp_text_input_v3_destroy_resource(Resource *resource) override;
    void zwp_text_input_v3_destroy(Resource *resource) override;
    void zwp_text_input_v3_enable(Resource *resource) override;
    void zwp_text_input_v3_disable(Resource *resource) override;
    void zwp_text_input_v3_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor) override;
    void zwp_text_input_v3_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose) override;
    void zwp_text_input_v3_set_text_change_cause(Resource *resource, uint32_t cause) override;
    void zwp_text_input_v3_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void zwp_text_input_v3_commit(Resource *resource) override;
};

}
