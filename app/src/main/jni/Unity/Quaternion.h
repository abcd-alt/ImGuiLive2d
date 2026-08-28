
#pragma once

#define _USE_MATH_DEFINES

#include <math.h>
#include <iostream>
#include "Vector3.hpp"

#define SMALL_float 0.0000000001

struct Quaternion {
    float x;
    float y;
    float z;
    float w;


    /**
     * Constructors.
     */
    Quaternion()
            : x(0), y(0), z(0), w(0) {}

    Quaternion(float x1, float y1, float z1, float w1) : x(x1), y(y1), z(z1), w(w1) {}

    Quaternion(const Quaternion &v);

    ~Quaternion();

    inline Quaternion(Vector3 vector, float scalar);


    /**
     * Constants for common quaternions.
     */
    static inline Quaternion Identity();

    /**
     * Create a quaternion rotation which rotates "fromVector" to "toVector".
     * @param fromVector: The vector from which to start the rotation.
     * @param toVector: The vector at which to end the rotation.
     * @return: A new quaternion.
     */
    static inline Quaternion FromToRotation(Vector3 fromVector,
                                            Vector3 toVector);

    static inline Quaternion Normalized(Quaternion rotation);

    static inline float Norm(Quaternion rotation);

    /**
     * Creates a rotation with the specified forward direction. This is the
     * same as calling LookRotation with (0, 1, 0) as the upwards vector.
     * The output is undefined for parallel vectors.
     * @param forward: The forward direction to look toward.
     * @return: A new quaternion.
     */
    static inline Quaternion LookRotation(Vector3 forward);

    /**
     * Creates a rotation with the specified forward and upwards directions.
     * The output is undefined for parallel vectors.
     * @param forward: The forward direction to look toward.
     * @param upwards: The direction to treat as up.
     * @return: A new quaternion.
     */
    static inline Quaternion LookRotation(Vector3 forward, Vector3 upwards);

    /**
     * Operator overloading.
     */
    inline struct Quaternion &operator+=(const float rhs);

    inline struct Quaternion &operator-=(const float rhs);

    inline struct Quaternion &operator*=(const float rhs);

    inline struct Quaternion &operator/=(const float rhs);

    inline struct Quaternion &operator+=(const Quaternion rhs);

    inline struct Quaternion &operator-=(const Quaternion rhs);

    inline struct Quaternion &operator*=(const Quaternion rhs);
};

inline Quaternion operator-(Quaternion rhs);

inline Quaternion operator+(Quaternion lhs, const float rhs);

inline Quaternion operator-(Quaternion lhs, const float rhs);

inline Quaternion operator*(Quaternion lhs, const float rhs);

inline Quaternion operator/(Quaternion lhs, const float rhs);

inline Quaternion operator+(const float lhs, Quaternion rhs);

inline Quaternion operator-(const float lhs, Quaternion rhs);

inline Quaternion operator*(const float lhs, Quaternion rhs);

inline Quaternion operator/(const float lhs, Quaternion rhs);

inline Quaternion operator+(Quaternion lhs, const Quaternion rhs);

inline Quaternion operator-(Quaternion lhs, const Quaternion rhs);

inline Quaternion operator*(Quaternion lhs, const Quaternion rhs);

inline Vector3 operator*(Quaternion lhs, const Vector3 rhs);

inline bool operator==(const Quaternion lhs, const Quaternion rhs);

inline bool operator!=(const Quaternion lhs, const Quaternion rhs);


/*******************************************************************************
 * Implementation
 */

