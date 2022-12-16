/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KConfigGroup>
#include <QHash>
#include <QObject>
#include <memory>

namespace KWin
{

class Window;
class KeyboardLayout;
class Xkb;
class VirtualDesktop;

namespace KeyboardLayoutSwitching
{

class Policy : public QObject
{
    Q_OBJECT
public:
    ~Policy() override;

    virtual QString name() const = 0;

    static std::unique_ptr<Policy> create(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config, const QString &policy);

protected:
    explicit Policy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config = KConfigGroup());
    virtual void clearCache() = 0;
    virtual void layoutChanged(uint index) = 0;

    void setLayout(uint index);

    KConfigGroup m_config;
    virtual const QString defaultLayoutEntryKey() const;
    void clearLayouts();

    static const char defaultLayoutEntryKeyPrefix[];
    Xkb *m_xkb;

private:
    KeyboardLayout *m_layout;
};

class GlobalPolicy : public Policy
{
    Q_OBJECT
public:
    explicit GlobalPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config);
    ~GlobalPolicy() override;

    QString name() const override
    {
        return QStringLiteral("Global");
    }

protected:
    void clearCache() override
    {
    }
    void layoutChanged(uint index) override
    {
    }

private:
    const QString defaultLayoutEntryKey() const override;
};

class VirtualDesktopPolicy : public Policy
{
    Q_OBJECT
public:
    explicit VirtualDesktopPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config);
    ~VirtualDesktopPolicy() override;

    QString name() const override
    {
        return QStringLiteral("Desktop");
    }

protected:
    void clearCache() override;
    void layoutChanged(uint index) override;

private:
    void desktopChanged();
    QHash<VirtualDesktop *, quint32> m_layouts;
};

class WindowPolicy : public Policy
{
    Q_OBJECT
public:
    explicit WindowPolicy(Xkb *xkb, KeyboardLayout *layout);
    ~WindowPolicy() override;

    QString name() const override
    {
        return QStringLiteral("Window");
    }

protected:
    void clearCache() override;
    void layoutChanged(uint index) override;

private:
    QHash<Window *, quint32> m_layouts;
};

class ApplicationPolicy : public Policy
{
    Q_OBJECT
public:
    explicit ApplicationPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config);
    ~ApplicationPolicy() override;

    QString name() const override
    {
        return QStringLiteral("WinClass");
    }

protected:
    void clearCache() override;
    void layoutChanged(uint index) override;

private:
    void windowActivated(Window *window);
    QHash<Window *, quint32> m_layouts;
    QHash<QString, quint32> m_layoutsRestored;
};

}
}
