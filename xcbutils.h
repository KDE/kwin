/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012, 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XCB_UTILS_H
#define KWIN_XCB_UTILS_H

#include <kwinglobals.h>
#include "main.h"

#include <QRect>
#include <QRegion>
#include <QScopedPointer>
#include <QVector>

#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/randr.h>

#include <xcb/shm.h>

class TestXcbSizeHints;

namespace KWin {

template <typename T> using ScopedCPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

namespace Xcb {

typedef xcb_window_t WindowId;

// forward declaration of methods
static void defineCursor(xcb_window_t window, xcb_cursor_t cursor);
static void setInputFocus(xcb_window_t window, uint8_t revertTo = XCB_INPUT_FOCUS_POINTER_ROOT, xcb_timestamp_t time = xTime());
static void moveWindow(xcb_window_t window, const QPoint &pos);
static void moveWindow(xcb_window_t window, uint32_t x, uint32_t y);
static void lowerWindow(xcb_window_t window);
static void selectInput(xcb_window_t window, uint32_t events);

/**
 * @brief Variadic template to wrap an xcb request.
 *
 * This struct is part of the generic implementation to wrap xcb requests
 * and fetching their reply. Each request is represented by two templated
 * elements: WrapperData and Wrapper.
 *
 * The WrapperData defines the following types:
 * @li reply_type of the xcb request
 * @li cookie_type of the xcb request
 * @li function pointer type for the xcb request
 * @li function pointer type for the reply
 * This uses variadic template arguments thus it can be used to specify any
 * xcb request.
 *
 * As the WrapperData does not specify the actual function pointers one needs
 * to derive another struct which specifies the function pointer requestFunc and
 * the function pointer replyFunc as static constexpr of type reply_func and
 * reply_type respectively. E.g. for the command xcb_get_geometry:
 * @code
 * struct GeometryData : public WrapperData< xcb_get_geometry_reply_t, xcb_get_geometry_cookie_t, xcb_drawable_t >
 * {
 *    static constexpr request_func requestFunc = &xcb_get_geometry_unchecked;
 *    static constexpr reply_func replyFunc = &xcb_get_geometry_reply;
 * };
 * @endcode
 *
 * To simplify this definition the macro XCB_WRAPPER_DATA is provided.
 * For the same xcb command this looks like this:
 * @code
 * XCB_WRAPPER_DATA(GeometryData, xcb_get_geometry, xcb_drawable_t)
 * @endcode
 *
 * The derived WrapperData has to be passed as first template argument to Wrapper. The other
 * template arguments of Wrapper are the same variadic template arguments as passed into
 * WrapperData. This is ensured at compile time and will cause a compile error in case there
 * is a mismatch of the variadic template arguments passed to WrapperData and Wrapper.
 * Passing another type than a struct derived from WrapperData to Wrapper will result in a
 * compile error. The following code snippets won't compile:
 * @code
 * XCB_WRAPPER_DATA(GeometryData, xcb_get_geometry, xcb_drawable_t)
 * // fails with "static assertion failed: Argument miss-match between Wrapper and WrapperData"
 * class IncorrectArguments : public Wrapper<GeometryData, uint8_t>
 * {
 * public:
 *     IncorrectArguments() = default;
 *     IncorrectArguments(xcb_window_t window) : Wrapper<GeometryData, uint8_t>(window) {}
 * };
 *
 * // fails with "static assertion failed: Data template argument must be derived from WrapperData"
 * class WrapperDataDirectly : public Wrapper<WrapperData<xcb_get_geometry_reply_t, xcb_get_geometry_request_t, xcb_drawable_t>, xcb_drawable_t>
 * {
 * public:
 *     WrapperDataDirectly() = default;
 *     WrapperDataDirectly(xcb_window_t window) : Wrapper<WrapperData<xcb_get_geometry_reply_t, xcb_get_geometry_request_t, xcb_drawable_t>, xcb_drawable_t>(window) {}
 * };
 *
 * // fails with "static assertion failed: Data template argument must be derived from WrapperData"
 * struct FakeWrapperData
 * {
 *     typedef xcb_get_geometry_reply_t reply_type;
 *     typedef xcb_get_geometry_cookie_t cookie_type;
 *     typedef std::tuple<xcb_drawable_t> argument_types;
 *     typedef cookie_type (*request_func)(xcb_connection_t*, xcb_drawable_t);
 *     typedef reply_type *(*reply_func)(xcb_connection_t*, cookie_type, xcb_generic_error_t**);
 *     static constexpr std::size_t argumentCount = 1;
 *     static constexpr request_func requestFunc = &xcb_get_geometry_unchecked;
 *     static constexpr reply_func replyFunc = &xcb_get_geometry_reply;
 * };
 * class NotDerivedFromWrapperData : public Wrapper<FakeWrapperData, xcb_drawable_t>
 * {
 * public:
 *     NotDerivedFromWrapperData() = default;
 *     NotDerivedFromWrapperData(xcb_window_t window) : Wrapper<FakeWrapperData, xcb_drawable_t>(window) {}
 * };
 * @endcode
 *
 * The Wrapper provides an easy to use RAII API which calls the WrapperData's requestFunc in
 * the ctor and fetches the reply the first time it is used. In addition the dtor takes care
 * of freeing the reply if it got fetched, otherwise it discards the reply. The Wrapper can
 * be used as if it were the reply_type directly.
 *
 * There are several command wrappers defined which either subclass Wrapper to add methods to
 * simplify the usage of the result_type or use a typedef. To add a new typedef one can use the
 * macro XCB_WRAPPER which creates the WrapperData struct as XCB_WRAPPER_DATA does and the
 * typedef. E.g:
 * @code
 * XCB_WRAPPER(Geometry, xcb_get_geometry, xcb_drawable_t)
 * @endcode
 *
 * creates a typedef Geometry and the struct GeometryData.
 *
 * Overall this allows to simplify the Xcb usage. For example consider the
 * following xcb code snippet:
 * @code
 * xcb_window_t w; // some window
 * xcb_connection_t *c = connection();
 * const xcb_get_geometry_cookie_t cookie = xcb_get_geometry_unchecked(c, w);
 * // do other stuff
 * xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(c, cookie, nullptr);
 * if (reply) {
 *     reply->x; // do something with the geometry
 * }
 * free(reply);
 * @endcode
 *
 * With the help of the Wrapper class this can be simplified to:
 * @code
 * xcb_window_t w; // some window
 * Xcb::Geometry geo(w);
 * if (!geo.isNull()) {
 *     geo->x; // do something with the geometry
 * }
 * @endcode
 *
 * @see XCB_WRAPPER_DATA
 * @see XCB_WRAPPER
 * @see Wrapper
 * @see WindowAttributes
 * @see OverlayWindow
 * @see WindowGeometry
 * @see Tree
 * @see CurrentInput
 * @see TransientFor
 */
template <typename Reply,
          typename Cookie,
          typename... Args>
struct WrapperData
{
    /**
     * @brief The type returned by the xcb reply function.
     */
    typedef Reply reply_type;
    /**
     * @brief The type returned by the xcb request function.
     */
    typedef Cookie cookie_type;
    /**
     * @brief Variadic arguments combined as a std::tuple.
     * @internal Used for verifying the arguments.
     */
    typedef std::tuple<Args...> argument_types;
    /**
     * @brief The function pointer definition for the xcb request function.
     */
    typedef Cookie (*request_func)(xcb_connection_t*, Args...);
    /**
     * @brief The function pointer definition for the xcb reply function.
     */
    typedef Reply *(*reply_func)(xcb_connection_t*, Cookie, xcb_generic_error_t**);
    /**
     * @brief Number of variadic arguments.
     * @internal Used for verifying the arguments.
     */
    static constexpr std::size_t argumentCount = sizeof...(Args);
};

/**
 * @brief Partial template specialization for WrapperData with no further arguments.
 *
 * This will be used for xcb requests just taking the xcb_connection_t* argument.
 */
template <typename Reply,
          typename Cookie>
struct WrapperData<Reply, Cookie>
{
    typedef Reply reply_type;
    typedef Cookie cookie_type;
    typedef std::tuple<> argument_types;
    typedef Cookie (*request_func)(xcb_connection_t*);
    typedef Reply *(*reply_func)(xcb_connection_t*, Cookie, xcb_generic_error_t**);
    static constexpr std::size_t argumentCount = 0;
};

/**
 * @brief Abstract base class for the wrapper.
 *
 * This class contains the complete functionality of the Wrapper. It's only an abstract
 * base class to provide partial template specialization for more specific constructors.
 */
template<typename Data>
class AbstractWrapper
{
public:
    typedef typename Data::cookie_type Cookie;
    typedef typename Data::reply_type Reply;
    virtual ~AbstractWrapper() {
        cleanup();
    }
    inline AbstractWrapper &operator=(const AbstractWrapper &other) {
        if (this != &other) {
            // if we had managed a reply, free it
            cleanup();
            // copy members
            m_retrieved = other.m_retrieved;
            m_cookie = other.m_cookie;
            m_window = other.m_window;
            m_reply = other.m_reply;
            // take over the responsibility for the reply pointer
            takeFromOther(const_cast<AbstractWrapper&>(other));
        }
        return *this;
    }

