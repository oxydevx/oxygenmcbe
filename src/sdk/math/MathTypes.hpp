#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include "../MemoryUtil.hpp"
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/matrix_transform.hpp>

static constexpr float PI_F = 3.1415926535f;

struct Vec2 final {
    float x, y;
    constexpr Vec2() : x(0.f), y(0.f) {}
    constexpr Vec2(float x, float y) : x(x), y(y) {}
    constexpr Vec2 operator-(Vec2 const& right) const { return { x - right.x, y - right.y }; }
    constexpr Vec2 operator+(Vec2 const& right) const { return { x + right.x, y + right.y }; }
    constexpr Vec2 operator/(Vec2 const& right) const { return { x / right.x, y / right.y }; }
    constexpr Vec2 operator*(Vec2 const& right) const { return { x * right.x, y * right.y }; }
    constexpr Vec2 operator*(float s) const { return { x * s, y * s }; }
    bool operator==(Vec2 const& right) const { return x == right.x && y == right.y; }
    bool operator!=(Vec2 const& right) const { return x != right.x || y != right.y; }
    [[nodiscard]] float magnitude() const { return std::sqrt(x * x + y * y); }
    [[nodiscard]] float distance(Vec2 const& o) const { return std::sqrt((x - o.x) * (x - o.x) + (y - o.y) * (y - o.y)); }
};

struct Vec3 final {
    float x, y, z;
    constexpr Vec3() : x(0.f), y(0.f), z(0.f) {}
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    constexpr Vec3 operator-(Vec3 const& right) const { return { x - right.x, y - right.y, z - right.z }; }
    constexpr Vec3 operator+(Vec3 const& right) const { return { x + right.x, y + right.y, z + right.z }; }
    constexpr Vec3 operator*(float right) const { return { x * right, y * right, z * right }; }
    constexpr Vec3 operator/(float right) const { return { x / right, y / right, z / right }; }
    [[nodiscard]] float distance(Vec3 const& o) const { return std::sqrt(std::pow(x - o.x, 2) + std::pow(y - o.y, 2) + std::pow(z - o.z, 2)); }
    [[nodiscard]] Vec3 normalized() const {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len < 0.0001f) return { 0.f, 0.f, 0.f };
        return { x / len, y / len, z / len };
    }
    [[nodiscard]] float length() const { return std::sqrt(x * x + y * y + z * z); }
    operator glm::vec3() const { return glm::vec3(x, y, z); }
    Vec3(glm::vec3 const& v) : x(v.x), y(v.y), z(v.z) {}
};

