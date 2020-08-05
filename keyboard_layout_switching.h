/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_KEYBOARD_LAYOUT_SWITCHING_H
#define KWIN_KEYBOARD_LAYOUT_SWITCHING_H

#include <QObject>
#include <QHash>
#include <KConfigGroup>

namespace KWin
{

class AbstractClient;
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

    static Policy *create(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config, const QString &policy);

protected:
    explicit Policy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config = KConfigGroup());
    virtual void clearCache() = 0;
    virtual void layoutChanged() = 0;

    void setLayout(quint32 layout);
    quint32 layout() const;

    KConfigGroup m_config;
    virtual const QString defaultLayoutEntryKey() const;
    void clearLayouts();

    static const char defaultLayoutEntryKeyPrefix[];

private:
    Xkb *m_xkb;
    KeyboardLayout *m_layout;
};

class GlobalPolicy : public Policy
{
    Q_OBJECT
public:
    explicit GlobalPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config);
    ~GlobalPolicy() override;

    QString name() const override {
        return QStringLiteral("Global");
    }

protected:
    void clearCache() override {}
    void layoutChanged() override {}

private:
    const QString defaultLayoutEntryKey() const override;
};

class VirtualDesktopPolicy : public Policy
{
    Q_OBJECT
public:
    explicit VirtualDesktopPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config);
    ~VirtualDesktopPolicy() override;

    QString name() const override {
        return QStringLiteral("Desktop");
    }

protected:
    void clearCache() override;
    void layoutChanged() override;

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

    QString name() const override {
        return QStringLiteral("Window");
    }

protected:
    void clearCache() override;
    void layoutChanged() override;

private:
    QHash<AbstractClient*, quint32> m_layouts;
};

class ApplicationPolicy : public Policy
{
    Q_OBJECT
public:
    explicit ApplicationPolicy(Xkb *xkb, KeyboardLayout *layout, const KConfigGroup &config);
    ~ApplicationPolicy() override;

    QString name() const override {
        return QStringLiteral("WinClass");
    }

protected:
    void clearCache() override;
    void layoutChanged() override;

private:
    void clientActivated(AbstractClient *c);
    QHash<AbstractClient*, quint32> m_layouts;
    QHash<QByteArray, quint32> m_layoutsRestored;
};

}
}

#endif
