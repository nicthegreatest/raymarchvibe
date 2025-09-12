#pragma once

#include "Effect.h"
#include "glad/glad.h"
#include <vector>

class OutputNode : public Effect {
public:
    OutputNode();
    ~OutputNode() override = default;

    void Load() override;
    void Update(float currentTime) override;
    void Render() override;
    void RenderUI() override;
    GLuint GetOutputTexture() const override;

    int GetInputPinCount() const override;
    void SetInputEffect(int pinIndex, Effect* inputEffect) override;
    Effect* GetInputEffect() const;

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& data) override;
    void ResetParameters() override;

    int GetDeserializedInputId() const { return m_deserialized_input_id; }

private:
    Effect* m_inputEffect = nullptr;
    int m_deserialized_input_id = -1;
};
