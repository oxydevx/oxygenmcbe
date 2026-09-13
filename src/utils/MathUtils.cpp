#define GLM_ENABLE_EXPERIMENTAL
#include <Windows.h>
#include "MathUtils.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtx/rotate_vector.hpp>
#include <imgui.h>

float MathUtils::animate(float endPoint, float current, float speed) {
    if (speed < 0.0) speed = 0.0;
    else if (speed > 1.0) speed = 1.0;

    float dif = std::fmax(endPoint, current) - std::fmin(endPoint, current);
    float factor = dif * speed;
    return current + (endPoint > current ? factor : -factor);
}

float MathUtils::lerp(float a, float b, float t) {
    return a + t * (b - a);
}

glm::vec3 MathUtils::lerp(glm::vec3& a, glm::vec3& b, float t) {
    return a + t * (b - a);
}

ImVec4 MathUtils::lerp(ImVec4& a, ImVec4& b, float t) {
    return ImVec4(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), lerp(a.w, b.w, t));
}

ImVec2 MathUtils::lerp(ImVec2& a, ImVec2& b, float t) {
    return ImVec2(lerp(a.x, b.x, t), lerp(a.y, b.y, t));
}

float MathUtils::getRotationKeyOffset(bool allowStrafe) {
    bool w = GetAsyncKeyState('W') & 0x8000;
    bool s = GetAsyncKeyState('S') & 0x8000;
    bool a = GetAsyncKeyState('A') & 0x8000;
    bool d = GetAsyncKeyState('D') & 0x8000;

    bool isMoving = w || s || a || d;
    if (!isMoving) return 0;

    float yawOffset = 0;
    if (w && a && allowStrafe)
        yawOffset = -45;
    else if (w && d && allowStrafe)
        yawOffset = 45;
    else if (s && a && allowStrafe)
        yawOffset = -135;
    else if (s && d && allowStrafe)
        yawOffset = 135;
    else if (w)
        yawOffset = 0;
    else if (a && allowStrafe)
        yawOffset = -90;
    else if (s)
        yawOffset = -180;
    else if (d && allowStrafe)
        yawOffset = 90;
    else
        yawOffset = 0;

    return yawOffset;
}

glm::vec2 MathUtils::getMotion(float yaw, float speed, bool allowStrafe) {
    yaw += getRotationKeyOffset(allowStrafe) + 90;

    float calcYaw = glm::radians(yaw);

    bool w = GetAsyncKeyState('W') & 0x8000;
    bool s = GetAsyncKeyState('S') & 0x8000;
    bool a = GetAsyncKeyState('A') & 0x8000;
    bool d = GetAsyncKeyState('D') & 0x8000;

    if (!w && !s && !a && !d) return { 0, 0 };

    glm::vec2 motion;
    motion.x = std::cos(calcYaw) * speed;
    motion.y = std::sin(calcYaw) * speed;

    return motion;
}

Vec2 MathUtils::getMotionV(float yaw, float speed, bool allowStrafe) {
    glm::vec2 m = getMotion(yaw, speed, allowStrafe);
    return Vec2(m.x, m.y);
}

float MathUtils::clamp(float value, float min, float max) {
    return std::max(min, std::min(value, max));
}

