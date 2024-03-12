/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationButtonGroup>

struct BasicDecorationState
{
    Qt::Edge titlebarEdge = Qt::TopEdge;
    int titlebarSize = 0;
    int borderSize = 0;
    int resizeOnlyBorderSize = 0;

    QSize clientSize;
};

class BasicDecoration : public KDecoration2::Decoration
{
    Q_OBJECT

public:
    BasicDecoration(QObject *parent, const QVariantList &args);
    ~BasicDecoration() override;

    bool init() override;

protected:
    void paint(QPainter *painter, const QRect &repaintArea) override;

private:
    int preferredTitlebarSize() const;

    void setState(std::function<void(BasicDecorationState *state)> mutator);
    void applyState(const BasicDecorationState &state);

    BasicDecorationState m_current;
    BasicDecorationState m_pending;

    KDecoration2::DecorationButtonGroup *m_frontButtonGroup = nullptr;
    KDecoration2::DecorationButtonGroup *m_backButtonGroup = nullptr;
};