    inline const Reply *operator->() {
        getReply();
        return m_reply;
    }
    inline bool isNull() {
        getReply();
        return m_reply == nullptr;
    }
    inline bool isNull() const {
        const_cast<AbstractWrapper*>(this)->getReply();
        return m_reply == NULL;
    }
    inline operator bool() {
        return !isNull();
    }
    inline operator bool() const {
        return !isNull();
    }
    inline const Reply *data() {
        getReply();
        return m_reply;
    }
    inline const Reply *data() const {
        const_cast<AbstractWrapper*>(this)->getReply();
        return m_reply;
    }
    inline WindowId window() const {
        return m_window;
    }
    inline bool isRetrieved() const {
        return m_retrieved;
    }
    /**
     * Returns the value of the reply pointer referenced by this object. The reply pointer of
     * this object will be reset to null. Calling any method which requires the reply to be valid
     * will crash.
     *
     * Callers of this function take ownership of the pointer.
     */
    inline Reply *take() {
        getReply();
        Reply *ret = m_reply;
        m_reply = nullptr;
        m_window = XCB_WINDOW_NONE;
        return ret;
    }

protected:
    AbstractWrapper()
        : m_retrieved(false)
        , m_window(XCB_WINDOW_NONE)
        , m_reply(nullptr)
    {
        m_cookie.sequence = 0;
    }
    explicit AbstractWrapper(WindowId window, Cookie cookie)
        : m_retrieved(false)
        , m_cookie(cookie)
        , m_window(window)
        , m_reply(nullptr)
    {
    }
    explicit AbstractWrapper(const AbstractWrapper &other)
        : m_retrieved(other.m_retrieved)
        , m_cookie(other.m_cookie)
        , m_window(other.m_window)
        , m_reply(nullptr)
    {
        takeFromOther(const_cast<AbstractWrapper&>(other));
    }
    void getReply() {
        if (m_retrieved || !m_cookie.sequence) {
            return;
        }
        m_reply = Data::replyFunc(connection(), m_cookie, nullptr);
        m_retrieved = true;
    }

private:
    inline void cleanup() {
        if (!m_retrieved && m_cookie.sequence) {
            xcb_discard_reply(connection(), m_cookie.sequence);
        } else if (m_reply) {
            free(m_reply);
        }
    }
    inline void takeFromOther(AbstractWrapper &other) {
        if (m_retrieved) {
            m_reply = other.take();
        } else {
            //ensure that other object doesn't try to get the reply or discards it in the dtor
            other.m_retrieved = true;
            other.m_window = XCB_WINDOW_NONE;
        }
    }
    bool m_retrieved;
    Cookie m_cookie;
    WindowId m_window;
    Reply *m_reply;
};

/**
 * @brief Template to compare the arguments of two std::tuple.
 *
 * @internal Used by static_assert in Wrapper
 */
template <typename T1, typename T2, std::size_t I>
struct tupleCompare
{
    typedef typename std::tuple_element<I, T1>::type tuple1Type;
    typedef typename std::tuple_element<I, T2>::type tuple2Type;
    /**
     * @c true if both tuple have the same arguments, @c false otherwise.
     */
    static constexpr bool value = std::is_same< tuple1Type, tuple2Type >::value && tupleCompare<T1, T2, I-1>::value;
};

/**
 * @brief Recursive template case for first tuple element.
 */
template <typename T1, typename T2>
struct tupleCompare<T1, T2, 0>
{
    typedef typename std::tuple_element<0, T1>::type tuple1Type;
    typedef typename std::tuple_element<0, T2>::type tuple2Type;
    static constexpr bool value = std::is_same< tuple1Type, tuple2Type >::value;
};

/**
 * @brief Wrapper taking a WrapperData as first template argument and xcb request args as variadic args.
 */
template<typename Data, typename... Args>
class Wrapper : public AbstractWrapper<Data>
{
public:
    static_assert(!std::is_same<Data, Xcb::WrapperData<typename Data::reply_type, typename Data::cookie_type, Args...> >::value,
                  "Data template argument must be derived from WrapperData");
    static_assert(std::is_base_of<Xcb::WrapperData<typename Data::reply_type, typename Data::cookie_type, Args...>, Data>::value,
                  "Data template argument must be derived from WrapperData");
    static_assert(sizeof...(Args) == Data::argumentCount,
                    "Wrapper and WrapperData need to have same template argument count");
    static_assert(tupleCompare<std::tuple<Args...>, typename Data::argument_types, sizeof...(Args) - 1>::value,
                    "Argument miss-match between Wrapper and WrapperData");
    Wrapper() = default;
    explicit Wrapper(Args... args)
        : AbstractWrapper<Data>(XCB_WINDOW_NONE, Data::requestFunc(connection(), args...))
    {
    }
    explicit Wrapper(xcb_window_t w, Args... args)
        : AbstractWrapper<Data>(w, Data::requestFunc(connection(), args...))
    {
    }
};

/**
 * @brief Template specialization for xcb_window_t being first variadic argument.
 */
template<typename Data, typename... Args>
class Wrapper<Data, xcb_window_t, Args...> : public AbstractWrapper<Data>
{
public:
    static_assert(!std::is_same<Data, Xcb::WrapperData<typename Data::reply_type, typename Data::cookie_type, xcb_window_t, Args...> >::value,
                  "Data template argument must be derived from WrapperData");
    static_assert(std::is_base_of<Xcb::WrapperData<typename Data::reply_type, typename Data::cookie_type, xcb_window_t, Args...>, Data>::value,
                  "Data template argument must be derived from WrapperData");
    static_assert(sizeof...(Args) + 1 == Data::argumentCount,
                    "Wrapper and WrapperData need to have same template argument count");
    static_assert(tupleCompare<std::tuple<xcb_window_t, Args...>, typename Data::argument_types, sizeof...(Args)>::value,
                    "Argument miss-match between Wrapper and WrapperData");
    Wrapper() = default;
    explicit Wrapper(xcb_window_t w, Args... args)
        : AbstractWrapper<Data>(w, Data::requestFunc(connection(), w, args...))
    {
    }
};

/**
 * @brief Template specialization for no variadic arguments.
 *
 * It's needed to prevent ambiguous constructors being generated.
 */
template<typename Data>
class Wrapper<Data> : public AbstractWrapper<Data>
{
public:
    static_assert(!std::is_same<Data, Xcb::WrapperData<typename Data::reply_type, typename Data::cookie_type> >::value,
                  "Data template argument must be derived from WrapperData");
    static_assert(std::is_base_of<Xcb::WrapperData<typename Data::reply_type, typename Data::cookie_type>, Data>::value,
                  "Data template argument must be derived from WrapperData");
    static_assert(Data::argumentCount == 0, "Wrapper for no arguments constructed with WrapperData with arguments");
    explicit Wrapper()
        : AbstractWrapper<Data>(XCB_WINDOW_NONE, Data::requestFunc(connection()))
    {
    }
};

class Atom
{
public:
    explicit Atom(const QByteArray &name, bool onlyIfExists = false, xcb_connection_t *c = connection())
        : m_connection(c)
        , m_retrieved(false)
        , m_cookie(xcb_intern_atom_unchecked(m_connection, onlyIfExists, name.length(), name.constData()))
        , m_atom(XCB_ATOM_NONE)
        , m_name(name)
        {
        }
    Atom() = delete;
    Atom(const Atom &) = delete;

