/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QQuickItem>
#include <QRect>
#include <QTimer>

#include <optional>

class ExpoCell;

class ExpoLayout : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(LayoutMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool fillGaps READ fillGaps WRITE setFillGaps NOTIFY fillGapsChanged)

public:
    enum LayoutMode : uint {
        LayoutClosest = 0,
        LayoutKompose = 1,
        LayoutNatural = 2,
    };
    Q_ENUM(LayoutMode)

    explicit ExpoLayout(QQuickItem *parent = nullptr);

    LayoutMode mode() const;
    void setMode(LayoutMode mode);

    bool fillGaps() const;
    void setFillGaps(bool fill);

    void addCell(ExpoCell *cell);
    void removeCell(ExpoCell *cell);

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

public Q_SLOTS:
    void update();
    void scheduleUpdate();

Q_SIGNALS:
    void modeChanged();
    void fillGapsChanged();

private:
    void calculateWindowTransformationsClosest();
    void calculateWindowTransformationsKompose();
    void calculateWindowTransformationsNatural();

    QList<ExpoCell *> m_cells;
    LayoutMode m_mode = LayoutNatural;
    QTimer m_updateTimer;
    int m_accuracy = 20;
    bool m_fillGaps = false;
};

class ExpoCell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ExpoLayout *layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(int naturalX READ naturalX WRITE setNaturalX NOTIFY naturalXChanged)
    Q_PROPERTY(int naturalY READ naturalY WRITE setNaturalY NOTIFY naturalYChanged)
    Q_PROPERTY(int naturalWidth READ naturalWidth WRITE setNaturalWidth NOTIFY naturalWidthChanged)
    Q_PROPERTY(int naturalHeight READ naturalHeight WRITE setNaturalHeight NOTIFY naturalHeightChanged)
    Q_PROPERTY(int x READ x NOTIFY xChanged)
    Q_PROPERTY(int y READ y NOTIFY yChanged)
    Q_PROPERTY(int width READ width NOTIFY widthChanged)
    Q_PROPERTY(int height READ height NOTIFY heightChanged)
    Q_PROPERTY(QString persistentKey READ persistentKey WRITE setPersistentKey NOTIFY persistentKeyChanged)

public:
    explicit ExpoCell(QObject *parent = nullptr);
    ~ExpoCell() override;

    ExpoLayout *layout() const;
    void setLayout(ExpoLayout *layout);

    int naturalX() const;
    void setNaturalX(int x);

    int naturalY() const;
    void setNaturalY(int y);

    int naturalWidth() const;
    void setNaturalWidth(int width);

    int naturalHeight() const;
    void setNaturalHeight(int height);

    int x() const;
    void setX(int x);

    int y() const;
    void setY(int y);

    int width() const;
    void setWidth(int width);

    int height() const;
    void setHeight(int height);

    QString persistentKey() const;
    void setPersistentKey(const QString &key);

public Q_SLOTS:
    void update();

Q_SIGNALS:
    void layoutChanged();
    void naturalXChanged();
    void naturalYChanged();
    void naturalWidthChanged();
    void naturalHeightChanged();
    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();
    void persistentKeyChanged();

private:
    QString m_persistentKey;
    int m_naturalX = 0;
    int m_naturalY = 0;
    int m_naturalWidth = 0;
    int m_naturalHeight = 0;
    std::optional<int> m_x;
    std::optional<int> m_y;
    std::optional<int> m_width;
    std::optional<int> m_height;
    QPointer<ExpoLayout> m_layout;
};
