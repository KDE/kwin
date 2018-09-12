/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/
#ifndef KWAYLAND_SERVER_TEXTINPUT_INTERFACE_H
#define KWAYLAND_SERVER_TEXTINPUT_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class Display;
class SeatInterface;
class SurfaceInterface;
class TextInputInterface;

/**
 * Enum describing the different InterfaceVersion encapsulated in this implementation
 *
 * @since 5.23
 **/
enum class TextInputInterfaceVersion {
    /**
     * wl_text_input as the non-standardized version
     **/
    UnstableV0,
    /**
     * not supported version
     **/
    UnstableV1,
    /**
     * zwp_text_input_v2 as used by Qt 5.7
     **/
    UnstableV2
};

/**
 * @brief Represent the Global for the interface.
 *
 * The class can represent different interfaces. Which concrete interface is represented
 * can be determined through {@link interfaceVersion}.
 *
 * To create a TextInputManagerInterface use {@link Display::createTextInputManager}
 *
 * @since 5.23
 **/
class KWAYLANDSERVER_EXPORT TextInputManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~TextInputManagerInterface();

    /**
     * @returns The interface version used by this TextInputManagerInterface
     **/
    TextInputInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit TextInputManagerInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
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
 * A TextInputInterface gets created by the {@link TextInputManagerInterface}. The individual
 * instances are not exposed directly. The SeatInterface provides access to the currently active
 * TextInputInterface. This is evaluated automatically based on which SurfaceInterface has
 * keyboard focus.
 *
 * @see TextInputManagerInterface
 * @see SeatInterface
 * @since 5.23
 **/
class KWAYLANDSERVER_EXPORT TextInputInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~TextInputInterface();

    /**
     * ContentHint allows to modify the behavior of the text input.
     **/
    enum class ContentHint : uint32_t {
        /**
         * no special behaviour
         */
        None = 0,
        /**
         * suggest word completions
         */
        AutoCompletion = 1 << 0,
        /**
         * suggest word corrections
         */
        AutoCorrection = 1 << 1,
        /**
         * switch to uppercase letters at the start of a sentence
         */
        AutoCapitalization = 1 << 2,
        /**
         * prefer lowercase letters
         */
        LowerCase = 1 << 3,
        /**
         * prefer uppercase letters
         */
        UpperCase = 1 << 4,
        /**
         * prefer casing for titles and headings (can be language dependent)
         */
        TitleCase = 1 << 5,
        /**
         * characters should be hidden
         */
        HiddenText = 1 << 6,
        /**
         * typed text should not be stored
         */
        SensitiveData = 1 << 7,
        /**
         * just latin characters should be entered
         */
        Latin = 1 << 8,
        /**
         * the text input is multi line
         */
        MultiLine = 1 << 9
    };
    Q_DECLARE_FLAGS(ContentHints, ContentHint)

    /**
     * The ContentPurpose allows to specify the primary purpose of a text input.
     *
     * This allows an input method to show special purpose input panels with
     * extra characters or to disallow some characters.
     */
    enum class ContentPurpose : uint32_t {
        /**
         * default input, allowing all characters
         */
        Normal,
        /**
         * allow only alphabetic characters
         **/
        Alpha,
        /**
         * allow only digits
         */
        Digits,
        /**
         * input a number (including decimal separator and sign)
         */
        Number,
        /**
         * input a phone number
         */
        Phone,
        /**
         * input an URL
         */
        Url,
        /**
         * input an email address
         **/
        Email,
        /**
         * input a name of a person
         */
        Name,
        /**
         * input a password
         */
        Password,
        /**
         * input a date
         */
        Date,
        /**
         * input a time
         */
        Time,
        /**
         * input a date and time
         */
        DateTime,
        /**
         * input for a terminal
         */
        Terminal
    };

    /**
     * @returns The interface version used by this TextInputInterface
     **/
    TextInputInterfaceVersion interfaceVersion() const;

    /**
     * The preferred language as a RFC-3066 format language tag.
     *
     * This can be used by the server to show a language specific virtual keyboard layout.
     * @see preferredLanguageChanged
     **/
    QByteArray preferredLanguage() const;

    /**
     * @see cursorRectangleChanged
     **/
    QRect cursorRectangle() const;

    /**
     * @see contentTypeChanged
     **/
    ContentPurpose contentPurpose() const;

    /**
     *@see contentTypeChanged
     **/
    ContentHints contentHints() const;

    /**
     * @returns The plain surrounding text around the input position.
     * @see surroundingTextChanged
     * @see surroundingTextCursorPosition
     * @see surroundingTextSelectionAnchor
     **/
    QByteArray surroundingText() const;
    /**
     * @returns The byte offset of current cursor position within the {@link surroundingText}
     * @see surroundingText
     * @see surroundingTextChanged
     **/
    qint32 surroundingTextCursorPosition() const;
    /**
     * The byte offset of the selection anchor within the {@link surroundingText}.
     *
     * If there is no selected text this is the same as cursor.
     * @return The byte offset of the selection anchor
     * @see surroundingText
     * @see surroundingTextChanged
     **/
    qint32 surroundingTextSelectionAnchor() const;

    /**
     * @return The surface the TextInputInterface is enabled on
     * @see isEnabled
     * @see enabledChanged
     **/
    QPointer<SurfaceInterface> surface() const;

    /**
     * @return Whether the TextInputInterface is currently enabled for a SurfaceInterface.
     * @see surface
     * @see enabledChanged
     **/
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
     **/
    void preEdit(const QByteArray &text, const QByteArray &commitText);

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
     **/
    void commit(const QByteArray &text);

    /**
     * Sets the cursor position inside the composing text (as byte offset) relative to the
     * start of the composing text. When @p index is a negative number no cursor is shown.
     *
     * The Client applies the @p index together with {@link preEdit}.
     * @param index The cursor position relative to the start of the composing text
     * @see preEdit
     **/
    void setPreEditCursor(qint32 index);

    /**
     * Notify when the text around the current cursor position should be deleted.
     *
     * The Client processes this event together with the commit string
     *
     * @param beforeLength length of text before current cursor position.
     * @param afterLength length of text after current cursor position.
     * @see commit
     **/
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);

    /**
     * Notify when the cursor @p index or @p anchor position should be modified.
     *
     * The Client applies this together with the commit string.
     **/
    void setCursorPosition(qint32 index, qint32 anchor);

    /**
     * Sets the text @p direction of input text.
     **/
    void setTextDirection(Qt::LayoutDirection direction);

    void keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    /**
     * Informs the client about changes in the visibility of the input panel (virtual keyboard).
     *
     * The @p overlappedSurfaceArea defines the area overlapped by the input panel (virtual keyboard)
     * on the SurfaceInterface having the text focus in surface local coordinates.
     *
     * @param visible Whether the input panel is currently visible
     * @param overlappedSurfaceArea The overlapping area in surface local coordinates
     **/
    void setInputPanelState(bool visible, const QRect &overlappedSurfaceArea);

    /**
     * Sets the language of the input text. The @p languageTag is a RFC-3066 format language tag.
     **/
    void setLanguage(const QByteArray &languageTag);

