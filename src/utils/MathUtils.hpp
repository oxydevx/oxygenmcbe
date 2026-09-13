#pragma once

#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <imgui.h>
#include "../sdk/math/MathTypes.hpp"

struct FrameTransform;

class MathUtils {
public:
    static float animate(float endPoint, float current, float speed);
    static float lerp(float a, float b, float t);
    static glm::vec3 lerp(glm::vec3& a, glm::vec3& b, float t);
    static ImVec4 lerp(ImVec4& a, ImVec4& b, float t);
    static ImVec2 lerp(ImVec2& a, ImVec2& b, float t);

    static float getRotationKeyOffset(bool allowStrafe = true);
    static glm::vec2 getMotion(float yaw, float speed, bool allowStrafe = true);
    static Vec2 getMotionV(float yaw, float speed, bool allowStrafe = true);

    template <typename T>
    static T clamp(T value, T min, T max) {
        return std::max(min, std::min(value, max));
    }
    static float clamp(float value, float min, float max);

    static float random(float min, float max);
    static int random(int min, int max);
    static float randomFloat(float min, float max);
    static float wrap(float val, float min, float max);

    static std::vector<glm::vec2> getBoxPoints(const AABB& aabb, const glmatrixf& matrix, const Vec3& origin, const Vec2& fov, const Vec2& displaySize);
    static std::vector<ImVec2> getImBoxPoints(const AABB& aabb, const glmatrixf& matrix, const Vec3& origin, const Vec2& fov, const Vec2& displaySize);

    static glm::vec2 getRots(const glm::vec3& pEyePos, const glm::vec3& pTarget);
    static glm::vec2 getRots(const glm::vec3& pEyePos, const AABB& target);
    static float snapYaw(float yaw);
    static glm::vec2 getMovement(bool w, bool a, bool s, bool d);
    static bool rayIntersectsAABB(glm::vec3 rayPos, glm::vec3 rayEnd, glm::vec3 hitboxMin, glm::vec3 hitboxMax);

    static bool worldToScreen(const Vec3& worldPos, ImVec2& screenPos, const glmatrixf& matrix, const Vec3& origin, const Vec2& fov, const Vec2& displaySize);
};