    ~Atom() {
        if (!m_retrieved && m_cookie.sequence) {
            xcb_discard_reply(m_connection, m_cookie.sequence);
        }
    }

    operator xcb_atom_t() const {
        (const_cast<Atom*>(this))->getReply();
        return m_atom;
    }
    bool isValid() {
        getReply();
        return m_atom != XCB_ATOM_NONE;
    }
    bool isValid() const {
        (const_cast<Atom*>(this))->getReply();
        return m_atom != XCB_ATOM_NONE;
    }

    inline const QByteArray &name() const {
        return m_name;
    }

private:
    void getReply() {
        if (m_retrieved || !m_cookie.sequence) {
            return;
        }
        ScopedCPointer<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(m_connection, m_cookie, nullptr));
        if (!reply.isNull()) {
            m_atom = reply->atom;
        }
        m_retrieved = true;
    }
    xcb_connection_t *m_connection;
    bool m_retrieved;
    xcb_intern_atom_cookie_t m_cookie;
    xcb_atom_t m_atom;
    QByteArray m_name;
};

/**
 * @brief Macro to create the WrapperData subclass.
 *
 * Creates a struct with name @p __NAME__ for the xcb request identified by @p __REQUEST__.
 * The variadic arguments are used to pass as template arguments to the WrapperData.
 *
 * The @p __REQUEST__ is the common prefix of the cookie type, reply type, request function and
 * reply function. E.g. "xcb_get_geometry" is used to create:
 * @li cookie type xcb_get_geometry_cookie_t
 * @li reply type xcb_get_geometry_reply_t
 * @li request function pointer xcb_get_geometry_unchecked
 * @li reply function pointer xcb_get_geometry_reply
 *
 * @param __NAME__ The name of the WrapperData subclass
 * @param __REQUEST__ The name of the xcb request, e.g. xcb_get_geometry
 * @param __VA_ARGS__ The variadic template arguments, e.g. xcb_drawable_t
 * @see XCB_WRAPPER
 */
#define XCB_WRAPPER_DATA( __NAME__, __REQUEST__, ... ) \
    struct __NAME__ : public WrapperData< __REQUEST__##_reply_t, __REQUEST__##_cookie_t, __VA_ARGS__ > \
    { \
        static constexpr request_func requestFunc = &__REQUEST__##_unchecked; \
        static constexpr reply_func replyFunc = &__REQUEST__##_reply; \
    };

/**
 * @brief Macro to create Wrapper typedef and WrapperData.
 *
 * This macro expands the XCB_WRAPPER_DATA macro and creates an additional
 * typedef for Wrapper with name @p __NAME__. The created WrapperData is also derived
 * from @p __NAME__ with "Data" as suffix.
 *
 * @param __NAME__ The name for the Wrapper typedef
 * @param __REQUEST__ The name of the xcb request, passed to XCB_WRAPPER_DATA
 * @param __VA_ARGS__ The variadic template arguments for Wrapper and WrapperData
 * @see XCB_WRAPPER_DATA
 */
#define XCB_WRAPPER( __NAME__, __REQUEST__, ... ) \
    XCB_WRAPPER_DATA( __NAME__##Data, __REQUEST__, __VA_ARGS__ ) \
    typedef Wrapper< __NAME__##Data, __VA_ARGS__ > __NAME__;

XCB_WRAPPER(WindowAttributes, xcb_get_window_attributes, xcb_window_t)
XCB_WRAPPER(OverlayWindow, xcb_composite_get_overlay_window, xcb_window_t)

XCB_WRAPPER_DATA(GeometryData, xcb_get_geometry, xcb_drawable_t)
class WindowGeometry : public Wrapper<GeometryData, xcb_window_t>
{
public:
    WindowGeometry() : Wrapper<GeometryData, xcb_window_t>() {}
    explicit WindowGeometry(xcb_window_t window) : Wrapper<GeometryData, xcb_window_t>(window) {}

    inline QRect rect() {
        const xcb_get_geometry_reply_t *geometry = data();
        if (!geometry) {
            return QRect();
        }
        return QRect(geometry->x, geometry->y, geometry->width, geometry->height);
    }

    inline QSize size() {
        const xcb_get_geometry_reply_t *geometry = data();
        if (!geometry) {
            return QSize();
        }
        return QSize(geometry->width, geometry->height);
    }
};

XCB_WRAPPER_DATA(TreeData, xcb_query_tree, xcb_window_t)
class Tree : public Wrapper<TreeData, xcb_window_t>
{
public:
    explicit Tree(WindowId window) : Wrapper<TreeData, xcb_window_t>(window) {}

    inline WindowId *children() {
        if (isNull() || data()->children_len == 0) {
            return nullptr;
        }
        return xcb_query_tree_children(data());
    }
    inline xcb_window_t parent() {
        if (isNull())
            return XCB_WINDOW_NONE;
        return (*this)->parent;
    }
};

XCB_WRAPPER(Pointer, xcb_query_pointer, xcb_window_t)

struct CurrentInputData : public WrapperData< xcb_get_input_focus_reply_t, xcb_get_input_focus_cookie_t >
{
    static constexpr request_func requestFunc = &xcb_get_input_focus_unchecked;
    static constexpr reply_func replyFunc = &xcb_get_input_focus_reply;
};

class CurrentInput : public Wrapper<CurrentInputData>
{
public:
    CurrentInput() : Wrapper<CurrentInputData>() {}

    inline xcb_window_t window() {
        if (isNull())
            return XCB_WINDOW_NONE;
        return (*this)->focus;
    }
};

struct QueryKeymapData : public WrapperData< xcb_query_keymap_reply_t, xcb_query_keymap_cookie_t >
{
    static constexpr request_func requestFunc = &xcb_query_keymap_unchecked;
    static constexpr reply_func replyFunc = &xcb_query_keymap_reply;
};

class QueryKeymap : public Wrapper<QueryKeymapData>
{
public:
    QueryKeymap() : Wrapper<QueryKeymapData>() {}
};

struct ModifierMappingData : public WrapperData< xcb_get_modifier_mapping_reply_t, xcb_get_modifier_mapping_cookie_t >
{
    static constexpr request_func requestFunc = &xcb_get_modifier_mapping_unchecked;
    static constexpr reply_func replyFunc = &xcb_get_modifier_mapping_reply;
};

class ModifierMapping : public Wrapper<ModifierMappingData>
{
public:
    ModifierMapping() : Wrapper<ModifierMappingData>() {}

    inline xcb_keycode_t *keycodes() {
        if (isNull()) {
            return nullptr;
        }
        return xcb_get_modifier_mapping_keycodes(data());
    }
    inline int size() {
        if (isNull()) {
            return 0;
        }
        return xcb_get_modifier_mapping_keycodes_length(data());
    }
};

XCB_WRAPPER_DATA(PropertyData, xcb_get_property, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint32_t, uint32_t)
class Property : public Wrapper<PropertyData, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint32_t, uint32_t>
{
public:
    Property()
        : Wrapper<PropertyData, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint32_t, uint32_t>()
        , m_type(XCB_ATOM_NONE)
    {
    }
    Property(const Property &other)
        : Wrapper<PropertyData, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint32_t, uint32_t>(other)
        , m_type(other.m_type)
    {
    }
    explicit Property(uint8_t _delete, xcb_window_t window, xcb_atom_t property, xcb_atom_t type, uint32_t long_offset, uint32_t long_length)
        : Wrapper<PropertyData, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint32_t, uint32_t>(window, _delete, window, property, type, long_offset, long_length)
        , m_type(type)
    {
    }
    Property &operator=(const Property &other) {
        Wrapper<PropertyData, uint8_t, xcb_window_t, xcb_atom_t, xcb_atom_t, uint32_t, uint32_t>::operator=(other);
        m_type = other.m_type;
        return *this;
    }

    /**
     * @brief Overloaded method for convenience.
     *
     * Uses the type which got passed into the ctor and derives the format from the sizeof(T).
     * Note: for the automatic format detection the size of the type T may not vary between
     * architectures. Thus one needs to use e.g. uint32_t instead of long. In general all xcb
     * data types can be used, all Xlib data types can not be used.
     *
     * @param defaultValue The default value to return in case of error
     * @param ok Set to @c false in case of error, @c true in case of success
     * @return The read value or @p defaultValue in error case
     */
    template <typename T>
    inline typename std::enable_if<!std::is_pointer<T>::value, T>::type value(T defaultValue = T(), bool *ok = nullptr) {
        return value<T>(sizeof(T) * 8, m_type, defaultValue, ok);
    }
    /**
     * @brief Reads the property as a POD type.
     *
     * Returns the first value of the property data. In case of @p format or @p type mismatch
     * the @p defaultValue is returned. The optional argument @p ok is set
     * to @c false in case of error and to @c true in case of successful reading of
     * the property.
     *
     * @param format The expected format of the property value, e.g. 32 for XCB_ATOM_CARDINAL
     * @param type The expected type of the property value, e.g. XCB_ATOM_CARDINAL
     * @param defaultValue The default value to return in case of error
     * @param ok Set to @c false in case of error, @c true in case of success
     * @return The read value or @p defaultValue in error case
     */
    template <typename T>
    inline typename std::enable_if<!std::is_pointer<T>::value, T>::type value(uint8_t format, xcb_atom_t type, T defaultValue = T(), bool *ok = nullptr) {
        T *reply = value<T*>(format, type, nullptr, ok);
        if (!reply) {
            return defaultValue;
        }
        return reply[0];
    }
    /**
     * @brief Overloaded method for convenience.
     *
     * Uses the type which got passed into the ctor and derives the format from the sizeof(T).
     * Note: for the automatic format detection the size of the type T may not vary between
     * architectures. Thus one needs to use e.g. uint32_t instead of long. In general all xcb
     * data types can be used, all Xlib data types can not be used.
     *
     * @param defaultValue The default value to return in case of error
     * @param ok Set to @c false in case of error, @c true in case of success
     * @return The read value or @p defaultValue in error case
     */
    template <typename T>
    inline typename std::enable_if<std::is_pointer<T>::value, T>::type value(T defaultValue = nullptr, bool *ok = nullptr) {
        return value<T>(sizeof(typename std::remove_pointer<T>::type) * 8, m_type, defaultValue, ok);
    }
    /**
     * @brief Reads the property as an array of T.
     *
     * This method is an overload for the case that T is a pointer type.
     *
     * Return the property value casted to the pointer type T. In case of @p format
     * or @p type mismatch the @p defaultValue is returned. Also if the value length
     * is @c 0 the @p defaultValue is returned. The optional argument @p ok is set
     * to @c false in case of error and to @c true in case of successful reading of
     * the property. Ok will always be true if the property exists and has been
     * successfully read, even in the case the property is empty and its length is 0
     *
     * @param format The expected format of the property value, e.g. 32 for XCB_ATOM_CARDINAL
     * @param type The expected type of the property value, e.g. XCB_ATOM_CARDINAL
     * @param defaultValue The default value to return in case of error
     * @param ok Set to @c false in case of error, @c true in case of success
     * @return The read value or @p defaultValue in error case
     */
    template <typename T>
    inline typename std::enable_if<std::is_pointer<T>::value, T>::type value(uint8_t format, xcb_atom_t type, T defaultValue = nullptr, bool *ok = nullptr) {
        if (ok) {
            *ok = false;
        }
        const PropertyData::reply_type *reply = data();
        if (!reply) {
            return defaultValue;
        }
        if (reply->type != type) {
            return defaultValue;
        }
        if (reply->format != format) {
            return defaultValue;
        }

        if (ok) {
            *ok = true;
        }
        if (xcb_get_property_value_length(reply) == 0) {
            return defaultValue;
        }

        return reinterpret_cast<T>(xcb_get_property_value(reply));
    }
    /**
     * @brief Reads the property as string and returns a QByteArray.
     *
     * In case of error this method returns a null QByteArray.
     */
    inline QByteArray toByteArray(uint8_t format = 8, xcb_atom_t type = XCB_ATOM_STRING, bool *ok = nullptr) {
        bool valueOk = false;
        const char *reply = value<const char*>(format, type, nullptr, &valueOk);
        if (ok) {
            *ok = valueOk;
        }

        if (valueOk && !reply) {
            return QByteArray("", 0); // valid, not null, but empty data
        } else if (!valueOk) {
            return QByteArray(); // Property not found, data empty and null
        }
        return QByteArray(reply, xcb_get_property_value_length(data()));
    }
    /**
     * @brief Overloaded method for convenience.
     */
    inline QByteArray toByteArray(bool *ok) {
        return toByteArray(8, m_type, ok);
    }
    /**
     * @brief Reads the property as a boolean value.
     *
     * If the property reply length is @c 1 the first element is interpreted as a boolean
     * value returning @c true for any value unequal to @c 0 and @c false otherwise.
     *
     * In case of error this method returns @c false. Thus it is not possible to distinguish
     * between error case and a read @c false value. Use the optional argument @p ok to
     * distinguish the error case.
     *
     * @param format Expected format. Defaults to 32.
     * @param type Expected type Defaults to XCB_ATOM_CARDINAL.
     * @param ok Set to @c false in case of error, @c true in case of success
     * @return bool The first element interpreted as a boolean value or @c false in error case
     * @see value
     */
    inline bool toBool(uint8_t format = 32, xcb_atom_t type = XCB_ATOM_CARDINAL, bool *ok = nullptr) {
        bool *reply = value<bool*>(format, type, nullptr, ok);
        if (!reply) {
            return false;
        }
        if (data()->value_len != 1) {
            if (ok) {
                *ok = false;
            }
            return false;
        }
        return reply[0] != 0;
    }
    /**
     * @brief Overloaded method for convenience.
     */
    inline bool toBool(bool *ok) {
        return toBool(32, m_type, ok);
    }
private:
    xcb_atom_t m_type;
};

class StringProperty : public Property
{
public:
    StringProperty() = default;
    explicit StringProperty(xcb_window_t w, xcb_atom_t p)
        : Property(false, w, p, XCB_ATOM_STRING, 0, 10000)
    {
    }
    operator QByteArray() {
        return toByteArray();
    }
};

class TransientFor : public Property
{
public:
    explicit TransientFor(WindowId window)
        : Property(0, window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 0, 1)
    {
    }

    /**
     * @brief Fill given window pointer with the WM_TRANSIENT_FOR property of a window.
     * @param prop WM_TRANSIENT_FOR property value.
     * @returns @c true on success, @c false otherwise
     */
    inline bool getTransientFor(WindowId *prop) {
        WindowId *windows = value<WindowId*>();
        if (!windows) {
            return false;
        }

        *prop = *windows;
        return true;
    }
};

class GeometryHints
{
public:
    GeometryHints() = default;
    void init(xcb_window_t window) {
        Q_ASSERT(window);
        if (m_window) {
            // already initialized
            return;
        }
        m_window = window;
        fetch();
    }
    void fetch() {
        if (!m_window) {
            return;
        }
        m_sizeHints = nullptr;
        m_hints = NormalHints(m_window);
    }
    void read() {
        m_sizeHints = m_hints.sizeHints();
    }

    bool hasPosition() const {
        return testFlag(NormalHints::SizeHints::UserPosition) || testFlag(NormalHints::SizeHints::ProgramPosition);
    }
    bool hasSize() const {
        return testFlag(NormalHints::SizeHints::UserSize) || testFlag(NormalHints::SizeHints::ProgramSize);
    }
    bool hasMinSize() const {
        return testFlag(NormalHints::SizeHints::MinSize);
    }
    bool hasMaxSize() const {
        return testFlag(NormalHints::SizeHints::MaxSize);
    }
    bool hasResizeIncrements() const {
        return testFlag(NormalHints::SizeHints::ResizeIncrements);
    }
    bool hasAspect() const {
        return testFlag(NormalHints::SizeHints::Aspect);
    }
    bool hasBaseSize() const {
        return testFlag(NormalHints::SizeHints::BaseSize);
    }
    bool hasWindowGravity() const {
        return testFlag(NormalHints::SizeHints::WindowGravity);
    }
    QSize maxSize() const {
        if (!hasMaxSize()) {
            return QSize(INT_MAX, INT_MAX);
        }
        return QSize(qMax(m_sizeHints->maxWidth, 1), qMax(m_sizeHints->maxHeight, 1));
    }
    QSize minSize() const {
        if (!hasMinSize()) {
            // according to ICCCM 4.1.23 base size should be used as a fallback
            return baseSize();
        }
        return QSize(m_sizeHints->minWidth, m_sizeHints->minHeight);
    }
    QSize baseSize() const {
        // Note: not using minSize as fallback
        if (!hasBaseSize()) {
            return QSize(0, 0);
        }
        return QSize(m_sizeHints->baseWidth, m_sizeHints->baseHeight);
    }
    QSize resizeIncrements() const {
        if (!hasResizeIncrements()) {
            return QSize(1, 1);
        }
        return QSize(qMax(m_sizeHints->widthInc, 1), qMax(m_sizeHints->heightInc, 1));
    }
    xcb_gravity_t windowGravity() const {
        if (!hasWindowGravity()) {
            return XCB_GRAVITY_NORTH_WEST;
        }
        return xcb_gravity_t(m_sizeHints->winGravity);
    }
    QSize minAspect() const {
        if (!hasAspect()) {
            return QSize(1, INT_MAX);
        }
        // prevent devision by zero
        return QSize(m_sizeHints->minAspect[0], qMax(m_sizeHints->minAspect[1], 1));
    }
    QSize maxAspect() const {
        if (!hasAspect()) {
            return QSize(INT_MAX, 1);
        }
        // prevent devision by zero
        return QSize(m_sizeHints->maxAspect[0], qMax(m_sizeHints->maxAspect[1], 1));
    }

private:
    /**
    * NormalHints as specified in ICCCM 4.1.2.3.
    */
    class NormalHints : public Property
    {
    public:
        struct SizeHints {
            enum Flags {
                UserPosition = 1,
                UserSize = 2,
                ProgramPosition = 4,
                ProgramSize = 8,
                MinSize = 16,
                MaxSize = 32,
                ResizeIncrements = 64,
                Aspect = 128,
                BaseSize = 256,
                WindowGravity = 512
            };
            qint32 flags = 0;
            qint32 pad[4] = {0, 0, 0, 0};
            qint32 minWidth = 0;
            qint32 minHeight = 0;
            qint32 maxWidth = 0;
            qint32 maxHeight = 0;
            qint32 widthInc = 0;
            qint32 heightInc = 0;
            qint32 minAspect[2] = {0, 0};
            qint32 maxAspect[2] = {0, 0};
            qint32 baseWidth = 0;
            qint32 baseHeight = 0;
            qint32 winGravity = 0;
        };
        explicit NormalHints() : Property() {};
        explicit NormalHints(WindowId window)
            : Property(0, window, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 0, 18)
        {
        }
        inline SizeHints *sizeHints() {
            return value<SizeHints*>(32, XCB_ATOM_WM_SIZE_HINTS, nullptr);
        }
    };
    friend TestXcbSizeHints;
    bool testFlag(NormalHints::SizeHints::Flags flag) const {
        if (!m_window || !m_sizeHints) {
            return false;
        }
        return m_sizeHints->flags & flag;
    }
    xcb_window_t m_window = XCB_WINDOW_NONE;
    NormalHints m_hints;
    NormalHints::SizeHints *m_sizeHints = nullptr;
};

class MotifHints
{
public:
    MotifHints(xcb_atom_t atom) : m_atom(atom) {}
    void init(xcb_window_t window) {
        Q_ASSERT(window);
        if (m_window) {
            // already initialized
            return;
        }
        m_window = window;
        fetch();
    }
    void fetch() {
        if (!m_window) {
            return;
        }
        m_hints = nullptr;
        m_prop = Property(0, m_window, m_atom, m_atom, 0, 5);
    }
    void read() {
        m_hints = m_prop.value<MwmHints*>(32, m_atom, nullptr);
    }
    bool hasDecoration() const {
        if (!m_window || !m_hints) {
            return false;
        }
        return m_hints->flags & uint32_t(Hints::Decorations);
    }
    bool noBorder() const {
        if (!hasDecoration()) {
            return false;
        }
        return !m_hints->decorations;
    }
    bool resize() const {
        return testFunction(Functions::Resize);
    }
    bool move() const {
        return testFunction(Functions::Move);
    }
    bool minimize() const {
        return testFunction(Functions::Minimize);
    }
    bool maximize() const {
        return testFunction(Functions::Maximize);
    }
    bool close() const {
        return testFunction(Functions::Close);
    }

private:
    struct MwmHints {
        uint32_t flags;
        uint32_t functions;
        uint32_t decorations;
        int32_t input_mode;
        uint32_t status;
    };
    enum class Hints {
        Functions = (1L << 0),
        Decorations = (1L << 1)
    };
    enum class Functions {
        All = (1L << 0),
        Resize = (1L << 1),
        Move = (1L << 2),
        Minimize = (1L << 3),
        Maximize = (1L << 4),
        Close = (1L << 5)
    };
    bool testFunction(Functions flag) const {
        if (!m_window || !m_hints) {
            return true;
        }
        if (!(m_hints->flags & uint32_t(Hints::Functions))) {
            return true;
        }
        // if MWM_FUNC_ALL is set, other flags say what to turn _off_
        const bool set_value = ((m_hints->functions & uint32_t(Functions::All)) == 0);
        if (m_hints->functions & uint32_t(flag)) {
            return set_value;
        }
        return !set_value;
    }
    xcb_window_t m_window = XCB_WINDOW_NONE;
    Property m_prop;
    xcb_atom_t m_atom;
    MwmHints *m_hints = nullptr;
};

namespace RandR
{
XCB_WRAPPER(ScreenInfo, xcb_randr_get_screen_info, xcb_window_t)

XCB_WRAPPER_DATA(ScreenResourcesData, xcb_randr_get_screen_resources, xcb_window_t)
class ScreenResources : public Wrapper<ScreenResourcesData, xcb_window_t>
{
public:
    explicit ScreenResources(WindowId window) : Wrapper<ScreenResourcesData, xcb_window_t>(window) {}

    inline xcb_randr_crtc_t *crtcs() {
        if (isNull()) {
            return nullptr;
        }
        return xcb_randr_get_screen_resources_crtcs(data());
    }
    inline xcb_randr_mode_info_t *modes() {
        if (isNull()) {
            return nullptr;
        }
        return xcb_randr_get_screen_resources_modes(data());
    }
    inline uint8_t *names() {
        if (isNull()) {
            return nullptr;
        }
        return xcb_randr_get_screen_resources_names(data());
    }
};

XCB_WRAPPER_DATA(CrtcGammaData, xcb_randr_get_crtc_gamma, xcb_randr_crtc_t)
class CrtcGamma : public Wrapper<CrtcGammaData, xcb_randr_crtc_t>
{
public:
    explicit CrtcGamma(xcb_randr_crtc_t c) : Wrapper<CrtcGammaData, xcb_randr_crtc_t>(c) {}

    inline uint16_t *red() {
        return xcb_randr_get_crtc_gamma_red(data());
    }
    inline uint16_t *green() {
        return xcb_randr_get_crtc_gamma_green(data());
    }
    inline uint16_t *blue() {
        return xcb_randr_get_crtc_gamma_blue(data());
    }
};

XCB_WRAPPER_DATA(CrtcInfoData, xcb_randr_get_crtc_info, xcb_randr_crtc_t, xcb_timestamp_t)
class CrtcInfo : public Wrapper<CrtcInfoData, xcb_randr_crtc_t, xcb_timestamp_t>
{
public:
    CrtcInfo() = default;
    CrtcInfo(const CrtcInfo&) = default;
    explicit CrtcInfo(xcb_randr_crtc_t c, xcb_timestamp_t t) : Wrapper<CrtcInfoData, xcb_randr_crtc_t, xcb_timestamp_t>(c, t) {}

    inline QRect rect() {
        const CrtcInfoData::reply_type *info = data();
        if (!info || info->num_outputs == 0 || info->mode == XCB_NONE || info->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
            return QRect();
        }
        return QRect(info->x, info->y, info->width, info->height);
    }
    inline xcb_randr_output_t *outputs() {
        const CrtcInfoData::reply_type *info = data();
        if (!info || info->num_outputs == 0 || info->mode == XCB_NONE || info->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
            return nullptr;
        }
        return xcb_randr_get_crtc_info_outputs(info);
    }
};

XCB_WRAPPER_DATA(OutputInfoData, xcb_randr_get_output_info, xcb_randr_output_t, xcb_timestamp_t)
class OutputInfo : public Wrapper<OutputInfoData, xcb_randr_output_t, xcb_timestamp_t>
{
public:
    OutputInfo() = default;
    OutputInfo(const OutputInfo&) = default;
    explicit OutputInfo(xcb_randr_output_t c, xcb_timestamp_t t) : Wrapper<OutputInfoData, xcb_randr_output_t, xcb_timestamp_t>(c, t) {}

    inline QString name() {
        const OutputInfoData::reply_type *info = data();
        if (!info || info->num_crtcs == 0 || info->num_modes == 0 || info->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
            return QString();
        }
        return QString::fromUtf8(reinterpret_cast<char*>(xcb_randr_get_output_info_name(info)), info->name_len);
    }
};

XCB_WRAPPER_DATA(CurrentResourcesData, xcb_randr_get_screen_resources_current, xcb_window_t)
class CurrentResources : public Wrapper<CurrentResourcesData, xcb_window_t>
{
public:
    explicit CurrentResources(WindowId window) : Wrapper<CurrentResourcesData, xcb_window_t>(window) {}

    inline xcb_randr_crtc_t *crtcs() {
        if (isNull()) {
            return nullptr;
        }
        return xcb_randr_get_screen_resources_current_crtcs(data());
    }
    inline xcb_randr_mode_info_t *modes() {
        if (isNull()) {
            return nullptr;
        }
        return xcb_randr_get_screen_resources_current_modes(data());
    }
};

XCB_WRAPPER(SetCrtcConfig, xcb_randr_set_crtc_config, xcb_randr_crtc_t, xcb_timestamp_t, xcb_timestamp_t, int16_t, int16_t, xcb_randr_mode_t, uint16_t, uint32_t, const xcb_randr_output_t*)
}

class ExtensionData
{
public:
    ExtensionData();
    int version;
    int eventBase;
    int errorBase;
    int majorOpcode;
    bool present;
    QByteArray name;
    QVector<QByteArray> opCodes;
    QVector<QByteArray> errorCodes;
};

class KWIN_EXPORT Extensions
{
public:
    bool isShapeAvailable() const {
        return m_shape.version > 0;
    }
    bool isShapeInputAvailable() const;
    int shapeNotifyEvent() const;
    bool hasShape(xcb_window_t w) const;
    bool isRandrAvailable() const {
        return m_randr.present;
    }
    int randrNotifyEvent() const;
    bool isDamageAvailable() const {
        return m_damage.present;
    }
    int damageNotifyEvent() const;
    bool isCompositeAvailable() const {
        return m_composite.version > 0;
    }
    bool isCompositeOverlayAvailable() const;
    bool isRenderAvailable() const {
        return m_render.version > 0;
    }
    bool isFixesAvailable() const {
        return m_fixes.version > 0;
    }
    int fixesCursorNotifyEvent() const;
    int fixesSelectionNotifyEvent() const;
    bool isFixesRegionAvailable() const;
    bool isSyncAvailable() const {
        return m_sync.present;
    }
    int syncAlarmNotifyEvent() const;
    QVector<ExtensionData> extensions() const;
    bool hasGlx() const {
        return m_glx.present;
    }
    int glxEventBase() const {
        return m_glx.eventBase;
    }
    int glxMajorOpcode() const {
        return m_glx.majorOpcode;
    }

    static Extensions *self();
    static void destroy();
private:
    Extensions();
    ~Extensions();
    void init();
    template <typename reply, typename T, typename F>
    void initVersion(T cookie, F f, ExtensionData *dataToFill);
    void extensionQueryReply(const xcb_query_extension_reply_t *extension, ExtensionData *dataToFill);

    ExtensionData m_shape;
    ExtensionData m_randr;
    ExtensionData m_damage;
    ExtensionData m_composite;
    ExtensionData m_render;
    ExtensionData m_fixes;
    ExtensionData m_sync;
    ExtensionData m_glx;

    static Extensions *s_self;
};

/**
 * This class is an RAII wrapper for an xcb_window_t. An xcb_window_t hold by an instance of this class
 * will be freed when the instance gets destroyed.
 *
 * Furthermore the class provides wrappers around some xcb methods operating on an xcb_window_t.
 *
 * For the cases that one is more interested in wrapping the xcb methods the constructor which takes
 * an existing window and the @ref reset method allow to disable the RAII functionality.
 */
class Window
{
public:
    /**
     * Takes over responsibility of @p window. If @p window is not provided an invalid Window is
     * created. Use @ref create to set an xcb_window_t later on.
     *
     * If @p destroy is @c true the window will be destroyed together with this object, if @c false
     * the window will be kept around. This is useful if you are not interested in the RAII capabilities
     * but still want to use a window like an object.
     *
     * @param window The window to manage.
     * @param destroy Whether the window should be destroyed together with the object.
     * @see reset
     */
    Window(xcb_window_t window = XCB_WINDOW_NONE, bool destroy = true);
    /**
     * Creates an xcb_window_t and manages it. It's a convenient method to create a window with
     * depth, class and visual being copied from parent and border being @c 0.
     * @param geometry The geometry for the window to be created
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     */
    Window(const QRect &geometry, uint32_t mask = 0, const uint32_t *values = nullptr, xcb_window_t parent = rootWindow());
    /**
     * Creates an xcb_window_t and manages it. It's a convenient method to create a window with
     * depth and visual being copied from parent and border being @c 0.
     * @param geometry The geometry for the window to be created
     * @param windowClass The window class
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     */
    Window(const QRect &geometry, uint16_t windowClass, uint32_t mask = 0, const uint32_t *values = nullptr, xcb_window_t parent = rootWindow());
    Window(const Window &other) = delete;
    ~Window();

    /**
     * Creates a new window for which the responsibility is taken over. If a window had been managed
     * before it is freed.
     *
     * Depth, class and visual are being copied from parent and border is @c 0.
     * @param geometry The geometry for the window to be created
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     */
    void create(const QRect &geometry, uint32_t mask = 0, const uint32_t *values = nullptr, xcb_window_t parent = rootWindow());
    /**
     * Creates a new window for which the responsibility is taken over. If a window had been managed
     * before it is freed.
     *
     * Depth and visual are being copied from parent and border is @c 0.
     * @param geometry The geometry for the window to be created
     * @param windowClass The window class
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     */
    void create(const QRect &geometry, uint16_t windowClass, uint32_t mask = 0, const uint32_t *values = nullptr, xcb_window_t parent = rootWindow());
    /**
     * Frees the existing window and starts to manage the new @p window.
     * If @p destroy is @c true the new managed window will be destroyed together with this
     * object or when reset is called again. If @p destroy is @c false the window will not
     * be destroyed. It is then the responsibility of the caller to destroy the window.
     */
    void reset(xcb_window_t window = XCB_WINDOW_NONE, bool destroy = true);
    /**
     * @returns @c true if a window is managed, @c false otherwise.
     */
    bool isValid() const;
    inline const QRect &geometry() const { return m_logicGeometry; }
    /**
     * Configures the window with a new geometry.
     * @param geometry The new window geometry to be used
     */
    void setGeometry(const QRect &geometry);
    void setGeometry(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void move(const QPoint &pos);
    void move(uint32_t x, uint32_t y);
    void resize(const QSize &size);
    void resize(uint32_t width, uint32_t height);
    void raise();
    void lower();
    void map();
    void unmap();
    void reparent(xcb_window_t parent, int x = 0, int y = 0);
    void changeProperty(xcb_atom_t property, xcb_atom_t type, uint8_t format, uint32_t length,
                        const void *data, uint8_t mode = XCB_PROP_MODE_REPLACE);
    void deleteProperty(xcb_atom_t property);
    void setBorderWidth(uint32_t width);
    void grabButton(uint8_t pointerMode, uint8_t keyboardmode,
                    uint16_t modifiers = XCB_MOD_MASK_ANY,
                    uint8_t button = XCB_BUTTON_INDEX_ANY,
                    uint16_t eventMask = XCB_EVENT_MASK_BUTTON_PRESS,
                    xcb_window_t confineTo = XCB_WINDOW_NONE,
                    xcb_cursor_t cursor = XCB_CURSOR_NONE,
                    bool ownerEvents = false);
    void ungrabButton(uint16_t modifiers = XCB_MOD_MASK_ANY, uint8_t button = XCB_BUTTON_INDEX_ANY);
    /**
     * Clears the window area. Same as xcb_clear_area with x, y, width, height being @c 0.
     */
    void clear();
    void setBackgroundPixmap(xcb_pixmap_t pixmap);
    void defineCursor(xcb_cursor_t cursor);
    void focus(uint8_t revertTo = XCB_INPUT_FOCUS_POINTER_ROOT, xcb_timestamp_t time = XCB_TIME_CURRENT_TIME);
    void selectInput(uint32_t events);
    void kill();
    operator xcb_window_t() const;
private:
    xcb_window_t doCreate(const QRect &geometry, uint16_t windowClass, uint32_t mask = 0, const uint32_t *values = nullptr, xcb_window_t parent = rootWindow());
    void destroy();
    xcb_window_t m_window;
    bool m_destroy;
    QRect m_logicGeometry;
};

inline
Window::Window(xcb_window_t window, bool destroy)
    : m_window(window)
    , m_destroy(destroy)
{
}

inline
Window::Window(const QRect &geometry, uint32_t mask, const uint32_t *values, xcb_window_t parent)
    : m_window(doCreate(geometry, XCB_COPY_FROM_PARENT, mask, values, parent))
    , m_destroy(true)
{
}

inline
Window::Window(const QRect &geometry, uint16_t windowClass, uint32_t mask, const uint32_t *values, xcb_window_t parent)
    : m_window(doCreate(geometry, windowClass, mask, values, parent))
    , m_destroy(true)
{
}

inline
Window::~Window()
{
    destroy();
}

inline
void Window::destroy()
{
    if (!isValid() || !m_destroy) {
        return;
    }
    xcb_destroy_window(connection(), m_window);
    m_window = XCB_WINDOW_NONE;
}

inline
bool Window::isValid() const
{
    return m_window != XCB_WINDOW_NONE;
}

inline
Window::operator xcb_window_t() const
{
    return m_window;
}

inline
void Window::create(const QRect &geometry, uint16_t windowClass, uint32_t mask, const uint32_t *values, xcb_window_t parent)
{
    destroy();
    m_window = doCreate(geometry, windowClass, mask, values, parent);
}

inline
void Window::create(const QRect &geometry, uint32_t mask, const uint32_t *values, xcb_window_t parent)
{
    create(geometry, XCB_COPY_FROM_PARENT, mask, values, parent);
}

inline
xcb_window_t Window::doCreate(const QRect &geometry, uint16_t windowClass, uint32_t mask, const uint32_t *values, xcb_window_t parent)
{
    m_logicGeometry = geometry;
    xcb_window_t w = xcb_generate_id(connection());
    xcb_create_window(connection(), XCB_COPY_FROM_PARENT, w, parent,
                      geometry.x(), geometry.y(), geometry.width(), geometry.height(),
                      0, windowClass, XCB_COPY_FROM_PARENT, mask, values);
    return w;
}

inline
void Window::reset(xcb_window_t window, bool shouldDestroy)
{
    destroy();
    m_window = window;
    m_destroy = shouldDestroy;
}

inline
void Window::setGeometry(const QRect &geometry)
{
    setGeometry(geometry.x(), geometry.y(), geometry.width(), geometry.height());
}

inline
void Window::setGeometry(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    m_logicGeometry.setRect(x, y, width, height);
    if (!isValid()) {
        return;
    }
    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[] = { x, y, width, height };
    xcb_configure_window(connection(), m_window, mask, values);
}

inline
void Window::move(const QPoint &pos)
{
    move(pos.x(), pos.y());
}

inline
void Window::move(uint32_t x, uint32_t y)
{
    m_logicGeometry.moveTo(x, y);
    if (!isValid()) {
        return;
    }
    moveWindow(m_window, x, y);
}

inline
void Window::resize(const QSize &size)
{
    resize(size.width(), size.height());
}

inline
void Window::resize(uint32_t width, uint32_t height)
{
    m_logicGeometry.setSize(QSize(width, height));
    if (!isValid()) {
        return;
    }
    const uint16_t mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[] = { width, height };
    xcb_configure_window(connection(), m_window, mask, values);
}

inline
void Window::raise()
{
    const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(connection(), m_window, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

inline
void Window::lower()
{
    lowerWindow(m_window);
}

inline
void Window::map()
{
    if (!isValid()) {
        return;
    }
    xcb_map_window(connection(), m_window);
}

inline
void Window::unmap()
{
    if (!isValid()) {
        return;
    }
    xcb_unmap_window(connection(), m_window);
}

inline
void Window::reparent(xcb_window_t parent, int x, int y)
{
    if (!isValid()) {
        return;
    }
    xcb_reparent_window(connection(), m_window, parent, x, y);
}

inline
void Window::changeProperty(xcb_atom_t property, xcb_atom_t type, uint8_t format, uint32_t length, const void *data, uint8_t mode)
{
    if (!isValid()) {
        return;
    }
    xcb_change_property(connection(), mode, m_window, property, type, format, length, data);
}

inline
void Window::deleteProperty(xcb_atom_t property)
{
    if (!isValid()) {
        return;
    }
    xcb_delete_property(connection(), m_window, property);
}

inline
void Window::setBorderWidth(uint32_t width)
{
    if (!isValid()) {
        return;
    }
    xcb_configure_window(connection(), m_window, XCB_CONFIG_WINDOW_BORDER_WIDTH, &width);
}

inline
void Window::grabButton(uint8_t pointerMode, uint8_t keyboardmode, uint16_t modifiers,
                        uint8_t button, uint16_t eventMask, xcb_window_t confineTo,
                        xcb_cursor_t cursor, bool ownerEvents)
{
    if (!isValid()) {
        return;
    }
    xcb_grab_button(connection(), ownerEvents, m_window, eventMask,
                    pointerMode, keyboardmode, confineTo, cursor, button, modifiers);
}

inline
void Window::ungrabButton(uint16_t modifiers, uint8_t button)
{
    if (!isValid()) {
        return;
    }
    xcb_ungrab_button(connection(), button, m_window, modifiers);
}

inline
void Window::clear()
{
    if (!isValid()) {
        return;
    }
    xcb_clear_area(connection(), false, m_window, 0, 0, 0, 0);
}

inline
void Window::setBackgroundPixmap(xcb_pixmap_t pixmap)
{
    if (!isValid()) {
        return;
    }
    const uint32_t values[] = {pixmap};
    xcb_change_window_attributes(connection(), m_window, XCB_CW_BACK_PIXMAP, values);
}

inline
void Window::defineCursor(xcb_cursor_t cursor)
{
    Xcb::defineCursor(m_window, cursor);
}

inline
void Window::focus(uint8_t revertTo, xcb_timestamp_t time)
{
    setInputFocus(m_window, revertTo, time);
}

inline
void Window::selectInput(uint32_t events)
{
    Xcb::selectInput(m_window, events);
}

inline
void Window::kill()
{
    xcb_kill_client(connection(), m_window);
}

// helper functions
static inline void moveResizeWindow(WindowId window, const QRect &geometry)
{
    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[] = {
        static_cast<uint32_t>(geometry.x()),
        static_cast<uint32_t>(geometry.y()),
        static_cast<uint32_t>(geometry.width()),
        static_cast<uint32_t>(geometry.height())
    };
    xcb_configure_window(connection(), window, mask, values);
}

static inline void moveWindow(xcb_window_t window, const QPoint& pos)
{
    moveWindow(window, pos.x(), pos.y());
}

static inline void moveWindow(xcb_window_t window, uint32_t x, uint32_t y)
{
    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    const uint32_t values[] = { x, y };
    xcb_configure_window(connection(), window, mask, values);
}

static inline void lowerWindow(xcb_window_t window)
{
    const uint32_t values[] = { XCB_STACK_MODE_BELOW };
    xcb_configure_window(connection(), window, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

static inline WindowId createInputWindow(const QRect &geometry, uint32_t mask, const uint32_t *values)
{
    WindowId window = xcb_generate_id(connection());
    xcb_create_window(connection(), 0, window, rootWindow(),
                      geometry.x(), geometry.y(), geometry.width(), geometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_ONLY,
                      XCB_COPY_FROM_PARENT, mask, values);
    return window;
}

static inline void restackWindows(const QVector<xcb_window_t> &windows)
{
    if (windows.count() < 2) {
        // only one window, nothing to do
        return;
    }
    for (int i=1; i<windows.count(); ++i) {
        const uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
        const uint32_t stackingValues[] = {
            windows.at(i-1),
            XCB_STACK_MODE_BELOW
        };
        xcb_configure_window(connection(), windows.at(i), mask, stackingValues);
    }
}

static inline void restackWindowsWithRaise(const QVector<xcb_window_t> &windows)
{
    if (windows.isEmpty()) {
        return;
    }
    const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(connection(), windows.first(), XCB_CONFIG_WINDOW_STACK_MODE, values);
    restackWindows(windows);
}

static inline int defaultDepth()
{
    static int depth = 0;
    if (depth != 0) {
        return depth;
    }
    int screen = Application::x11ScreenNumber();
    for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(connection()));
            it.rem;
            --screen, xcb_screen_next(&it)) {
        if (screen == 0) {
            depth = it.data->root_depth;
            break;
        }
    }
    return depth;
}

static inline xcb_rectangle_t fromQt(const QRect &rect)
{
    xcb_rectangle_t rectangle;
    rectangle.x = rect.x();
    rectangle.y = rect.y();
    rectangle.width  = rect.width();
    rectangle.height = rect.height();
    return rectangle;
}

static inline QVector<xcb_rectangle_t> regionToRects(const QRegion &region)
{
    QVector<xcb_rectangle_t> rects;
    rects.reserve(region.rectCount());
    for (const QRect &rect : region) {
        rects.append(Xcb::fromQt(rect));
    }
    return rects;
}

static inline void defineCursor(xcb_window_t window, xcb_cursor_t cursor)
{
    xcb_change_window_attributes(connection(), window, XCB_CW_CURSOR, &cursor);
}

static inline void setInputFocus(xcb_window_t window, uint8_t revertTo, xcb_timestamp_t time)
{
    xcb_set_input_focus(connection(), revertTo, window, time);
}

static inline void setTransientFor(xcb_window_t window, xcb_window_t transient_for_window)
{
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_TRANSIENT_FOR,
                        XCB_ATOM_WINDOW, 32, 1, &transient_for_window);
}

static inline void sync()
{
    auto *c = connection();
    const auto cookie = xcb_get_input_focus(c);
    xcb_generic_error_t *error = nullptr;
    ScopedCPointer<xcb_get_input_focus_reply_t> sync(xcb_get_input_focus_reply(c, cookie, &error));
    if (error) {
        free(error);
    }
}

void selectInput(xcb_window_t window, uint32_t events)
{
    xcb_change_window_attributes(connection(), window, XCB_CW_EVENT_MASK, &events);
}

/**
 * @brief Small helper class to encapsulate SHM related functionality.
 */
class Shm
{
public:
    Shm();
    ~Shm();
    int shmId() const;
    void *buffer() const;
    xcb_shm_seg_t segment() const;
    bool isValid() const;
    uint8_t pixmapFormat() const;
private:
    bool init();
    int m_shmId;
    void *m_buffer;
    xcb_shm_seg_t m_segment;
    bool m_valid;
    uint8_t m_pixmapFormat;
};

inline
void *Shm::buffer() const
{
    return m_buffer;
}

inline
bool Shm::isValid() const
{
    return m_valid;
}

inline
xcb_shm_seg_t Shm::segment() const
{
    return m_segment;
}

inline
int Shm::shmId() const
{
    return m_shmId;
}

inline
uint8_t Shm::pixmapFormat() const
{
    return m_pixmapFormat;
}

} // namespace X11

} // namespace KWin
#endif // KWIN_X11_UTILS_H
