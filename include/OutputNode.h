#pragma once

#include "Effect.h"

class OutputNode : public Effect {
public:
    OutputNode();
    ~OutputNode() override = default;

    void Load() override;
    void Update(float currentTime) override;
    void Render() override;
    void RenderUI() override;

    int GetInputPinCount() const override;
    void SetInputEffect(int pinIndex, Effect* inputEffect) override;
    Effect* GetInputEffect() const;

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& data) override;
    void ResetParameters() override;

private:
    Effect* m_inputEffect = nullptr;
};
