/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPushButton>
#include <QWidget>

class PipResizeHandle : public QWidget
{
    Q_OBJECT

public:
    explicit PipResizeHandle(Qt::Edges edges, QWidget *parent = nullptr);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::Edges m_edges;
    bool m_hovered = false;
};

class Pip : public QWidget
{
    Q_OBJECT

public:
    explicit Pip(QWidget *parent = nullptr);

Q_SIGNALS:
    void pinned();
    void unpinned();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void layout();

    QPushButton *m_closeButton = nullptr;
    PipResizeHandle *m_topLeftResizeHandle = nullptr;
    PipResizeHandle *m_topResizeHandle = nullptr;
    PipResizeHandle *m_topRightResizeHandle = nullptr;
    PipResizeHandle *m_rightResizeHandle = nullptr;
    PipResizeHandle *m_bottomRightResizeHandle = nullptr;
    PipResizeHandle *m_bottomResizeHandle = nullptr;
    PipResizeHandle *m_bottomLeftResizeHandle = nullptr;
    PipResizeHandle *m_leftResizeHandle = nullptr;
};

class PipPin : public QWidget
{
    Q_OBJECT

public:
    explicit PipPin(QWidget *parent = nullptr);

    bool isPinned() const;
    void setPinned(bool pinned);

Q_SIGNALS:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool m_pinned = true;
    bool m_hovered = false;
};

class Media : public QWidget
{
    Q_OBJECT

public:
    explicit Media(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void layout();

    std::unique_ptr<Pip> m_pip;
    PipPin *m_pin = nullptr;
};
