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
    j["name"] = name;
    j["id"] = id; // Important for linking
    return j;
}

void OutputNode::Deserialize(const nlohmann::json& data) {
    name = data.value("name", "Scene Output");
    // ID is const, set in constructor. We rely on linking happening after all nodes are created.
}

void OutputNode::ResetParameters() {
    // No parameters to reset
}
