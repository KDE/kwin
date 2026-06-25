/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/backendoutput.h"

namespace KWin
{

class PlaceholderOutput : public BackendOutput
{
    Q_OBJECT

public:
    PlaceholderOutput(const QSize &size, qreal scale = 1);
    ~PlaceholderOutput() override;

    std::expected<void, OutputError> testPresentation(const std::shared_ptr<OutputFrame> &frame) override;
    std::expected<void, OutputError> present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame) override;
};

} // namespace KWin
