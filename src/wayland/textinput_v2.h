/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>
#include <memory>

#include "textinput.h"

struct wl_resource;
namespace KWaylandServer
{
class ClientConnection;
class Display;
class SeatInterface;
class SurfaceInterface;
class TextInputV2Interface;
class TextInputV2InterfacePrivate;
class TextInputManagerV2InterfacePrivate;

/**
 * @brief Represent the Global for the interface.
 *
 * The class can represent different interfaces. Which concrete interface is represented
 * can be determined through {@link interfaceVersion}.
 *
 * To create a TextInputManagerV2Interface use {@link Display::createTextInputManager}
 */
class KWIN_EXPORT TextInputManagerV2Interface : public QObject
{
    Q_OBJECT
public:
    explicit TextInputManagerV2Interface(Display *display, QObject *parent = nullptr);
    ~TextInputManagerV2Interface() override;

private:
    std::unique_ptr<TextInputManagerV2InterfacePrivate> d;
};

/**
 * @brief Represents a generic Resource for a text input object.
 *
 * This class does not directly correspond to a Wayland resource, but is a generic contract
 * for any interface which implements a text input, e.g. the unstable wl_text_input interface.
 *
 * It does not expose the actual interface to cover up the fact that the interface is unstable
 * and might change. If one needs to know the actual used protocol, use the method {@link interfaceVersion}.
 *
 * A TextInputV2Interface gets created by the {@link TextInputManagerV2Interface}. The individual
 * instances are not exposed directly. The SeatInterface provides access to the currently active
 * TextInputV2Interface. This is evaluated automatically based on which SurfaceInterface has
 * keyboard focus.
 *
 * @see TextInputManagerV2Interface
 * @see SeatInterface
 */
class KWIN_EXPORT TextInputV2Interface : public QObject
{
    Q_OBJECT
public:
    ~TextInputV2Interface() override;

    enum class UpdateReason : uint32_t {
        StateChange = 0, // updated state because it changed
        StateFull = 1, // full state after enter or input_method_changed event
        StateReset = 2, // full state after reset
        StateEnter = 3, // full state after switching focus to a different widget on client side
    };
    Q_ENUM(UpdateReason)
    /**
     * The preferred language as a RFC-3066 format language tag.
     *
     * This can be used by the server to show a language specific virtual keyboard layout.
     * @see preferredLanguageChanged
     */
    QString preferredLanguage() const;

    /**
     * @see cursorRectangleChanged
     */
    QRect cursorRectangle() const;

    /**
     * @see contentTypeChanged
     */
    TextInputContentPurpose contentPurpose() const;

    /**
     *@see contentTypeChanged
     */
    TextInputContentHints contentHints() const;

    /**
     * @returns The plain surrounding text around the input position.
     * @see surroundingTextChanged
     * @see surroundingTextCursorPosition
     * @see surroundingTextSelectionAnchor
     */
    QString surroundingText() const;
    /**
     * @returns The byte offset of current cursor position within the {@link surroundingText}
     * @see surroundingText
     * @see surroundingTextChanged
     */
    qint32 surroundingTextCursorPosition() const;
    /**
     * The byte offset of the selection anchor within the {@link surroundingText}.
     *
     * If there is no selected text this is the same as cursor.
     * @return The byte offset of the selection anchor
     * @see surroundingText
     * @see surroundingTextChanged
     */
    qint32 surroundingTextSelectionAnchor() const;

    /**
     * @return The surface the TextInputV2Interface is enabled on
     * @see isEnabled
     * @see enabledChanged
     */
    QPointer<SurfaceInterface> surface() const;

    /**
     * @return whether @p client supports text-input-v2
     */
    bool clientSupportsTextInput(ClientConnection *client) const;

    /**
     * @return Whether the TextInputV2Interface is currently enabled for a SurfaceInterface.
     * @see surface
     * @see enabledChanged
     */
    bool isEnabled() const;

    /**
     * Notify when a new composing @p text (pre-edit) should be set around the
     * current cursor position. Any previously set composing text should
     * be removed.
     *
     * The @p commitText can be used to replace the preedit text on reset
     * (for example on unfocus).
     *
     * @param text The new utf8-encoded pre-edit text
     * @param commitText Utf8-encoded text to replace preedit text on reset
     * @see commit
     * @see preEditCursor
     */
    void preEdit(const QString &text, const QString &commitText);

