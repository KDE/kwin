/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QVector2D>
#include <QVector3D>

namespace KWin
{

/**
 * This file contains a template decorator for geometric types that encodes the
 * coordinate system into the type. The purpose is to prevent accidentally using
 * a geometric type in a certain coordinate system in code that expects a
 * different coordinate system. Instead of silently accepting these situations,
 * the compiler will prevent it and an explicit conversion has to be done.
 *
 * Example usage is as follows:
 * ```
 * Vector<QVector2D, LogicalCoordinates> logicalPosition(10, 10);
 * Vector<QVector2D, DeviceCoordinates> devicePosition;
 *
 * // This will produce a compiler error:
 * // devicePosition = logicalPosition
 *
 * // Converting from logical to device coordiantes requires a scale factor:
 * const auto scale = 1.5
 * // With that we can do an explicit conversion to the device coordinate system.
 * devicePosition = logicalPosition.convertTo<DeviceCoordinates>(scale);
 * // devicePosition now contains the coordinates (15, 15).
 * ```
 *
 * This decorator should work for any geometric type that can be constructed
 * by providing a set of plain numbers as constructor arguments, such as
 * individual values for a vector's axis values. Note that conversion between
 * geometric types is not currently supported.
 *
 * Coordinate systems are simple empty structs used as type tags. The template
 * has two parameters, one for the underlying geometric type and one for
 * the coordinate system.
 *
 * One helper template is defined, this provides a method to convert between two
 * coordinate systems. To define a conversion, this template should be
 * specialized for the input and output types.
 */

/**
 * Coordinate system for logical coordinates.
 */
struct LogicalCoordinates;
/**
 * Coordinate system for device coordinates;
 */
struct DeviceCoordinates;
/**
 * Coordinate system for UV (texture) coordinates.
 */
struct UvCoordinates;
/**
 * Coordinate system for when the coordinate type is not known.
 */
struct UnknownCoordinates;

template<typename VectorType, typename System>
class Vector;

// Convenience aliases.
template<typename System>
using Vector2D = Vector<QVector2D, System>;

template<typename System>
using Vector3D = Vector<QVector3D, System>;

using LogicalVector2D = Vector2D<LogicalCoordinates>;
using DeviceVector2D = Vector2D<DeviceCoordinates>;
using UvCoordinate = Vector2D<UvCoordinates>;

namespace Detail
{
template<typename VectorType, typename FromSystem, typename ToSystem>
struct CoordinateConverter;
}

/*
 * The actual decorator template.
 */
template<typename VectorType, typename System>
class Vector : public VectorType
{
public:
    /*
     * Prevent conversion from plain VectorType to this type.
     * Without this, VectorType can be implicitly converted and the compiler
     * will not produce errors for incorrect operations.
     */
    Vector(const VectorType &) = delete;
    Vector(VectorType &&) = delete;

    /**
     * Component-wise constructor.
     *
     * This constructs a vector from a number of numeric components. It assumes
     * the underlying VectorType has a constructor that supports this. It is
     * only enabled if all the parameters are plain numeric types, to prevent
     * Vector(VectorType) from working.
     */
    template<typename... Args,
             typename = std::enable_if_t<std::conjunction_v<std::is_arithmetic<std::remove_reference_t<Args>>...>>>
    Vector(Args &&...args)
        : VectorType(std::forward<Args>(args)...)
    {
    }

    /**
     * Convenience non-static version of convertTo that converts the current
     * vector to the destination coordinate system.
     */
    template<typename ToSystem, typename... Args>
    inline Vector<VectorType, ToSystem> convertTo(Args &&...args) const
    {
        return Vector<VectorType, System>::convertTo(this, args);
    }

    /**
     * Convert a vector in the current coordinate system to a different coordinate system.
     */
    template<typename ToSystem, typename... Args>
    static inline Vector<VectorType, ToSystem> convertTo(const Vector<VectorType, System> &vector, Args &&...args)
    {
        if constexpr (std::is_same_v<System, ToSystem>) {
            return vector;
        } else {
            return Detail::CoordinateConverter<VectorType, System, ToSystem>::convert(vector, std::forward<Args>(args)...);
        }
    }

    /**
     * Convert a vector in a different coordinate system to the current coordinate system.
     */
    template<typename FromSystem, typename... Args>
    static inline Vector<VectorType, System> convertFrom(const Vector<VectorType, FromSystem> &vector, Args &&...args)
    {
        if constexpr (std::is_same_v<System, FromSystem>) {
            return vector;
        } else {
            return Detail::CoordinateConverter<VectorType, FromSystem, System>::convert(vector, std::forward<Args>(args)...);
        }
    }

