/*
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>
#include <memory>

namespace KWin
{

class Output;
class Display;
class OutputOrderV1InterfacePrivate;

class OutputOrderV1Interface : public QObject
{
    Q_OBJECT
public:
    explicit OutputOrderV1Interface(Display *display, QObject *parent);
    ~OutputOrderV1Interface() override;

    void setOutputOrder(const QList<Output *> &outputOrder);

private:
    std::unique_ptr<OutputOrderV1InterfacePrivate> d;
};

}
