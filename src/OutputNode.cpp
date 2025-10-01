#include "OutputNode.h"
#include "imgui.h" // For RenderUI

OutputNode::OutputNode() : Effect() {
    name = "Scene Output";
}

void OutputNode::Load() {
    // Nothing to load
}

void OutputNode::Update(float currentTime) {
    // Nothing to update
    (void)currentTime;
}

void OutputNode::Render() {
    // This node does not render anything itself.
    // The main loop will find this node and render its input.
}

void OutputNode::RenderUI() {
    ImGui::Text("Connect a node to this input");
    ImGui::Text("to see it on the main screen.");
}

GLuint OutputNode::GetOutputTexture() const {
    if (m_inputEffect) {
        return m_inputEffect->GetOutputTexture();
    }
    return 0; // Return 0 if no input is connected
}

int OutputNode::GetInputPinCount() const {
    return 1;
}

void OutputNode::SetInputEffect(int pinIndex, Effect* inputEffect) {
    if (pinIndex == 0) {
        m_inputEffect = inputEffect;
    }
}

Effect* OutputNode::GetInputEffect() const {
    return m_inputEffect;
}

nlohmann::json OutputNode::Serialize() const {
    nlohmann::json j;
    j["type"] = "OutputNode";
    j["id"] = id;
    j["name"] = name;
    if (m_inputEffect) {
        j["input_id"] = m_inputEffect->id;
    }
    return j;
}

void OutputNode::Deserialize(const nlohmann::json& data) {
    name = data.value("name", "Scene Output");
    if (data.contains("input_id")) {
        m_deserialized_input_id = data["input_id"];
    }
}

void OutputNode::ResetParameters() {
    // No parameters to reset
}

std::unique_ptr<Effect> OutputNode::Clone() const {
    return std::make_unique<OutputNode>(*this);
}