Quaternion::Quaternion(const Quaternion &v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

Quaternion::~Quaternion() {}


Quaternion Quaternion::Identity() { return Quaternion(0, 0, 0, 1); }

Quaternion::Quaternion(Vector3 vector, float scalar) : x(vector.X),
                                                       y(vector.Y), z(vector.Z), w(scalar) {}

float Quaternion::Norm(Quaternion rotation) {
    return sqrt(rotation.x * rotation.x +
                rotation.y * rotation.y +
                rotation.z * rotation.z +
                rotation.w * rotation.w);
}

Quaternion Quaternion::Normalized(Quaternion rotation) {
    return rotation / Norm(rotation);
}

Quaternion Quaternion::LookRotation(Vector3 forward) {
    return LookRotation(forward, Vector3(0, 1, 0));
}

Quaternion Quaternion::FromToRotation(Vector3 fromVector, Vector3 toVector) {
    float dot = Vector3::Dot(fromVector, toVector);
    float k = sqrt(Vector3::SqrMagnitude(fromVector) *
                   Vector3::SqrMagnitude(toVector));
    if (fabs(dot / k + 1) < 0.00001) {
        Vector3 ortho = Vector3::Orthogonal(fromVector);
        return Quaternion(Vector3::Normalized(ortho), 0);
    }
    Vector3 cross = Vector3::Cross(fromVector, toVector);
    return Normalized(Quaternion(cross, dot + k));
}

Quaternion Quaternion::LookRotation(Vector3 forward, Vector3 upwards) {
    // Normalize inputs
    forward = Vector3::Normalized(forward);
    upwards = Vector3::Normalized(upwards);
    // Don't allow zero vectors
    if (Vector3::SqrMagnitude(forward) < SMALL_float ||
        Vector3::SqrMagnitude(upwards) < SMALL_float)
        return Quaternion::Identity();
    // Handle alignment with up direction
    if (1 - fabs(Vector3::Dot(forward, upwards)) < SMALL_float)
        return FromToRotation(Vector3::Forward(), forward);
    // Get orthogonal vectors
    Vector3 right = Vector3::Normalized(Vector3::Cross(upwards, forward));
    upwards = Vector3::Cross(forward, right);
    // Calculate rotation
    Quaternion quaternion;
    float radicand = right.X + upwards.Y + forward.Z;
    if (radicand > 0) {
        quaternion.w = sqrt(1.0 + radicand) * 0.5;
        float recip = 1.0 / (4.0 * quaternion.w);
        quaternion.x = (upwards.Z - forward.Y) * recip;
        quaternion.y = (forward.X - right.Z) * recip;
        quaternion.z = (right.Y - upwards.X) * recip;
    } else if (right.X >= upwards.Y && right.X >= forward.Z) {
        quaternion.x = sqrt(1.0 + right.X - upwards.Y - forward.Z) * 0.5;
        float recip = 1.0 / (4.0 * quaternion.x);
        quaternion.w = (upwards.Z - forward.Y) * recip;
        quaternion.z = (forward.X + right.Z) * recip;
        quaternion.y = (right.Y + upwards.X) * recip;
    } else if (upwards.Y > forward.Z) {
        quaternion.y = sqrt(1.0 - right.X + upwards.Y - forward.Z) * 0.5;
        float recip = 1.0 / (4.0 * quaternion.y);
        quaternion.z = (upwards.Z + forward.Y) * recip;
        quaternion.w = (forward.X - right.Z) * recip;
        quaternion.x = (right.Y + upwards.X) * recip;
    } else {
        quaternion.z = sqrt(1.0 - right.X - upwards.Y + forward.Z) * 0.5;
        float recip = 1.0 / (4.0 * quaternion.z);
        quaternion.y = (upwards.Z + forward.Y) * recip;
        quaternion.x = (forward.X + right.Z) * recip;
        quaternion.w = (right.Y - upwards.X) * recip;
    }
    return quaternion;
}

struct Quaternion &Quaternion::operator+=(const float rhs) {
    x += rhs;
    y += rhs;
    z += rhs;
    w += rhs;
    return *this;
}

struct Quaternion &Quaternion::operator-=(const float rhs) {
    x -= rhs;
    y -= rhs;
    z -= rhs;
    w -= rhs;
    return *this;
}

struct Quaternion &Quaternion::operator*=(const float rhs) {
    x *= rhs;
    y *= rhs;
    z *= rhs;
    w *= rhs;
    return *this;
}

struct Quaternion &Quaternion::operator/=(const float rhs) {
    x /= rhs;
    y /= rhs;
    z /= rhs;
    w /= rhs;
    return *this;
}

struct Quaternion &Quaternion::operator+=(const Quaternion rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    w += rhs.w;
    return *this;
}

struct Quaternion &Quaternion::operator-=(const Quaternion rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    w -= rhs.w;
    return *this;
}

struct Quaternion &Quaternion::operator*=(const Quaternion rhs) {
    Quaternion q;
    q.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
    q.x = x * rhs.w + w * rhs.x + y * rhs.z - z * rhs.y;
    q.y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
    q.z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
    *this = q;
    return *this;
}

Quaternion operator-(Quaternion rhs) { return rhs * -1; }

Quaternion operator+(Quaternion lhs, const float rhs) { return lhs += rhs; }

Quaternion operator-(Quaternion lhs, const float rhs) { return lhs -= rhs; }

Quaternion operator*(Quaternion lhs, const float rhs) { return lhs *= rhs; }

Quaternion operator/(Quaternion lhs, const float rhs) { return lhs /= rhs; }

Quaternion operator+(const float lhs, Quaternion rhs) { return rhs += lhs; }

Quaternion operator-(const float lhs, Quaternion rhs) { return rhs -= lhs; }

Quaternion operator*(const float lhs, Quaternion rhs) { return rhs *= lhs; }

Quaternion operator/(const float lhs, Quaternion rhs) { return rhs /= lhs; }

Quaternion operator+(Quaternion lhs, const Quaternion rhs) {
    return lhs += rhs;
}

Quaternion operator-(Quaternion lhs, const Quaternion rhs) {
    return lhs -= rhs;
}

Quaternion operator*(Quaternion lhs, const Quaternion rhs) {
    return lhs *= rhs;
}

Vector3 operator*(Quaternion lhs, const Vector3 rhs) {
    Vector3 u = Vector3(lhs.x, lhs.y, lhs.z);
    float s = lhs.w;
    return u * (Vector3::Dot(u, rhs) * 2)
           + rhs * (s * s - Vector3::Dot(u, u))
           + Vector3::Cross(u, rhs) * (2.0 * s);
}

bool operator==(const Quaternion lhs, const Quaternion rhs) {
    return lhs.x == rhs.x &&
           lhs.y == rhs.y &&
           lhs.z == rhs.z &&
           lhs.w == rhs.w;
}

bool operator!=(const Quaternion lhs, const Quaternion rhs) {
    return !(lhs == rhs);
}