    /**
     * Convert a plain VectorType to a vector of the current coordinate system.
     */
    static inline Vector<VectorType, System> convertFrom(const VectorType &vector)
    {
        return Detail::CoordinateConverter<VectorType, UnknownCoordinates, System>::convert(vector);
    }
};

/*
 * Helper specialization that allows converting a plain VectorType into a
 * Vector that is explicitly marked as having an unknown coordinate system.
 *
 * Mostly this enables the copy and move constructors accepting VectorType so
 * we can do a more simple conversion rather than having to go through
 * individual components.
 */
template<typename VectorType>
class Vector<VectorType, UnknownCoordinates> : public VectorType
{
public:
    Vector(const VectorType &other)
    {
        *this = other;
    }

    Vector(VectorType &&other)
    {
        std::swap(*this, other);
    }
};

/*
 * Mathematical operator overloads.
 *
 * Assuming VectorType has these implemented, doing
 * `Vector<VectorType> operator Vector<VectorType>` results in a VectorType
 * rather than Vector<VectorType>. To fix that we need to overload those
 * operators with the right types.
 *
 * We only want to enable these operators when they're either the exact same
 * type as the vector or when they're plain scalars. Anything else risks mixing
 * coordinate systems.
 */
template<typename VectorType, typename System>
Vector<VectorType, System> operator*(const Vector<VectorType, System> &vector, const Vector<VectorType, System> &other)
{
    // static_cast to prevent recursion
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) * static_cast<VectorType>(other));
}

template<typename VectorType, typename System>
Vector<VectorType, System> operator*(const Vector<VectorType, System> &vector, double other)
{
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) * other);
}

template<typename VectorType, typename System>
Vector<VectorType, System> operator/(const Vector<VectorType, System> &vector, const Vector<VectorType, System> &other)
{
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) / static_cast<VectorType>(other));
}

template<typename VectorType, typename System>
Vector<VectorType, System> operator/(const Vector<VectorType, System> &vector, double other)
{
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) / other);
}

template<typename VectorType, typename System>
Vector<VectorType, System> operator+(const Vector<VectorType, System> &vector, const Vector<VectorType, System> &other)
{
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) + static_cast<VectorType>(other));
}

template<typename VectorType, typename System>
Vector<VectorType, System> operator+(const Vector<VectorType, System> &vector, double other)
{
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) + other);
}

template<typename VectorType, typename System>
Vector<VectorType, System> operator-(const Vector<VectorType, System> &vector, const Vector<VectorType, System> &other)
{
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) - static_cast<VectorType>(other));
}

template<typename VectorType, typename System>
Vector<VectorType, System> operator-(const Vector<VectorType, System> &vector, double other)
{
    return Vector<VectorType, System>::convertFrom(static_cast<VectorType>(vector) - other);
}

namespace Detail
{
/*
 * Helper struct for conversion between different coordinate systems.
 *
 * This struct should be specialized to define coordinate system conversions.
 */
template<typename VectorType, typename FromSystem, typename ToSystem>
struct CoordinateConverter
{
    /*
     * Conversion method that performs the actual conversion. This method should
     * be specialised to implement the actual conversion. These specialisations
     * can add extra parameters as requirements for the conversion.
     */
    static Vector<VectorType, ToSystem> convert(const Vector<VectorType, FromSystem> &vector);
};

/*
 * Template specialization for converting a decorated QVector2D from device
 * coordinates to logical coordinates. This conversion requires a scaling factor.
 */
template<>
struct CoordinateConverter<QVector2D, DeviceCoordinates, LogicalCoordinates>
{
    static Vector<QVector2D, LogicalCoordinates> convert(const Vector<QVector2D, DeviceCoordinates> &vector, qreal scale)
    {
        return Vector<QVector2D, LogicalCoordinates>(vector.x() / scale, vector.y() / scale);
    }
};

/*
 * Template specialization for converting a decorated QVector2D from logical
 * coordinates to device coordinates. This conversion requires a scaling factor.
 */
template<>
struct CoordinateConverter<QVector2D, LogicalCoordinates, DeviceCoordinates>
{
    static Vector<QVector2D, DeviceCoordinates> convert(const Vector<QVector2D, LogicalCoordinates> &vector, qreal scale)
    {
        return Vector<QVector2D, DeviceCoordinates>(vector.x() * scale, vector.y() * scale);
    }
};

/**
 * Partial template specialization for converting a QVector2D in unknown
 * coordinates to a known coordinate system. This effectively reinterprets the
 * vector as being in the target coordinate system.
 */
template<typename ToSystem>
struct CoordinateConverter<QVector2D, UnknownCoordinates, ToSystem>
{
    static Vector<QVector2D, ToSystem> convert(const Vector<QVector2D, UnknownCoordinates> &vector)
    {
        return Vector<QVector2D, ToSystem>(vector.x(), vector.y());
    }
};

}

}
