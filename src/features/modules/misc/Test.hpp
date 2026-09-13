#pragma once

#include "../../Feature.hpp"

class Test : public Feature {
public:
    Test();
    void OnEvent()    override;
    void OnEnabled()  override;
    void OnDisabled() override;

    bool IsSmoothAnimationEnabled() const { return m_smoothAnimation; }
    bool IsMotionBlurEnabled() const { return m_motionBlur; }

private:
    bool m_smoothAnimation = false;
    bool m_motionBlur = false;
};