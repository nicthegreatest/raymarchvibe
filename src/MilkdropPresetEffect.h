
#pragma once

#include "ShaderEffect.h"
#include <string>
#include <vector>
#include <map>

// Forward declarations for projectm-eval types
struct prjm_eval_context_;
typedef struct prjm_eval_context_ prjm_eval_context;
struct prjm_eval_exptreenode;

class MilkdropPresetEffect : public ShaderEffect {
public:
    MilkdropPresetEffect(const std::string& presetName, const std::string& perFrameCode, const std::string& perPixelCode);
    ~MilkdropPresetEffect() override;

    void Update(float t) override;
    void Bind(int textureUnit);

private:
    void CompileAndLink();
    void ParsePerFrameCode();

    std::string m_presetName;
    std::string m_perFrameCode;
    std::string m_perPixelCode;

    // Per-frame variables
    float m_time;
    float m_fps;
    float m_frame;
    float m_progress;
    float m_bass;
    float m_mid;
    float m_treb;
    float m_bass_att;
    float m_mid_att;
    float m_treb_att;

    std::map<std::string, float> m_q_vars;
    std::map<std::string, float> m_t_vars;

    prjm_eval_context* m_eval_context;
    prjm_eval_exptreenode* m_per_frame_ast;
};
