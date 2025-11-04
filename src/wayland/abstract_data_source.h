/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include "clientconnection.h"

struct wl_client;

namespace KWin
{

/**
 * Drag and Drop actions supported by the data source.
 */
enum class DnDAction {
    None = 0,
    Copy = 1 << 0,
    Move = 1 << 1,
    Ask = 1 << 2,
};
Q_DECLARE_FLAGS(DnDActions, DnDAction)

/**
 * @brief The AbstractDataSource class abstracts the data that
 * can be transferred to another client.
 *
 * It loosely maps to DataDeviceInterface
 */

// Anything related to selections are pure virtual, content relating
// to drag and drop has a default implementation

class KWIN_EXPORT AbstractDataSource : public QObject
{
    Q_OBJECT
public:
    virtual bool isAccepted() const
    {
        return false;
    }

    virtual void accept(const QString &mimeType)
    {
    };
    virtual void requestData(const QString &mimeType, qint32 fd) = 0;
    virtual void cancel() = 0;

    virtual QStringList mimeTypes() const = 0;

    /**
     * @returns The Drag and Drop actions supported by this DataSourceInterface.
     */
    virtual DnDActions supportedDragAndDropActions() const
    {
        return {};
    };
    virtual DnDAction selectedDndAction() const
    {
        return DnDAction::None;
    }
    /**
     * The user performed the drop action during a drag and drop operation.
     */
    virtual void dropPerformed()
    {
        m_dndDropped = true;
    }
    /**
     * The drop destination finished interoperating with this data source.
     */
    virtual void dndFinished()
    {
    }
    /**
     * This event indicates the @p action selected by the compositor after matching the
     * source/destination side actions. Only one action (or none) will be offered here.
     */
    virtual void dndAction(DnDAction action)
    {
    }

    bool isDndCancelled() const
    {
        return m_dndCancelled;
    }

    bool isDropPerformed() const
    {
        return m_dndDropped;
    }

    /**
     *  Called when a user stops clicking but it is not accepted by a client.
     */
    virtual void dndCancelled()
    {
        m_dndCancelled = true;
    }

    virtual wl_client *client() const
    {
        return nullptr;
    };

    /**
     * Called from kwin core code, this updates the keyboard modifiers currently pressed
     * which can be used to determine the best DND action
     */
    void setKeyboardModifiers(Qt::KeyboardModifiers heldModifiers);
    Qt::KeyboardModifiers keyboardModifiers() const;

    /**
     * Action forced by the data source. Its primary purpose is to support XDND where supported
     * source actions are unknown until some specific action arrives with an XdndPosition message.
     */
    void setExclusiveAction(DnDAction action);
    std::optional<DnDAction> exclusiveAction() const;

Q_SIGNALS:
    void aboutToBeDestroyed();

    void mimeTypeOffered(const QString &);
    void supportedDragAndDropActionsChanged();
    void keyboardModifiersChanged();
    void dndActionChanged();
    void exclusiveActionChanged();
    void acceptedChanged();

protected:
    explicit AbstractDataSource(QObject *parent = nullptr);

private:
    std::optional<DnDAction> m_exclusiveAction;
    Qt::KeyboardModifiers m_heldModifiers;
    bool m_dndCancelled = false;
    bool m_dndDropped = false;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DnDActions)