float MathUtils::random(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

int MathUtils::random(int min, int max) {
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<> distr(min, max);
    return distr(eng);
}

float MathUtils::randomFloat(float min, float max) {
    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_real_distribution<float> distr(min, max);
    return distr(eng);
}

float MathUtils::wrap(float val, float min, float max) {
    return fmod(fmod(val - min, max - min) + (max - min), max - min) + min;
}

std::vector<glm::vec2> MathUtils::getBoxPoints(const AABB& aabb, const glmatrixf& matrix, const Vec3& origin, const Vec2& fov, const Vec2& displaySize) {
    Vec3 worldPoints[8] = {
        {aabb.minX(), aabb.minY(), aabb.minZ()},
        {aabb.minX(), aabb.minY(), aabb.maxZ()},
        {aabb.maxX(), aabb.minY(), aabb.minZ()},
        {aabb.maxX(), aabb.minY(), aabb.maxZ()},
        {aabb.minX(), aabb.maxY(), aabb.minZ()},
        {aabb.minX(), aabb.maxY(), aabb.maxZ()},
        {aabb.maxX(), aabb.maxY(), aabb.minZ()},
        {aabb.maxX(), aabb.maxY(), aabb.maxZ()}
    };

    std::vector<glm::vec2> points;
    points.reserve(8);
    for (const auto& wp : worldPoints) {
        Vec2 result = { 0, 0 };
        if (!matrix.OWorldToScreen(origin, wp, result, fov, displaySize)) return {};
        points.emplace_back(result.x, result.y);
    }

    if (points.size() < 3) return {};

    auto it = std::min_element(points.begin(), points.end(), [](const glm::vec2& a, const glm::vec2& b) {
        return a.x < b.x;
    });
    glm::vec2 start = *it;

    std::vector<glm::vec2> indices;
    indices.reserve(8);
    indices.push_back(start);

    glm::vec2 current = start;
    glm::vec2 lastDir = glm::vec2(0, -1);

    do {
        float smallestAngle = 2 * glm::pi<float>();
        glm::vec2 smallestDir = { 0, 0 };
        glm::vec2 smallestE = { 0, 0 };
        float lastDirAtan2 = atan2(lastDir.y, lastDir.x);

        for (const auto& t : points) {
            if (current == t) continue;

            glm::vec2 dir = t - current;
            float angle = atan2(dir.y, dir.x) - lastDirAtan2;
            if (angle > glm::pi<float>()) angle -= 2 * glm::pi<float>();
            else if (angle <= -glm::pi<float>()) angle += 2 * glm::pi<float>();

            if (angle >= 0 && angle < smallestAngle) {
                smallestAngle = angle;
                smallestDir = dir;
                smallestE = t;
            }
        }

        indices.push_back(smallestE);
        lastDir = smallestDir;
        current = smallestE;

    } while (current != start && indices.size() < 8);

    return indices;
}

std::vector<ImVec2> MathUtils::getImBoxPoints(const AABB& aabb, const glmatrixf& matrix, const Vec3& origin, const Vec2& fov, const Vec2& displaySize) {
    std::vector<glm::vec2> points = getBoxPoints(aabb, matrix, origin, fov, displaySize);
    std::vector<ImVec2> imPoints;
    imPoints.reserve(points.size());
    for (auto& point : points) {
        imPoints.emplace_back(point.x, point.y);
    }
    return imPoints;
}

glm::vec2 MathUtils::getRots(const glm::vec3& pEyePos, const glm::vec3& pTarget) {
    glm::vec3 delta = pTarget - pEyePos;
    float yaw = atan2(delta.z, delta.x);
    yaw = glm::degrees(yaw) - 90;
    yaw = wrap(yaw, -180, 180);
    float pitch = atan2(delta.y, sqrt(delta.x * delta.x + delta.z * delta.z)) * 180.0f / glm::pi<float>();

    return { -pitch, yaw };
}

glm::vec2 MathUtils::getRots(const glm::vec3& pEyePos, const AABB& target) {
    return getRots(pEyePos, glm::vec3(target.getCenter().x, target.getCenter().y, target.getCenter().z));
}

float MathUtils::snapYaw(float yaw) {
    if (yaw < -135 || yaw > 135) return -180;
    if (yaw < -45) return -90;
    if (yaw < 45) return 0;
    if (yaw < 135) return 90;
    return 180;
}

glm::vec2 MathUtils::getMovement(bool w, bool a, bool s, bool d) {
    glm::vec2 ret = glm::vec2(0, 0);
    float forward = 0.0f;
    float side = 0.0f;

    if (!w && !a && !s && !d)
        return ret;

    static constexpr float forwardF = 1;
    static constexpr float sideF = 0.7071067691f;

    if (w) {
        if (!a && !d)
            forward = forwardF;
        if (a) {
            forward = sideF;
            side = sideF;
        }
        else if (d) {
            forward = sideF;
            side = -sideF;
        }
    }
    else if (s) {
        if (!a && !d)
            forward = -forwardF;
        if (a) {
            forward = -sideF;
            side = sideF;
        }
        else if (d) {
            forward = -sideF;
            side = -sideF;
        }
    }
    else if (!w && !s) {
        if (!a && d) side = -forwardF;
        else side = forwardF;
    }

    ret.x = side;
    ret.y = forward;
    return ret;
}

bool MathUtils::rayIntersectsAABB(glm::vec3 rayPos, glm::vec3 rayEnd, glm::vec3 hitboxMin, glm::vec3 hitboxMax) {
    glm::vec3 t0 = (hitboxMin - rayPos) / (rayEnd - rayPos);
    glm::vec3 t1 = (hitboxMax - rayPos) / (rayEnd - rayPos);

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float tminmax = glm::max(tmin.x, glm::max(tmin.y, tmin.z));
    float tmaxmin = glm::min(tmax.x, glm::min(tmax.y, tmax.z));

    return tminmax <= tmaxmin;
}

bool MathUtils::worldToScreen(const Vec3& worldPos, ImVec2& screenPos, const glmatrixf& matrix, const Vec3& origin, const Vec2& fov, const Vec2& displaySize) {
    Vec2 result;
    bool success = matrix.OWorldToScreen(origin, worldPos, result, fov, displaySize);
    screenPos = ImVec2(result.x, result.y);
    return success;
}
