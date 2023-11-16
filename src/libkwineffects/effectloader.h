/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <kwin_export.h>
// KDE
#include <KPluginMetaData>
#include <KSharedConfig>
// Qt
#include <QFlags>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QQueue>
#include <QStaticPlugin>

namespace KWin
{
class Effect;
class EffectPluginFactory;

/**
 * @brief Flags defining how a Loader should load an Effect.
 *
 * These Flags are only used internally when querying the configuration on whether
 * an Effect should be loaded.
 *
 * @see AbstractEffectLoader::readConfig()
 */
enum class LoadEffectFlag {
    Load = 1 << 0, ///< Effect should be loaded
    CheckDefaultFunction = 1 << 2 ///< The Check Default Function needs to be invoked if the Effect provides it
};
Q_DECLARE_FLAGS(LoadEffectFlags, LoadEffectFlag)

/**
 * @brief Interface to describe how an effect loader has to function.
 *
 * The AbstractEffectLoader specifies the methods a concrete loader has to implement and how
 * those methods are expected to perform. Also it provides an interface to the outside world
 * (that is EffectsHandler).
 *
 * The abstraction is used because there are multiple types of Effects which need to be loaded:
 * @li Built-In Effects
 * @li Scripted Effects
 * @li Binary Plugin Effects
 *
 * Serving all of them with one Effect Loader is rather complex given that different stores need
 * to be queried at the same time. Thus the idea is to have one implementation per type and one
 * implementation which makes use of all of them and combines the loading.
 */
class KWIN_EXPORT AbstractEffectLoader : public QObject
{
    Q_OBJECT
public:
    ~AbstractEffectLoader() override;

    /**
     * @brief The KSharedConfig this EffectLoader should operate on.
     *
     * Important: a valid KSharedConfig must be provided before trying to load any effects!
     *
     * @param config
     * @internal
     */
    virtual void setConfig(KSharedConfig::Ptr config);

    /**
     * @brief Whether this Effect Loader can load the Effect with the given @p name.
     *
     * The Effect Loader determines whether it knows or can find an Effect called @p name,
     * and thus whether it can attempt to load the Effect.
     *
     * @param name The name of the Effect to look for.
     * @return bool @c true if the Effect Loader knows this effect, false otherwise
     */
    virtual bool hasEffect(const QString &name) const = 0;

    /**
     * @brief All the Effects this loader knows of.
     *
     * The implementation should re-query its store whenever this method is invoked.
     * It's possible that the store of effects changed (e.g. a new one got installed)
     *
     * @return QStringList The internal names of the known Effects
     */
    virtual QStringList listOfKnownEffects() const = 0;

    /**
     * @brief Synchronous loading of the Effect with the given @p name.
     *
     * Loads the Effect without checking any configuration value or any enabled by default
     * function provided by the Effect.
     *
     * The loader is expected to apply the following checks:
     * If the Effect is already loaded, the Effect should not get loaded again. Thus the loader
     * is expected to track which Effects it has loaded, and which of those have been destroyed.
     * The loader should check whether the Effect is supported. If the Effect indicates it is
     * not supported, it should not get loaded.
     *
     * If the Effect loaded successfully the signal effectLoaded(KWin::Effect*,const QString&)
     * must be emitted. Otherwise the user of the loader is not able to get the loaded Effect.
     * It's not returning the Effect as queryAndLoadAll() is working async and thus the users
     * of the loader are expected to be prepared for async loading.
     *
     * @param name The internal name of the Effect which should be loaded
     * @return bool @c true if the effect could be loaded, @c false in error case
     * @see queryAndLoadAll()
     * @see effectLoaded(KWin::Effect*,const QString&)
     */
    virtual bool loadEffect(const QString &name) = 0;

