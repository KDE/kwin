/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/offscreenquickview.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QQueue>

namespace KWin
{

class RenderView;

class PaintDurationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit PaintDurationModel(ssize_t count);

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void push(std::chrono::nanoseconds value);
    int value() const;

    void resize(ssize_t size);

private:
    ssize_t m_maxCount;
    QQueue<int> m_values;
};

class ShowFpsScreen : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(int maximumFps READ maximumFps NOTIFY maximumFpsChanged)
    Q_PROPERTY(PaintDurationModel *paintDuration READ paintDuration NOTIFY paintChanged)
    Q_PROPERTY(PaintDurationModel *paintDurationCPU READ paintDurationCPU NOTIFY paintChanged)
    Q_PROPERTY(int paintAmount READ paintAmount NOTIFY paintChanged)
    Q_PROPERTY(QColor paintColor READ paintColor NOTIFY paintChanged)
    Q_PROPERTY(QString presentationMode READ presentationMode NOTIFY presentationModeChanged)

public:
    explicit ShowFpsScreen(uint32_t maximumFps);

    int fps() const;
    int maximumFps() const;
    PaintDurationModel *paintDuration();
    PaintDurationModel *paintDurationCPU();
    int paintAmount() const;
    QColor paintColor() const;
    QString presentationMode() const;

    void setMaximumFps(uint32_t fps);

Q_SIGNALS:
    void fpsChanged();
    void maximumFpsChanged();
    void paintChanged();
    void presentationModeChanged();

public:
    void presented(OutputFrame *frame, std::chrono::nanoseconds timestamp, PresentationMode mode);

    std::unique_ptr<OffscreenQuickScene> m_scene;
    int m_fps = 0;
    int m_newFps = 0;
    uint32_t m_maximumFps = 0;
    std::chrono::steady_clock::time_point m_lastFpsTime;
    PaintDurationModel m_paintDuration;
    PaintDurationModel m_paintDurationCPU;
    int m_paintAmount = 0;
    QString m_presentationMode = QStringLiteral("VSync");
};

class ShowFpsEffect : public Effect
{
    Q_OBJECT

public:
    ShowFpsEffect();
    ~ShowFpsEffect() override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    bool paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;

    static bool supported();

private:
    void removeView(RenderView *view);

    std::unordered_map<RenderView *, std::unique_ptr<ShowFpsScreen>> m_data;
    RenderView *m_currentView = nullptr;
};

} // namespace KWin
