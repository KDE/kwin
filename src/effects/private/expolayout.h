/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QQuickItem>
#include <QRect>

#include <optional>

class ExpoCell;

class ExpoLayout : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(LayoutMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool fillGaps READ fillGaps WRITE setFillGaps NOTIFY fillGapsChanged)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)

public:
    enum LayoutMode : uint {
        LayoutClosest = 0,
        LayoutNatural = 1,
        LayoutNone = 2
    };
    Q_ENUM(LayoutMode)

    explicit ExpoLayout(QQuickItem *parent = nullptr);

    LayoutMode mode() const;
    void setMode(LayoutMode mode);

    bool fillGaps() const;
    void setFillGaps(bool fill);

    int spacing() const;
    void setSpacing(int spacing);

    void addCell(ExpoCell *cell);
    void removeCell(ExpoCell *cell);

    bool isReady() const;
    void setReady();

    Q_INVOKABLE void forceLayout();

protected:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
#else
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
#endif
    void updatePolish() override;

Q_SIGNALS:
    void modeChanged();
    void fillGapsChanged();
    void spacingChanged();
    void readyChanged();

private:
    void calculateWindowTransformationsClosest();
    void calculateWindowTransformationsNatural();
    void resetTransformations();

    QList<ExpoCell *> m_cells;
    LayoutMode m_mode = LayoutNatural;
    int m_accuracy = 20;
    int m_spacing = 10;
    bool m_ready = false;
    bool m_fillGaps = false;
};

class ExpoCell : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ExpoLayout *layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int naturalX READ naturalX WRITE setNaturalX NOTIFY naturalXChanged)
    Q_PROPERTY(int naturalY READ naturalY WRITE setNaturalY NOTIFY naturalYChanged)
    Q_PROPERTY(int naturalWidth READ naturalWidth WRITE setNaturalWidth NOTIFY naturalWidthChanged)
    Q_PROPERTY(int naturalHeight READ naturalHeight WRITE setNaturalHeight NOTIFY naturalHeightChanged)
    Q_PROPERTY(int x READ x NOTIFY xChanged)
    Q_PROPERTY(int y READ y NOTIFY yChanged)
    Q_PROPERTY(int width READ width NOTIFY widthChanged)
    Q_PROPERTY(int height READ height NOTIFY heightChanged)
    Q_PROPERTY(QString persistentKey READ persistentKey WRITE setPersistentKey NOTIFY persistentKeyChanged)
    Q_PROPERTY(int bottomMargin READ bottomMargin WRITE setBottomMargin NOTIFY bottomMarginChanged)

public:
    explicit ExpoCell(QObject *parent = nullptr);
    ~ExpoCell() override;

    bool isEnabled() const;
    void setEnabled(bool enabled);

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

    QRect naturalRect() const;
    QMargins margins() const;

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

    int bottomMargin() const;
    void setBottomMargin(int margin);

public Q_SLOTS:
    void update();

Q_SIGNALS:
    void layoutChanged();
    void enabledChanged();
    void naturalXChanged();
    void naturalYChanged();
    void naturalWidthChanged();
    void naturalHeightChanged();
    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();
    void persistentKeyChanged();
    void bottomMarginChanged();

private:
    QString m_persistentKey;
    bool m_enabled = true;
    int m_naturalX = 0;
    int m_naturalY = 0;
    int m_naturalWidth = 0;
    int m_naturalHeight = 0;
    QMargins m_margins;
    std::optional<int> m_x;
    std::optional<int> m_y;
    std::optional<int> m_width;
    std::optional<int> m_height;
    QPointer<ExpoLayout> m_layout;
};