Q_SIGNALS:
    /**
     * Requests input panels (virtual keyboard) to show.
     * @see requestHideInputPanel
     **/
    void requestShowInputPanel();
    /**
     * Requests input panels (virtual keyboard) to hide.
     * @see requestShowInputPanel
     **/
    void requestHideInputPanel();
    /**
     * Invoked by the client when the input state should be
     * reset, for example after the text was changed outside of the normal
     * input method flow.
     **/
    void requestReset();
    /**
     * Emitted whenever the preferred @p language changes.
     * @see preferredLanguage
     **/
    void preferredLanguageChanged(const QByteArray &language);
    /**
     * @see cursorRectangle
     **/
    void cursorRectangleChanged(const QRect &rect);
    /**
     * Emitted when the {@link contentPurpose} and/or {@link contentHints} changes.
     * @see contentPurpose
     * @see contentHints
     **/
    void contentTypeChanged();
    /**
     * Emitted when the {@link surroundingText}, {@link surroundingTextCursorPosition}
     * and/or {@link surroundingTextSelectionAnchor} changed.
     * @see surroundingText
     * @see surroundingTextCursorPosition
     * @see surroundingTextSelectionAnchor
     **/
    void surroundingTextChanged();
    /**
     * Emitted whenever this TextInputInterface gets enabled or disabled for a SurfaceInterface.
     * @see isEnabled
     * @see surface
     **/
    void enabledChanged();

protected:
    class Private;
    explicit TextInputInterface(Private *p, QObject *parent = nullptr);

private:
    friend class TextInputManagerUnstableV0Interface;
    friend class TextInputManagerUnstableV2Interface;
    friend class SeatInterface;

    Private *d_func() const;
};


}
}

Q_DECLARE_METATYPE(KWayland::Server::TextInputInterfaceVersion)
Q_DECLARE_METATYPE(KWayland::Server::TextInputInterface *)
Q_DECLARE_METATYPE(KWayland::Server::TextInputInterface::ContentHint)
Q_DECLARE_METATYPE(KWayland::Server::TextInputInterface::ContentHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWayland::Server::TextInputInterface::ContentHints)
Q_DECLARE_METATYPE(KWayland::Server::TextInputInterface::ContentPurpose)

#endif
