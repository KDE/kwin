/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>

#include <memory>

struct wl_resource;

namespace KWin
{

class Display;
class XdgDialogV1Interface;
class XdgDialogV1InterfacePrivate;
class XdgDialogWmV1InterfacePrivate;
class XdgToplevelInterface;

class XdgDialogWmV1Interface : public QObject
{
    Q_OBJECT
public:
    XdgDialogWmV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgDialogWmV1Interface() override;

    XdgDialogV1Interface *dialogForToplevel(XdgToplevelInterface *toplevel) const;

Q_SIGNALS:
    void dialogCreated(XdgDialogV1Interface *dialog);

private:
    std::unique_ptr<XdgDialogWmV1InterfacePrivate> d;
};

class XdgDialogV1Interface : public QObject
{
    Q_OBJECT
public:
    ~XdgDialogV1Interface() override;

    bool isModal() const;
    XdgToplevelInterface *toplevel() const;

Q_SIGNALS:
    void modalChanged(bool modal);

private:
    XdgDialogV1Interface(wl_resource *resource, XdgToplevelInterface *toplevel);
    friend class XdgDialogWmV1InterfacePrivate;
    std::unique_ptr<XdgDialogV1InterfacePrivate> d;
};

}