    /**
     * @brief The Effect Loader should query its store for all available effects and try to load them.
     *
     * The Effect  Loader is supposed to perform this operation in a highly async way. If there is
     * IO which needs to be performed this should be done in a background thread and a queue should
     * be used to load the effects. The loader should make sure to not load more than one Effect
     * in one event cycle. Loading the Effect has to be performed in the Compositor thread and
     * thus blocks the Compositor. Therefore after loading one Effect all events should get
     * processed first, so that the Compositor can perform a painting pass if needed. To simplify
     * this operation one can use the EffectLoadQueue. This requires to add another loadEffect
     * method with the custom loader specific type to refer to an Effect and LoadEffectFlags.
     *
     * The LoadEffectFlags have to be determined by querying the configuration with readConfig().
     * If the Load flag is set the loading can proceed and all the checks from
     * loadEffect(const QString &) have to be applied.
     * In addition if the CheckDefaultFunction flag is set and the Effect provides such a method,
     * it should be queried to determine whether the Effect is enabled by default. If such a method
     * returns @c false the Effect should not get loaded. If the Effect does not provide a way to
     * query whether it's enabled by default at runtime the flag can get ignored.
     *
     * If the Effect loaded successfully the signal effectLoaded(KWin::Effect*,const QString&)
     * must be emitted.
     *
     * @see loadEffect(const QString &)
     * @see effectLoaded(KWin::Effect*,const QString&)
     */
    virtual void queryAndLoadAll() = 0;

    /**
     * @brief Whether the Effect with the given @p name is supported by the compositing backend.
     *
     * @param name The name of the Effect to check.
     * @return bool @c true if it is supported, @c false otherwise
     */
    virtual bool isEffectSupported(const QString &name) const = 0;

    /**
     * @brief Clears the load queue, that is all scheduled Effects are discarded from loading.
     */
    virtual void clear() = 0;

Q_SIGNALS:
    /**
     * @brief The loader emits this signal when it successfully loaded an effect.
     *
     * @param effect The created Effect
     * @param name The internal name of the loaded Effect
     * @return void
     */
    void effectLoaded(KWin::Effect *effect, const QString &name);

protected:
    explicit AbstractEffectLoader(QObject *parent = nullptr);
    /**
     * @brief Checks the configuration for the Effect identified by @p effectName.
     *
     * For each Effect there could be a key called "<effectName>Enabled". If there is such a key
     * the returned flags will contain Load in case it's @c true. If the key does not exist the
     * @p defaultValue determines whether the Effect should be loaded. A value of @c true means
     * that Load | CheckDefaultFunction is returned, in case of @c false no Load flags are returned.
     *
     * @param effectName The name of the Effect to look for in the configuration
     * @param defaultValue Whether the Effect is enabled by default or not.
     * @returns Flags indicating whether the Effect should be loaded and how it should be loaded
     */
    LoadEffectFlags readConfig(const QString &effectName, bool defaultValue) const;

private:
    KSharedConfig::Ptr m_config;
};

template<typename Loader, typename QueueType>
class EffectLoadQueue;
/**
 * @brief Helper class to queue the loading of Effects.
 *
 * Loading an Effect has to be done in the compositor thread and thus the Compositor is blocked
 * while the Effect loads. To not block the compositor for several frames the loading of all
 * Effects need to be queued. By invoking the slot dequeue() through a QueuedConnection the queue
 * can ensure that events are processed between the loading of two Effects and thus the compositor
 * doesn't block.
 *
 * As it needs to be a slot, the queue must subclass QObject, but it also needs to be templated as
 * the information to load an Effect is specific to the Effect Loader. Thus there is the
 * AbstractEffectLoadQueue providing the slots as pure virtual functions and the templated
 * EffectLoadQueue inheriting from AbstractEffectLoadQueue.
 *
 * The queue operates like a normal queue providing enqueue and a scheduleDequeue instead of dequeue.
 *
 */
class AbstractEffectLoadQueue : public QObject
{
    Q_OBJECT
public:
    explicit AbstractEffectLoadQueue(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
protected Q_SLOTS:
    virtual void dequeue() = 0;

private:
    template<typename Loader, typename QueueType>
    friend class EffectLoadQueue;
};

template<typename Loader, typename QueueType>
class EffectLoadQueue : public AbstractEffectLoadQueue
{
public:
    explicit EffectLoadQueue(Loader *parent)
        : AbstractEffectLoadQueue(parent)
        , m_effectLoader(parent)
        , m_dequeueScheduled(false)
    {
    }
    void enqueue(const QPair<QueueType, LoadEffectFlags> value)
    {
        m_queue.enqueue(value);
        scheduleDequeue();
    }
    void clear()
    {
        m_queue.clear();
        m_dequeueScheduled = false;
    }

protected:
    void dequeue() override
    {
        if (m_queue.isEmpty()) {
            return;
        }
        m_dequeueScheduled = false;
        const auto pair = m_queue.dequeue();
        m_effectLoader->loadEffect(pair.first, pair.second);
        scheduleDequeue();
    }

private:
    void scheduleDequeue()
    {
        if (m_queue.isEmpty() || m_dequeueScheduled) {
            return;
        }
        m_dequeueScheduled = true;
        QMetaObject::invokeMethod(this, &AbstractEffectLoadQueue::dequeue, Qt::QueuedConnection);
    }
    Loader *m_effectLoader;
    bool m_dequeueScheduled;
    QQueue<QPair<QueueType, LoadEffectFlags>> m_queue;
};

/**
 * @brief Can load scripted Effects
 */
class KWIN_EXPORT ScriptedEffectLoader : public AbstractEffectLoader
{
    Q_OBJECT
public:
    explicit ScriptedEffectLoader(QObject *parent = nullptr);
    ~ScriptedEffectLoader() override;

