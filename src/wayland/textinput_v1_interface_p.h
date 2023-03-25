/*
    SPDX-FileCopyrightText: 2022 Xuetian Weng <wengxt@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "clientconnection.h"
#include "wayland/seat_interface.h"
#include "wayland/seat_interface_p.h"
#include "wayland/surface_interface.h"
#include "wayland/textinput_v1_interface.h"

#include <QPointer>
#include <QRect>
#include <QSet>
#include <QVector>

#include <qwayland-server-text-input-unstable-v1.h>

namespace KWaylandServer
{
class TextInputManagerV1InterfacePrivate : public QtWaylandServer::zwp_text_input_manager_v1
{
public:
    TextInputManagerV1InterfacePrivate(TextInputManagerV1Interface *_q, Display *display);

    TextInputManagerV1Interface *q;
    Display *display;
    std::unique_ptr<TextInputV1Interface> textInputV1;

protected:
    void zwp_text_input_manager_v1_create_text_input(Resource *resource, uint32_t id) override;
};

class TextInputV1InterfacePrivate : public QtWaylandServer::zwp_text_input_v1
{
public:
    TextInputV1InterfacePrivate(SeatInterface *seat, TextInputV1Interface *_q);

    void sendEnter(SurfaceInterface *surface);
    void sendLeave(SurfaceInterface *surface);
    void preEdit(const QString &text, const QString &commit);
    void preEditStyling(quint32 index, uint32_t length, uint32_t style);
    void commitString(const QString &text);
    void deleteSurroundingText(qint32 index, quint32 length);
    void setTextDirection(Qt::LayoutDirection direction);
    void setPreEditCursor(qint32 index);
    void setCursorPosition(qint32 index, qint32 anchor);
    void keysymPressed(quint32 time, quint32 keysym, quint32 modifiers);
    void keysymReleased(quint32 time, quint32 keysym, quint32 modifiers);
    void sendInputPanelState();
    void sendLanguage();
    void sendModifiersMap();

    template<typename T>
    void forActivatedResource(const T &callback)
    {
        if (auto resource = activated.value(surface)) {
            callback(resource);
        }
    }

    QList<Resource *> textInputsForClient(ClientConnection *client) const;
    static TextInputV1InterfacePrivate *get(TextInputV1Interface *inputInterface)
    {
        return inputInterface->d.get();
    }

    QString preferredLanguage;
    QRect cursorRectangle;
    TextInputContentHints contentHints = TextInputContentHint::None;
    TextInputContentPurpose contentPurpose = TextInputContentPurpose::Normal;
    QPointer<SeatInterface> seat;
    QPointer<SurfaceInterface> surface;
    QString surroundingText;
    qint32 surroundingTextCursorPosition = 0;
    qint32 surroundingTextSelectionAnchor = 0;
    bool inputPanelVisible = false;
    QRect overlappedSurfaceArea;
    QString language;
    QByteArray modifiersMap;
    TextInputV1Interface *q;
    // Based on weston reference implementation, one surface can have only one activated text input.
    QHash<SurfaceInterface *, Resource *> activated;
    QHash<Resource *, quint32> serialHash;

protected:
    void zwp_text_input_v1_bind_resource(Resource *resource) override;
    void zwp_text_input_v1_destroy_resource(Resource *resource) override;
    void zwp_text_input_v1_activate(Resource *resource, struct wl_resource *seat, struct wl_resource *surface) override;
    void zwp_text_input_v1_deactivate(Resource *resource, wl_resource *seat) override;
    void zwp_text_input_v1_show_input_panel(Resource *resource) override;
    void zwp_text_input_v1_hide_input_panel(Resource *resource) override;
    void zwp_text_input_v1_reset(Resource *resource) override;
    void zwp_text_input_v1_set_surrounding_text(Resource *resource, const QString &text, uint32_t cursor, uint32_t anchor) override;
    void zwp_text_input_v1_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose) override;
    void zwp_text_input_v1_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void zwp_text_input_v1_set_preferred_language(Resource *resource, const QString &language) override;
    void zwp_text_input_v1_commit_state(Resource *resource, uint32_t serial) override;
    void zwp_text_input_v1_invoke_action(Resource *resource, uint32_t button, uint32_t index) override;
};

}
