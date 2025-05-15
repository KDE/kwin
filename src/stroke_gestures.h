/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Thomas Jaeger <ThJaeger@gmail.com>
    SPDX-FileCopyrightText: 2023 Daniel Kondor <kondor.dani@gmail.com>
    SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: MIT
*/

#pragma once

#include <QList>
#include <QMultiMap>
#include <QObject>
#include <QPointF>

#include <memory> // std::unique_ptr

class QAction;

namespace KWin
{

struct stroke_t;

struct stroke_deleter
{
    void operator()(stroke_t *s) const;
};

using Stroke = std::unique_ptr<stroke_t, stroke_deleter>;

// not derived from Gesture because we can't offer started() & cancelled() on a per-stroke basis
class StrokeGesture : public QObject
{
    Q_OBJECT

public:
    explicit StrokeGesture(Qt::KeyboardModifiers modifiers, const QList<QPointF> &points, const QAction *actionInfo = nullptr, QObject *parent = nullptr);
    ~StrokeGesture() override;

    StrokeGesture(const StrokeGesture &) = delete;
    StrokeGesture &operator=(const StrokeGesture &) = delete;

    bool operator==(const StrokeGesture &) const;

    Qt::KeyboardModifiers modifiers() const;
    const QList<QPointF> &points() const;
    double time(int n) const;
    const QAction *actionInfo() const; // not triggered directly by this class, for informational use only

    static Stroke make_stroke(const QList<QPointF> &points);
    static double min_matching_score();
    bool compare(const Stroke &drawn, double &score_out) const;

Q_SIGNALS:
    /**
     * Gesture matching ended and this StrokeGesture matched.
     */
    void triggered();

private:
    QList<QPointF> m_points;
    Qt::KeyboardModifiers m_modifiers;
    Stroke m_stroke;
    const QAction *m_actionInfo;
};

class StrokeGestures : public QObject
{
    Q_OBJECT

public:
    StrokeGestures(QObject *parent = nullptr);
    ~StrokeGestures() override;

    bool isEmpty() const;
    bool isEmpty(Qt::KeyboardModifiers modifiers) const;
    const QList<StrokeGesture *> list() const;
    const QList<StrokeGesture *> list(Qt::KeyboardModifiers modifiers) const;

    void add(StrokeGesture *gesture);
    void remove(StrokeGesture *gesture);

    StrokeGesture *bestMatch(Qt::KeyboardModifiers modifiers, const QList<QPointF> &points, double &bestScore) const;

private:
    QMultiMap<Qt::KeyboardModifiers, StrokeGesture *> m_gestures;
};

} // namespace KWin