struct Vec4 final {
    float x, y, z, w;
    constexpr Vec4() : x(0.f), y(0.f), z(0.f), w(0.f) {}
    constexpr Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

struct Vec3i final {
    int x, y, z;
    constexpr Vec3i() : x(0), y(0), z(0) {}
    constexpr Vec3i(int x, int y, int z) : x(x), y(y), z(z) {}
    constexpr Vec3i(Vec3 const& vec) : x(static_cast<int>(vec.x)), y(static_cast<int>(vec.y)), z(static_cast<int>(vec.z)) {}
};

using BlockPos = Vec3i;

struct AABB final {
    Vec3 lower, higher;
    constexpr AABB() : lower(), higher() {}
    constexpr AABB(Vec3 lower, Vec3 higher) : lower(lower), higher(higher) {}
    Vec3 getCenter() const { return (lower + higher) * 0.5f; }
    Vec3 closestPoint(Vec3 const& to) const {
        return {
            std::clamp(to.x, lower.x, higher.x),
            std::clamp(to.y, lower.y, higher.y),
            std::clamp(to.z, lower.z, higher.z),
        };
    }
    [[nodiscard]] float minX() const { return lower.x; }
    [[nodiscard]] float minY() const { return lower.y; }
    [[nodiscard]] float minZ() const { return lower.z; }
    [[nodiscard]] float maxX() const { return higher.x; }
    [[nodiscard]] float maxY() const { return higher.y; }
    [[nodiscard]] float maxZ() const { return higher.z; }
    std::array<Vec3, 8> getCorners() const {
        return { {
            {minX(), minY(), minZ()},
            {maxX(), minY(), minZ()},
            {maxX(), minY(), maxZ()},
            {minX(), minY(), maxZ()},
            {minX(), maxY(), minZ()},
            {maxX(), maxY(), minZ()},
            {maxX(), maxY(), maxZ()},
            {minX(), maxY(), maxZ()},
        } };
    }
};

#ifdef RGB
#undef RGB
#endif

struct Color {
    float r, g, b, a;
    constexpr Color(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}
    constexpr Color() : r(0.f), g(0.f), b(0.f), a(1.f) {}
    Color(float pFloat[4]) : r(pFloat[0]), g(pFloat[1]), b(pFloat[2]), a(pFloat[3]) {}
    static constexpr Color RGB(int r, int g, int b, int a = 255) { return Color(r / 255.f, g / 255.f, b / 255.f, a / 255.f); }
    [[nodiscard]] unsigned int toABGR() const {
        unsigned int ri = static_cast<unsigned int>(r * 255.f) & 0xFF;
        unsigned int gi = static_cast<unsigned int>(g * 255.f) & 0xFF;
        unsigned int bi = static_cast<unsigned int>(b * 255.f) & 0xFF;
        unsigned int ai = static_cast<unsigned int>(a * 255.f) & 0xFF;
        return (ai << 24) | (bi << 16) | (gi << 8) | ri;
    }
};

struct HSV {
    float h, s, v;
    constexpr HSV() : h(0.f), s(0.f), v(0.f) {}
    constexpr HSV(float h, float s, float v) : h(h), s(s), v(v) {}
};

struct glmatrixf : public glm::mat4 {
    using glm::mat4::mat4;
    glmatrixf() : glm::mat4(1.0f) {}
    glmatrixf(glm::mat4 const& m) : glm::mat4(m) {}
    bool OWorldToScreen(Vec3 origin, Vec3 pos, Vec2& screen, Vec2 fov, Vec2 displaySize) const {
        pos = pos - origin;
        float x = glm::dot((*this)[0], glm::vec4(pos.x, pos.y, pos.z, 1.0f));
        float y = glm::dot((*this)[1], glm::vec4(pos.x, pos.y, pos.z, 1.0f));
        float z = glm::dot((*this)[2], glm::vec4(pos.x, pos.y, pos.z, 1.0f));
        if (z > 0) return false;
        float mX = displaySize.x / 2.0F;
        float mY = displaySize.y / 2.0F;
        screen.x = mX + (mX * x / -z * fov.x);
        screen.y = mY - (mY * y / -z * fov.y);
        if (screen.x > displaySize.x * 2 || screen.y > displaySize.y * 2) return false;
        return true;
    }
    Vec2 WorldToScreen(Vec3 pos, int width, int height) {
        glm::vec4 clip = (*this) * glm::vec4(pos.x, pos.y, pos.z, 1.0f);
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        Vec2 result;
        result.x = (ndc.x + 1.0f) * 0.5f * width;
        result.y = (1.0f - ndc.y) * 0.5f * height;
        return result;
    }
};

struct FrameTransform {
    glmatrixf mMatrix{};
    Vec3 mOrigin{};
    Vec3 mPlayerPos{};
    Vec2 mFov{};
};

namespace Oxygen {
    using Vec2 = ::Vec2;
    using Vec3 = ::Vec3;

    inline Vec2 CalcAngle(Vec3 const& from, Vec3 const& to) {
        Vec3 d = to - from;
        float hyp = std::sqrt(d.x * d.x + d.z * d.z);
        float yaw = std::atan2(d.x, -d.z) * (180.0f / PI_F);
        float pitch = std::atan2(-d.y, hyp) * (180.0f / PI_F);
        return Vec2{ yaw, pitch };
    }
}