    /**
     * Notify when @p text should be inserted into the editor widget.
     * The text to commit could be either just a single character after a key press or the
     * result of some composing ({@link preEdit}). It could be also an empty text
     * when some text should be removed (see {@link deleteSurroundingText}) or when
     * the input cursor should be moved (see {@link cursorPosition}).
     *
     * Any previously set composing text should be removed.
     * @param text The utf8-encoded text to be inserted into the editor widget
     * @see preEdit
     * @see deleteSurroundingText
     */
    void commitString(const QString &text);

    /**
     * Sets the cursor position inside the composing text (as byte offset) relative to the
     * start of the composing text. When @p index is a negative number no cursor is shown.
     *
     * The Client applies the @p index together with {@link preEdit}.
     * @param index The cursor position relative to the start of the composing text
     * @see preEdit
     */
    void setPreEditCursor(qint32 index);

    /**
     * Sets the style for a range of text for the composing text (as byte offset).
     *
     * The Client applies the @p index together with {@link preEdit}.
     * @param index The position relative to the start of the composing text
     * @param length The length of the style to apply on
     * @param style style flag
     * @see preEdit
     */
    void preEditStyling(uint32_t index, uint32_t length, uint32_t style);

    /**
     * Notify when the text around the current cursor position should be deleted.
     *
     * The Client processes this event together with the commit string
     *
     * @param beforeLength length of text before current cursor position.
     * @param afterLength length of text after current cursor position.
     * @see commit
     */
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);

    /**
     * Notify when the cursor @p index or @p anchor position should be modified.
     *
     * The Client applies this together with the commit string.
     */
    void setCursorPosition(qint32 index, qint32 anchor);

    /**
     * Sets the text @p direction of input text.
     */
    void setTextDirection(Qt::LayoutDirection direction);

    void keysymPressed(quint32 keysym, quint32 modifiers = 0);
    void keysymReleased(quint32 keysym, quint32 modifiers = 0);

    /**
     * Informs the client about changes in the visibility of the input panel (virtual keyboard).
     *
     * The @p overlappedSurfaceArea defines the area overlapped by the input panel (virtual keyboard)
     * on the SurfaceInterface having the text focus in surface local coordinates.
     *
     * @param visible Whether the input panel is currently visible
     * @param overlappedSurfaceArea The overlapping area in surface local coordinates
     */
    void setInputPanelState(bool visible, const QRect &overlappedSurfaceArea);

    /**
     * Sets the language of the input text. The @p languageTag is a RFC-3066 format language tag.
     */
    void setLanguage(const QString &languageTag);

    /**
     * Sets the modifiers map to use when modifiers are included in a key event.
     */
    void setModifiersMap(const QByteArray &modifiersMap);

Q_SIGNALS:
    /**
     * Requests input panels (virtual keyboard) to show.
     * @see requestHideInputPanel
     */
    void requestShowInputPanel();
    /**
     * Requests input panels (virtual keyboard) to hide.
     * @see requestShowInputPanel
     */
    void requestHideInputPanel();
    /**
     * Emitted whenever the preferred @p language changes.
     * @see preferredLanguage
     */
    void preferredLanguageChanged(const QString &language);
    /**
     * @see cursorRectangle
     */
    void cursorRectangleChanged(const QRect &rect);
    /**
     * Emitted when the {@link contentPurpose} and/or {@link contentHints} changes.
     * @see contentPurpose
     * @see contentHints
     */
    void contentTypeChanged();
    /**
     * Emitted when the {@link surroundingText}, {@link surroundingTextCursorPosition}
     * and/or {@link surroundingTextSelectionAnchor} changed.
     * @see surroundingText
     * @see surroundingTextCursorPosition
     * @see surroundingTextSelectionAnchor
     */
    void surroundingTextChanged();
    /**
     * Emitted whenever this TextInputV2Interface gets enabled or disabled for a SurfaceInterface.
     * @see isEnabled
     * @see surface
     */
    void enabledChanged();
    /**
     * Emitted whenever TextInputInterface should update the current state.
     */
    void stateUpdated(uint32_t serial, UpdateReason reason);

private:
    friend class TextInputManagerV2InterfacePrivate;
    friend class SeatInterface;
    friend class SeatInterfacePrivate;
    friend class TextInputV2InterfacePrivate;
    explicit TextInputV2Interface(SeatInterface *seat);

    std::unique_ptr<TextInputV2InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::TextInputV2Interface *)
Q_DECLARE_METATYPE(KWaylandServer::TextInputV2Interface::UpdateReason)
