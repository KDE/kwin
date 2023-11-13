/*
 SPDX-FileCopyrightText: 2023 David Edmundson <davidedmundson@kde.org>

SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include <QObject>

namespace KWin
{

class SecurityContextManagerV1InterfacePrivate;
class SecurityContextManagerV1Interface;
class Display;

class SecurityContextManagerV1Interface : public QObject
{
    Q_OBJECT
public:
    SecurityContextManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~SecurityContextManagerV1Interface() override;

private:
    std::unique_ptr<SecurityContextManagerV1InterfacePrivate> d;
};
}