    bool hasEffect(const QString &name) const override;
    bool isEffectSupported(const QString &name) const override;
    QStringList listOfKnownEffects() const override;

    void clear() override;
    void queryAndLoadAll() override;
    bool loadEffect(const QString &name) override;
    bool loadEffect(const KPluginMetaData &effect, LoadEffectFlags flags);

private:
    QList<KPluginMetaData> findAllEffects() const;
    KPluginMetaData findEffect(const QString &name) const;
    bool loadJavascriptEffect(const KPluginMetaData &effect);
    bool loadDeclarativeEffect(const KPluginMetaData &effect);

    QStringList m_loadedEffects;
    EffectLoadQueue<ScriptedEffectLoader, KPluginMetaData> *m_queue;
    QMetaObject::Connection m_queryConnection;
};

class PluginEffectLoader : public AbstractEffectLoader
{
    Q_OBJECT
public:
    explicit PluginEffectLoader(QObject *parent = nullptr);
    ~PluginEffectLoader() override;

    bool hasEffect(const QString &name) const override;
    bool isEffectSupported(const QString &name) const override;
    QStringList listOfKnownEffects() const override;

    void clear() override;
    void queryAndLoadAll() override;
    bool loadEffect(const QString &name) override;
    bool loadEffect(const KPluginMetaData &info, LoadEffectFlags flags);

    void setPluginSubDirectory(const QString &directory);

private:
    QList<KPluginMetaData> findAllEffects() const;
    KPluginMetaData findEffect(const QString &name) const;
    EffectPluginFactory *factory(const KPluginMetaData &info) const;
    QStringList m_loadedEffects;
    QString m_pluginSubDirectory;
};

class KWIN_EXPORT EffectLoader : public AbstractEffectLoader
{
    Q_OBJECT
public:
    explicit EffectLoader(QObject *parent = nullptr);
    ~EffectLoader() override;
    bool hasEffect(const QString &name) const override;
    bool isEffectSupported(const QString &name) const override;
    QStringList listOfKnownEffects() const override;
    bool loadEffect(const QString &name) override;
    void queryAndLoadAll() override;
    void setConfig(KSharedConfig::Ptr config) override;
    void clear() override;

private:
    QList<AbstractEffectLoader *> m_loaders;
};

}
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::LoadEffectFlags)
