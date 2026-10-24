// RaymarchVibe - Real-time Shader Exploration
// main.cpp - FINAL BUILD VERSION
#define IMGUI_DEFINE_MATH_OPERATORS // Ensure this is defined before any ImGui header is included

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <memory>
#include <queue>
#include <mutex>
#include <nlohmann/json.hpp> // For scene save/load
#include <filesystem> // For std::filesystem::path
#include <chrono> // For recording timer
#include <cstdlib> // For exit
#include <cstdint>
#include <cstring>

// --- Core App Headers ---
#include "Effect.h"
#include "ShaderEffect.h"
#include "Renderer.h"
#include "ShadertoyIntegration.h"

// --- ImGui and Widget Headers ---
// #define IMGUI_DEFINE_MATH_OPERATORS // Moved to top of file
#include "imgui.h"
#include "imgui_internal.h"      // For DockBuilder API
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "ImGuiSimpleTimeline.h"
#include "imnodes.h"
#include "ImGuiFileDialog.h"
#include "AudioSystem.h" // Added the actual AudioSystem header
#include "NodeTemplates.h" // For node template factory functions
#include "OutputNode.h" // For the Scene Output node
#include "Bess/Config/Themes.h" // Added Themes header
#include "VideoRecorder.h"
#include "TransitionRenderer.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

// Placeholder Audio System has been removed.

// --- Window dimensions ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// --- Global Constants ---
static const std::string PASSTHROUGH_EFFECT_NAME = "Passthrough (Final Output)";

// --- Forward Declarations ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void drop_callback(GLFWwindow* window, int count, const char** paths);

// New: GLFW error callback function
void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

void RenderMenuBar();
void RenderShaderEditorWindow();
void RenderEffectPropertiesWindow();
void RenderTimelineWindow();
void RenderNodeEditorWindow();
void RenderConsoleWindow();
void RenderAudioReactivityWindow();
void RenderShadertoyWindow();
void RenderFpsMeter();
void RenderCameraHelpText();

std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects);

TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log);
void ClearErrorMarkers();
static std::string LoadFileContent(const std::string& path, std::string& errorMsg); // Forward declaration

// --- Shader File I/O Helpers (New) ---
bool LoadShaderFromFileToEditor(const std::string& filePath, ShaderEffect* targetEffect, TextEditor& editor, std::string& consoleLog) {
    if (!targetEffect) {
        consoleLog = "Error: No shader effect selected to load into.";
        return false;
    }
    std::string errorMsg;
    std::string fileContent = LoadFileContent(filePath, errorMsg); // Existing helper to read file
    if (!errorMsg.empty()) {
        consoleLog = errorMsg;
        return false;
    }

    targetEffect->SetSourceFilePath(filePath);
    targetEffect->LoadShaderFromSource(fileContent); // Sets internal source code
    targetEffect->Load(); // Compiles, links, fetches uniforms, parses controls

    editor.SetText(fileContent);
    ClearErrorMarkers(); // Clear previous error markers

    const std::string& compileLog = targetEffect->GetCompileErrorLog();
    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
        // It's a compile error/warning if "Successfully" or "applied successfully" is not found
        consoleLog = "Loaded shader: " + filePath + ", but with issues:\n" + compileLog;
        editor.SetErrorMarkers(ParseGlslErrorLog(compileLog)); // Parse and set new error markers
    } else {
        consoleLog = "Loaded shader: " + filePath + " successfully.";
    }
    return true;
}

bool SaveEditorContentToFile(const std::string& filePath, TextEditor& editor, ShaderEffect* targetEffectToUpdatePath, std::string& consoleLog) {
    if (filePath.empty()) {
        consoleLog = "Error: File path for saving cannot be empty.";
        return false;
    }
    std::string shaderCode = editor.GetText();
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        consoleLog = "Error: Could not open file for saving: " + filePath;
        return false;
    }
    outFile << shaderCode;
    outFile.close(); // Close file before checking stream state

    if (!outFile.good()) { // Check if write operation was successful
        consoleLog = "Error: Failed to write shader to file: " + filePath;
        return false;
    }

    consoleLog = "Shader saved to: " + filePath;
    if (targetEffectToUpdatePath) {
        targetEffectToUpdatePath->SetSourceFilePath(filePath);
    }
    return true;
}


void SaveScene(const std::string& filePath);
void LoadScene(const std::string& filePath);


// --- Global State ---
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;
static int g_selectedTimelineItem = -1;
static Renderer g_renderer;
static TransitionRenderer g_transitions;
// The picture last put on screen. A node click blends from this, even if ImNodes had cleared g_selectedEffect.
static Effect* g_lastShownEffect = nullptr;
static TextEditor g_editor;
static AudioSystem g_audioSystem;
VideoRecorder g_videoRecorder;
static Bess::Config::Themes g_themes; // Global Themes object
static bool g_showGui = true;
static bool g_verboseLogging = false; // Verbose terminal logging flag (set via CLI -verbose=ON)
static std::string g_initialShaderPath = "shaders/raymarch_v2.frag"; // Initial shader to load (overridable via CLI -load=PATH)

// Camera state
static bool g_cameraControlsEnabled = false;
static float g_cameraRadius = 5.0f;
static float g_cameraAzimuth = 0.0f;
static float g_cameraPolar = glm::pi<float>() / 2.0f;
static glm::vec3 g_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
static bool g_isDragging = false;
static bool g_isTilting = false;
static bool g_isRotatingLight = false;
static float g_lightRadius = 5.0f;
static float g_lightAzimuth = 0.0f;
static float g_lightPolar = glm::pi<float>() / 2.0f;
static double g_lastMouseX = 0.0, g_lastMouseY = 0.0;

// Recording state
static std::chrono::steady_clock::time_point g_recordingStartTime;
static bool g_offlineRendering = false;
static float g_offlineTime = 0.0f;
static int g_videoQuality = 2; // Default to High
static int g_audioBitrate = 1; // Default to 192k
static char g_recordingFilename[128] = "output.mp4";
static int g_recordingFormat = 0;
static const char* g_recordingFormats[] = { "mp4", "mov" };
static bool g_recordAudio = true;
static std::string g_recordingError;
static bool g_recordingOverwritePending = false;
static int64_t g_offlineAudioRemainder = 0;
static std::vector<float> g_recordPullScratch;
// Realtime recordings step shader time by exactly 1/60. The display may be faster.
static bool g_recordingClockLive = false;
static double g_recordingTimeOrigin = 0.0;
static double g_recordingTimeAccum = 0.0;
static int g_recordingTicks = -1;

// Window visibility flags
static bool g_showShaderEditorWindow = true;
// static bool g_showEffectPropertiesWindow = true; // Removed, integrated into Node Editor
static bool g_showConsoleWindow = true;
// static bool g_showRenderViewWindow = true; // Render View is not a typical window, handled by main loop's final render pass.
static bool g_showTimelineWindow = false;
static bool g_showNodeEditorWindow = false;
static bool g_showAudioWindow = false;
static bool g_showShadertoyWindow = false;
static bool g_showFpsMeter = false;

static char g_shadertoyApiKeyBuffer[256] = ""; // For user's API key

static bool g_enableAudioLink = false; // Changed to false
static ImFont* g_audioTitleFont = nullptr;
static ImFont* g_audioSmallFont = nullptr;
static std::string g_consoleLog = "Welcome to RaymarchVibe Demoscene Tool!";
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool g_timeline_paused = false; // Reverted to false for default playback
// static float g_timeline_time = 0.0f; // This will be g_timelineState.currentTime_seconds // Unused variable
static bool g_timelineControlActive = false; // Added for explicit timeline UI control
static bool g_no_output_logged = false;

// --- Node Editor State for Delayed Positioning ---
#include <set> // Required for std::set
// static std::map<int, ImVec2> g_new_node_initial_positions; // Already included by ImGui
static std::set<int> g_nodes_requiring_initial_position;
static std::map<int, ImVec2> g_new_node_initial_positions;
static std::vector<int> g_nodes_to_delete;


// --- Drag and Drop Queue ---
struct DroppedFile {
    std::string path;
    ImVec2 drop_pos;
};
static std::vector<DroppedFile> g_dropped_files_queue;
static std::mutex g_dropped_files_mutex;

// --- Rate Limiting for Drag and Drop ---
static std::chrono::steady_clock::time_point g_last_drop_time;
static const std::chrono::milliseconds g_drop_cooldown(100); // Minimum 100ms between drops to prevent spam


// --- Timeline State (New) ---
#include "Timeline.h" // For TimelineState struct
static TimelineState g_timelineState;

// Shader Templates
static const std::string nativeShaderTemplate = R"(#version 330 core
out vec4 FragColor;

uniform vec2 iResolution; // viewport resolution (in pixels)
uniform float iTime;       // shader playback time (in seconds)
uniform vec4 iMouse;      // mouse pixel coords. xy: current (if MLB down), zw: click

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    FragColor = vec4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
)";

static const std::string shadertoyShaderTemplate = R"(// Common uniforms provided by Shadertoy
// uniform vec3 iResolution; // viewport resolution (in pixels)
// uniform float iTime;       // shader playback time (in seconds)
// uniform float iTimeDelta;  // render time (in seconds)
// uniform int iFrame;        // shader playback frame
// uniform vec4 iMouse;      // mouse pixel coords. xy: current (if MLB down), zw: click
// uniform sampler2D iChannel0; // input channel. XX = 2D/Cube
// uniform sampler2D iChannel1; // input channel. XX = 2D/Cube
// uniform sampler2D iChannel2; // input channel. XX = 2D/Cube
// uniform sampler2D iChannel3; // input channel. XX = 2D/Cube
// uniform vec3 iChannelResolution[4]; // channel resolution (in pixels)
// uniform float iChannelTime[4];       // channel playback time (in seconds)

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
}
)";


// --- Helper Functions ---

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon font.
static void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static Effect* FindEffectById(int effect_id) {
    for (const auto& effect_ptr : g_scene) {
        if (effect_ptr && effect_ptr->id == effect_id) {
            return effect_ptr.get();
        }
    }
    return nullptr;
}

// A connected Scene Output wins over the selection. outputNodeDrives is that case:
// clicking a node must not replace the chain, and must not start a blend.
static Effect* PresentedEffect(bool* outputNodeDrives) {
    for (const auto& effect : g_scene) {
        if (auto* outputNode = dynamic_cast<OutputNode*>(effect.get())) {
            if (outputNode->GetInputEffect()) {
                if (outputNodeDrives) *outputNodeDrives = true;
                return outputNode->GetInputEffect();
            }
        }
    }
    if (outputNodeDrives) *outputNodeDrives = false;
    return g_selectedEffect;
}

void MarkNodeForDeletion(int node_id) {
    // Add node to the deletion queue if it's not already there
    if (std::find(g_nodes_to_delete.begin(), g_nodes_to_delete.end(), node_id) == g_nodes_to_delete.end())
    {
        g_nodes_to_delete.push_back(node_id);
    }
}

// Helper function to add a new effect to the scene and set its initial position
void CreateAndPlaceNode(std::unique_ptr<Effect> newEffect, ImVec2 position) {
    if (newEffect) {
        Effect* rawPtr = newEffect.get();

        // Add to scene first, but keep ownership here until we're sure it loads successfully
        // Note: We need to move it to the scene and handle failures carefully

        try {
            g_scene.push_back(std::move(newEffect));
            rawPtr->Load();
            g_nodes_requiring_initial_position.insert(rawPtr->id);
            g_new_node_initial_positions[rawPtr->id] = position;
        }
        catch (const std::exception& e) {
            // Load failed - remove from scene and log error
            g_consoleLog += "ERROR: Failed to load effect '" + rawPtr->name + "', removing from scene. Exception: " + std::string(e.what()) + "\n";
            // Remove the failed effect from g_scene
            auto it = std::find_if(g_scene.begin(), g_scene.end(),
                [rawPtr](const std::unique_ptr<Effect>& eff) { return eff.get() == rawPtr; });
            if (it != g_scene.end()) {
                g_scene.erase(it);
            }
            // The unique_ptr went out of scope, so rawPtr is gone - no need to delete manually
        }
        catch (...) {
            // Catch any other exceptions (rare but possible)
            g_consoleLog += "ERROR: Fatal exception loading effect '" + rawPtr->name + "', removing from scene.\n";
            // Remove the failed effect from g_scene
            auto it = std::find_if(g_scene.begin(), g_scene.end(),
                [rawPtr](const std::unique_ptr<Effect>& eff) { return eff.get() == rawPtr; });
            if (it != g_scene.end()) {
                g_scene.erase(it);
            }
        }
    }
}

// Helper to load file content
static std::string LoadFileContent(const std::string& path, std::string& errorMsg) {
    std::ifstream file(path);
    if (!file.is_open()) {
        errorMsg = "Error: Could not open file: " + path;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) { // Check stream state after reading
        errorMsg = "Error: Failed to read file content from: " + path;
        return "";
    }
    errorMsg = ""; // Clear error message on success
    return buffer.str();
}

// Helper function to check for GL errors
void checkGLError(const std::string& label, bool logToGlobalConsole = true) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        std::string errorStr;
        switch(err) {
            case GL_INVALID_ENUM: errorStr = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "INVALID_OPERATION"; break;
            // GL_STACK_OVERFLOW and GL_STACK_UNDERFLOW are deprecated and may not be defined
            // case GL_STACK_OVERFLOW: errorStr = "STACK_OVERFLOW"; break;
            // case GL_STACK_UNDERFLOW: errorStr = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY: errorStr = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorStr = "UNKNOWN_ERROR (" + std::to_string(err) + ")"; break;
        }
        std::string logMsg = "GL_ERROR (" + label + "): " + errorStr;
        std::cerr << logMsg << std::endl;
        if (logToGlobalConsole && g_consoleLog.size() < 4096) { // Prevent g_consoleLog from getting too huge
             g_consoleLog += logMsg + "\n";
        }
    }
}

// Verbose logging helper
static void VLog(const std::string& msg) {
    if (g_verboseLogging) {
        std::cout << "[VERBOSE] " << msg << std::endl;
    }
}

// Attempt to resolve shader path passed via CLI. Supports a leading '/shaders/..' treated as relative to CWD.
static std::string ResolveShaderPath(const std::string& input) {
    if (input.empty()) return input;
    try {
        if (std::filesystem::exists(input)) return input;
        if (!input.empty() && input[0] == '/') {
            std::string alt1 = "." + input; // try relative to current working dir
            if (std::filesystem::exists(alt1)) return alt1;
            std::string alt2 = input.substr(1); // try stripping leading '/'
            if (std::filesystem::exists(alt2)) return alt2;
        }
    } catch (const std::exception&) {
        // ignore and fallback to original
    }
    return input;
}

// Parse CLI flags like -verbose=ON and -load=PATH
static void ParseCommandLineArgs(int argc, char** argv) {
    if (argc <= 1) return;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? std::string(argv[i]) : std::string();
        if (arg.empty()) continue;

        if (arg.rfind("-verbose", 0) == 0 || arg == "-v" || arg == "--verbose") {
            bool value = true; // default to ON if flag present
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                std::string val = arg.substr(eq + 1);
                std::string up = val;
                std::transform(up.begin(), up.end(), up.begin(), [](unsigned char c){ return (char)std::toupper(c); });
                if (up == "ON" || up == "1" || up == "TRUE" || up == "YES") value = true;
                else if (up == "OFF" || up == "0" || up == "FALSE" || up == "NO") value = false;
            }
            g_verboseLogging = value;
            if (g_verboseLogging) {
                std::cout << "[VERBOSE] Verbose logging enabled" << std::endl;
            }
            continue;
        }

        if (arg.rfind("-load", 0) == 0 || arg == "--load") {
            std::string val;
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                val = arg.substr(eq + 1);
            } else if (arg == "--load" || arg == "-load") {
                if (i + 1 < argc) { val = argv[++i]; }
            }
            if (!val.empty()) {
                std::string resolved = ResolveShaderPath(val);
                if (std::filesystem::exists(resolved)) {
                    g_initialShaderPath = resolved;
                    VLog(std::string("CLI requested shader load: ") + g_initialShaderPath);
                } else {
                    std::cerr << "CLI -load path not found: " << val << std::endl;
                    std::cerr << "Continuing with default shader: " << g_initialShaderPath << std::endl;
                }
            }
            continue;
        }

        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: RaymarchVibe [-verbose=ON|OFF] [-load=PATH_TO_SHADER]\n";
            std::cout << "Examples:\n";
            std::cout << "  ./RaymarchVibe -verbose=ON -load=shaders/new_shader_test.frag\n";
            std::cout << "  ./RaymarchVibe -v -load=/shaders/new_shader_test.frag\n";
            continue; // don't exit, still launch the app
        }
    }
}


// --- UI Window Implementations ---

static void applyRecordingExtension() {
    const char* ext = g_recordingFormats[g_recordingFormat];
    std::string name(g_recordingFilename);
    const auto slash = name.find_last_of("/\\");
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        name.erase(dot);
    name.push_back('.');
    name += ext;
    if (name.size() >= sizeof(g_recordingFilename))
        name.resize(sizeof(g_recordingFilename) - 1);
    std::strncpy(g_recordingFilename, name.c_str(), sizeof(g_recordingFilename) - 1);
    g_recordingFilename[sizeof(g_recordingFilename) - 1] = '\0';
}

static void noteRecordingFailure() {
    g_recordingError = g_videoRecorder.get_last_error();
    if (g_recordingError.empty())
        g_recordingError = "Could not start recording.";
    g_consoleLog += "Recording: " + g_recordingError + "\n";
}

// One 1/60 of the open audio file, padded with silence if the file has ended.
// The block is what this saved frame hears and what the shader reacts to.
static void pullRealtimeFileAudio() {
    if (!g_audioSystem.IsRecordPullActive())
        return;
    const int sampleRate = static_cast<int>(g_audioSystem.GetCurrentInputSampleRate());
    const int channels = static_cast<int>(g_audioSystem.GetCurrentInputChannels());
    if (sampleRate <= 0 || channels <= 0)
        return;
    g_offlineAudioRemainder += sampleRate;
    const int framesNeeded = static_cast<int>(g_offlineAudioRemainder / 60);
    g_offlineAudioRemainder %= 60;
    if (framesNeeded <= 0)
        return;
    const size_t samples = static_cast<size_t>(framesNeeded) * static_cast<size_t>(channels);
    if (g_recordPullScratch.size() < samples)
        g_recordPullScratch.resize(samples);
    const ma_uint64 framesRead = g_audioSystem.ReadRecordPull(g_recordPullScratch.data(), static_cast<ma_uint32>(framesNeeded));
    if (framesRead > 0)
        g_videoRecorder.add_audio_frame(g_recordPullScratch.data(), static_cast<int>(framesRead));
}

// Starts a take from the menu settings. Offline seek/pause runs only after the encoder is up.
static bool beginRecording() {
    if (g_videoRecorder.is_recording())
        return false;
    if (g_recordAudio &&
        g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::Microphone &&
        !g_audioSystem.IsCaptureDeviceInitialized()) {
        g_audioSystem.InitializeAndStartSelectedCaptureDevice();
    }

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        g_recordingError = "Cannot start recording: no OpenGL window.";
        g_consoleLog += "Recording: " + g_recordingError + "\n";
        return false;
    }
    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    fb_width &= ~1;
    fb_height &= ~1;
    g_offlineAudioRemainder = 0;

    // Realtime file audio is pulled per saved frame. The playback callback stays on the
    // wall clock for the speakers and must not also feed the file.
    const bool wantFilePull = g_recordAudio && !g_offlineRendering
        && g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::AudioFile
        && g_audioSystem.IsAudioFileLoaded();
    if (wantFilePull && !g_audioSystem.BeginRecordPull()) {
        g_recordingError = "Could not open the audio file for recording.";
        g_consoleLog += "Recording: " + g_recordingError + "\n";
        return false;
    }

    const bool started = g_videoRecorder.start_recording(
        g_recordingFilename, fb_width, fb_height, 60, g_recordingFormats[g_recordingFormat],
        g_recordAudio,
        static_cast<int>(g_audioSystem.GetCurrentInputSampleRate()),
        static_cast<int>(g_audioSystem.GetCurrentInputChannels()),
        g_offlineRendering,
        static_cast<VideoRecorder::VideoQuality>(g_videoQuality),
        static_cast<VideoRecorder::AudioBitrate>(g_audioBitrate),
        wantFilePull);
    if (!started) {
        g_audioSystem.EndRecordPull();
        noteRecordingFailure();
        return false;
    }

    g_recordingError.clear();
    if (g_offlineRendering) {
        g_offlineTime = g_timelineState.currentTime_seconds;
        if (g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::AudioFile) {
            g_audioSystem.Pause();
            const float duration = g_audioSystem.GetPlaybackDuration();
            if (duration > 0.0f)
                g_audioSystem.SetPlaybackProgress(g_offlineTime / duration);
        }
    }
    g_recordingStartTime = std::chrono::steady_clock::now();
    return true;
}

static void requestStartRecording() {
    if (std::filesystem::exists(g_recordingFilename))
        g_recordingOverwritePending = true;
    else
        beginRecording();
}

// Folder last used by File > Load Shader. Empty until the dialog closes on a real directory.
static std::string g_lastShaderLoadDirectory;

static void rememberShaderLoadDirectory(const std::filesystem::path& candidate) {
    std::error_code ec;
    std::filesystem::path directory = candidate;
    if (directory.empty())
        return;
    if (!std::filesystem::is_directory(directory, ec))
        directory = directory.parent_path();
    if (!directory.empty() && std::filesystem::is_directory(directory, ec))
        g_lastShaderLoadDirectory = directory.string();
}

static std::string shaderLoadDialogStartPath() {
    std::error_code ec;
    if (!g_lastShaderLoadDirectory.empty() && std::filesystem::is_directory(g_lastShaderLoadDirectory, ec))
        return g_lastShaderLoadDirectory;
    return ".";
}

void RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Shader...")) {
                IGFD::FileDialogConfig config;
                config.path = shaderLoadDialogStartPath();
                config.countSelectionMax = 1;
                ImGuiFileDialog::Instance()->OpenDialog("LoadShaderDlgKey", "Choose Shader File", ".frag,.fs,.glsl,.*", config);
            }
            bool canSave = (g_selectedEffect && dynamic_cast<ShaderEffect*>(g_selectedEffect));
            if (ImGui::MenuItem("Save Shader", nullptr, false, canSave)) {
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    const std::string& currentPath = se->GetSourceFilePath();
                    if (!currentPath.empty() && currentPath.find("shadertoy://") == std::string::npos && currentPath != "dynamic_source" && currentPath.rfind("Untitled", 0) != 0) {
                        SaveEditorContentToFile(currentPath, g_editor, se, g_consoleLog);
                    } else { // No valid path or is an "Untitled" default, so effectively "Save As"
                        ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
                    }
                }
            }
            if (ImGui::MenuItem("Save Shader As...", nullptr, false, canSave)) {
                 ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene...")) {
                ImGuiFileDialog::Instance()->OpenDialog("SaveSceneDlgKey", "Save Scene File", ".json");
            }
            if (ImGui::MenuItem("Load Scene...")) {
                ImGuiFileDialog::Instance()->OpenDialog("LoadSceneDlgKey", "Load Scene File", ".json");
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Themes")) {
                const auto& availableThemes = g_themes.getThemes();
                // TODO: Get current theme to show a checkmark next to it.
                for (const auto& pair : availableThemes) {
                    if (ImGui::MenuItem(pair.first.c_str())) {
                        g_themes.applyTheme(pair.first);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) { glfwSetWindowShouldClose(glfwGetCurrentContext(), true); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Shader Editor", nullptr, &g_showShaderEditorWindow);
            // ImGui::MenuItem("Effect Properties", nullptr, &g_showEffectPropertiesWindow); // Removed
            ImGui::MenuItem("Console", nullptr, &g_showConsoleWindow);
            ImGui::Separator();
            ImGui::MenuItem("Timeline", nullptr, &g_showTimelineWindow);
            ImGui::MenuItem("Shadertoy", nullptr, &g_showShadertoyWindow);
            ImGui::Separator();
            ImGui::MenuItem("Camera Controls", nullptr, &g_cameraControlsEnabled);
            ImGui::Separator();
            ImGui::MenuItem("Toggle All GUI", "Spacebar", &g_showGui);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Recording (F1)")) {
            ImGui::InputText("Filename", g_recordingFilename, sizeof(g_recordingFilename));
            ImGui::SameLine();
            if (ImGui::Button("Browse")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("SaveRecordingDlgKey", "Choose Output File", ".mp4,.mov", config);
            }

            ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
            if (ImGuiFileDialog::Instance()->Display("SaveRecordingDlgKey")) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    const std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                    std::strncpy(g_recordingFilename, filePath.c_str(), sizeof(g_recordingFilename) - 1);
                    g_recordingFilename[sizeof(g_recordingFilename) - 1] = '\0';
                }
                ImGuiFileDialog::Instance()->Close();
            }

            if (ImGui::BeginCombo("Format", g_recordingFormats[g_recordingFormat])) {
                for (int i = 0; i < IM_ARRAYSIZE(g_recordingFormats); i++) {
                    const bool is_selected = (g_recordingFormat == i);
                    if (ImGui::Selectable(g_recordingFormats[i], is_selected)) {
                        g_recordingFormat = i;
                        applyRecordingExtension();
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Record Audio", &g_recordAudio);
            ImGui::Checkbox("Offline Rendering (Smooth Video)", &g_offlineRendering);
            ImGui::SameLine(); HelpMarker("Decouples rendering from real-time. Ensures smooth 60 FPS video even if the app runs slowly. Audio sync only works with Audio File source.");

            const char* quality_items[] = { "Low", "Medium", "High", "Ultra" };
            ImGui::Combo("Video Quality", &g_videoQuality, quality_items, IM_ARRAYSIZE(quality_items));

            const char* bitrate_items[] = { "128 kbps", "192 kbps", "320 kbps", "Lossless (ALAC)" };
            ImGui::Combo("Audio Bitrate", &g_audioBitrate, bitrate_items, IM_ARRAYSIZE(bitrate_items));

            if (g_videoRecorder.is_recording()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                if (ImGui::Button("Stop Recording")) {
                    g_videoRecorder.stop_recording();
                }
                ImGui::PopStyleColor(4);
                ImGui::SameLine();

                auto duration = std::chrono::steady_clock::now() - g_recordingStartTime;
                auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
                duration -= hours;
                auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
                duration -= minutes;
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
                ImGui::Text("Status: Recording... %02d:%02d:%02d", (int)hours.count(), (int)minutes.count(), (int)seconds.count());

            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                if (ImGui::Button("Start Recording"))
                    requestStartRecording();
                ImGui::PopStyleColor(4);
                ImGui::Text("Status: Idle");
                if (!g_recordingError.empty())
                    ImGui::TextWrapped("%s", g_recordingError.c_str());
            }

            ImGui::EndMenu();
        }
        ImGui::MenuItem("Node Editor (F2)", "F2", &g_showNodeEditorWindow);
        ImGui::MenuItem("Audio Reactivity (F3)", "F3", &g_showAudioWindow);
        ImGui::MenuItem("FPS Meter (F4)", "F4", &g_showFpsMeter);

        ImGui::EndMainMenuBar();
    }

    // Outside the menu so F1 can open it while the menu is closed.
    if (g_recordingOverwritePending) {
        ImGui::OpenPopup("Overwrite File?");
        g_recordingOverwritePending = false;
    }
    if (ImGui::BeginPopupModal("Overwrite File?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File '%s' already exists.\nDo you want to overwrite it?", g_recordingFilename);
        ImGui::Separator();
        if (ImGui::Button("Overwrite", ImVec2(120, 0))) {
            beginRecording();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // --- Handle File Dialogs for Shader Load/Save ---
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGuiFileDialog::Instance()->Display("LoadShaderDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            rememberShaderLoadDirectory(filePathName);
            std::string justFileName = std::filesystem::path(filePathName).filename().string();

            auto newEffect = std::make_unique<ShaderEffect>(filePathName, SCR_WIDTH, SCR_HEIGHT);
            newEffect->name = justFileName.empty() ? "Untitled Shader" : justFileName;
            newEffect->Load();
            if (!newEffect->GetCompileErrorLog().empty() && newEffect->GetCompileErrorLog().find("Successfully") == std::string::npos && newEffect->GetCompileErrorLog().find("applied successfully") == std::string::npos) {
                g_consoleLog = "Error loading shader " + justFileName + " into new effect. Log: " + newEffect->GetCompileErrorLog();
            } else {
                g_editor.SetText(newEffect->GetShaderSource());
                ClearErrorMarkers();
                g_scene.push_back(std::move(newEffect));
                g_selectedEffect = g_scene.back().get(); // Select the new effect
                g_consoleLog = "Loaded shader '" + justFileName + "' into a new effect.";
            }
        } else {
            rememberShaderLoadDirectory(ImGuiFileDialog::Instance()->GetCurrentPath());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGuiFileDialog::Instance()->Display("SaveShaderAsDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            SaveEditorContentToFile(filePathName, g_editor, dynamic_cast<ShaderEffect*>(g_selectedEffect), g_consoleLog);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGuiFileDialog::Instance()->Display("SaveSceneDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            SaveScene(filePathName);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGuiFileDialog::Instance()->Display("LoadSceneDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            LoadScene(filePathName);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Handle dialog for loading texture into a shader iChannel
    if (ImGuiFileDialog::Instance()->Display("LoadTextureForIChannelDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            if (g_selectedEffect) {
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    int channelIndex = se->GetChannelPendingTextureLoad();
                    if (channelIndex != -1) {
                        auto newImageEffect = RaymarchVibe::NodeTemplates::CreateImageLoaderEffect(filePath);
                        ImVec2 pos = ImNodes::GetNodeScreenSpacePos(se->id);
                        CreateAndPlaceNode(std::move(newImageEffect), pos + ImVec2(0, 150));
                        Effect* rawImageEffect = g_scene.back().get();
                        se->SetInputEffect(channelIndex, rawImageEffect);
                        se->ClearChannelPendingTextureLoad();
                    }
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void RenderShaderEditorWindow() {
    static char filePathBuffer_SaveAs[512] = ""; 
    static int lineToGo = 1;

    ImGui::SetNextWindowSize(ImVec2(600, 800), ImGuiCond_FirstUseEver);
    ImGui::Begin("Shader Editor");

    if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
        // Toolbar for Apply, Find, Go To Line
        if (ImGui::Button("Apply")) {
            se->ApplyShaderCode(g_editor.GetText());
            const std::string& log = se->GetCompileErrorLog();
            if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) {
                g_consoleLog = "Apply failed:\n" + log;
                g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
            } else {
                g_consoleLog = "Shader applied successfully!";
                ClearErrorMarkers();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh (F5)") || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F5, false))) {
            const std::string& currentPath = se->GetSourceFilePath();
            if (!currentPath.empty() && currentPath != "dynamic_source" && currentPath.rfind("shadertoy://", 0) != 0) {
                LoadShaderFromFileToEditor(currentPath, se, g_editor, g_consoleLog);
            } else {
                g_consoleLog = "Cannot refresh a shader that was not loaded from a file.";
            }
        }
        ImGui::SameLine();

        ImGui::PushItemWidth(80);
        ImGui::InputInt("##GoToLine", &lineToGo, 0, 0); // No step buttons
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Go")) {
            if (lineToGo > 0) {
                g_editor.SetCursorPosition(TextEditor::Coordinates(lineToGo - 1, 0));
            }
        }
        ImGui::SameLine();
        ImGui::Text("Mouse: (%.1f, %.1f)", g_mouseState[0], g_mouseState[1]);

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20.0f, 0.0f));
        ImGui::SameLine();

        bool isShadertoy = se->IsShadertoyMode();
        if (ImGui::Checkbox("Shadertoy Mode", &isShadertoy)) {
            se->SetShadertoyMode(isShadertoy);
            se->ApplyShaderCode(g_editor.GetText());
            const std::string& log = se->GetCompileErrorLog();
            if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) {
                g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
                g_consoleLog = log;
            } else {
                ClearErrorMarkers();
                g_consoleLog = "Toggled Shadertoy mode and re-applied shader.";
            }
        }

        ImGui::Separator();
        g_editor.Render("TextEditor");

    } else {
        ImGui::TextWrapped("No valid shader effect selected. Please select a shader node in the Node Editor to see its source code.");
    }

    if (ImGuiFileDialog::Instance()->Display("SaveShaderAsDlgKey_Editor")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string shaderCode = g_editor.GetText();
            std::ofstream outFile(filePathName);
            if (outFile.is_open()) {
                outFile << shaderCode;
                outFile.close();
                if (outFile.good()) {
                    g_consoleLog = "Shader saved to: " + filePathName;
                    if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                        se->SetSourceFilePath(filePathName);
                        strncpy(filePathBuffer_SaveAs, filePathName.c_str(), sizeof(filePathBuffer_SaveAs) - 1);
                    }
                } else { g_consoleLog = "Error: Failed to write shader to file: " + filePathName; }
            } else { g_consoleLog = "Error: Could not open file for saving: " + filePathName; }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::End();
}

// void RenderEffectPropertiesWindow() { // Function removed as properties are now in Node Editor
//     ImGui::Begin("Effect Properties");
//     if (g_selectedEffect) {
//         if (ImGui::Button("Reset Parameters")) {
//             g_selectedEffect->ResetParameters();
//             if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
//                 g_editor.SetText(se->GetShaderSource());
//                 ClearErrorMarkers();
//             }
//             g_consoleLog = "Parameters reset successfully.";
//         }
//         ImGui::Separator();
//         g_selectedEffect->RenderUI();
//     } else {
//         ImGui::Text("No effect selected.");
//     }
//     ImGui::End();
// }

void RenderTimelineWindow() {
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::Checkbox("Enable Timeline Master Control (Sets iTime)", &g_timelineState.isEnabled);
    ImGui::SameLine(); HelpMarker("If checked, the timeline's current time will be used as the master 'iTime' for shaders. Otherwise, shaders use system time.");

    ImGui::Checkbox("Enable Timeline UI Playback Control", &g_timelineControlActive);
    ImGui::SameLine(); HelpMarker("When enabled, use Pause/Play/Reset below to control this timeline's playhead. This playhead may or may not be the master 'iTime'.");

    // Disable timeline playback UI controls if g_timelineControlActive is false
    if (!g_timelineControlActive) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button(g_timeline_paused ? "Play" : "Pause")) {
        if (g_timelineControlActive) g_timeline_paused = !g_timeline_paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        if (g_timelineControlActive) g_timelineState.currentTime_seconds = 0.0f; // Use TimelineState
    }

    if (!g_timelineControlActive) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }

    ImGui::SameLine();
    ImGui::Text("Time: %.2f", g_timelineState.currentTime_seconds); // Use TimelineState

    // Add controls for zoom and scroll (basic for now)
    ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
    ImGui::PushItemWidth(100);
    ImGui::DragFloat("Zoom", &g_timelineState.zoomLevel, 0.1f, 0.1f, 10.0f);
    ImGui::SameLine();
    ImGui::DragFloat("Scroll", &g_timelineState.horizontalScroll_seconds, 0.5f, 0.0f, g_timelineState.totalDuration_seconds);
    ImGui::PopItemWidth();

    ImGui::Separator();
    std::vector<ImGui::TimelineItem> timelineItems;
    static std::vector<int> track_indices_storage;

    if (track_indices_storage.size() < g_scene.size()) {
        track_indices_storage.resize(g_scene.size());
    }

    for (size_t i = 0; i < g_scene.size(); ++i) {
        if (!g_scene[i]) {
            continue;
        }
        track_indices_storage[i] = i % 4;
        timelineItems.push_back({
            g_scene[i]->name.c_str(),
            &g_scene[i]->startTime,
            &g_scene[i]->endTime,
            &track_indices_storage[i]
        });
    }

    bool timeline_event = ImGui::SimpleTimeline("Scene", timelineItems, &g_timelineState.currentTime_seconds, &g_selectedTimelineItem,
                               4, // num_tracks
                               0.0f, // sequence_total_start_time_seconds
                               g_timelineState.totalDuration_seconds, // sequence_total_end_time_seconds
                               g_timelineState.horizontalScroll_seconds, // Pass scroll
                               g_timelineState.zoomLevel                 // Pass zoom
                               );

    if (timeline_event) {
        if (g_selectedTimelineItem >= 0 && static_cast<size_t>(g_selectedTimelineItem) < g_scene.size()) {
            if (g_scene[g_selectedTimelineItem]) {
                g_selectedEffect = g_scene[g_selectedTimelineItem].get();
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    g_editor.SetText(se->GetShaderSource());
                    ClearErrorMarkers();
                }
            }
        }
    }

    ImGui::End();
}

void RenderNodeEditorWindow() {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Node Editor");

    // Node Properties width. Drag the divider between the canvas and this pane to change it.
    static float sidebar_width = 350.0f;
    const float sidebar_default_width = 350.0f;
    const float sidebar_min_width = 180.0f;
    const float canvas_min_width = 160.0f;
    const float splitter_thickness = 6.0f;
    const ImVec2 content_avail = ImGui::GetContentRegionAvail();
    const float sidebar_max_width = ImMax(sidebar_min_width, content_avail.x - splitter_thickness - canvas_min_width);
    sidebar_width = ImClamp(sidebar_width, ImMin(sidebar_min_width, sidebar_max_width), sidebar_max_width);
    const float canvas_width = ImMax(1.0f, content_avail.x - splitter_thickness - sidebar_width);

    // Node Editor Canvas (Left Side)
    ImGui::BeginChild("NodeEditorCanvas", ImVec2(canvas_width, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImNodes::BeginNodeEditor();
    for (const auto& effect_ptr : g_scene) {
        if (!effect_ptr) continue;
        ImNodes::BeginNode(effect_ptr->id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(effect_ptr->name.c_str());
        ImNodes::EndNodeTitleBar();

        if (ImGui::BeginPopupContextItem("Node Context Menu"))
        {
            if (ImGui::MenuItem("Duplicate"))
            {
                if (effect_ptr) {
                    auto newEffect = effect_ptr->Clone();
                    Effect* rawPtr = newEffect.get();
                    g_scene.push_back(std::move(newEffect));
                    rawPtr->Load(); // Important to load after adding to scene

                    // Position the new node near the original
                    ImVec2 originalPos = ImNodes::GetNodeScreenSpacePos(effect_ptr->id);
                    g_nodes_requiring_initial_position.insert(rawPtr->id);
                    g_new_node_initial_positions[rawPtr->id] = originalPos + ImVec2(50.0f, 50.0f);
                }
            }
            if (ImGui::MenuItem("Unlink"))
            {
                // 1. Clear all input connections OF this node
                for (int i = 0; i < effect_ptr->GetInputPinCount(); ++i) {
                    effect_ptr->SetInputEffect(i, nullptr);
                }

                // 2. Clear all connections FROM other nodes TO this node
                for (const auto& other_effect : g_scene) {
                    if (other_effect && other_effect.get() != effect_ptr.get()) {
                        // This requires a virtual method in Effect to get all its inputs
                        // and check if they point to effect_ptr. For now, we assume ShaderEffect and OutputNode.
                        if (auto* other_se = dynamic_cast<ShaderEffect*>(other_effect.get())) {
                            const auto& inputs = other_se->GetInputs();
                            for (size_t i = 0; i < inputs.size(); ++i) {
                                if (inputs[i] && inputs[i]->id == effect_ptr->id) {
                                    other_se->SetInputEffect(i, nullptr);
                                }
                            }
                        } else if (auto* other_on = dynamic_cast<OutputNode*>(other_effect.get())) {
                            if (other_on->GetInputEffect() && other_on->GetInputEffect()->id == effect_ptr->id) {
                                other_on->SetInputEffect(0, nullptr);
                            }
                        }
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete"))
            {
                MarkNodeForDeletion(effect_ptr->id);
            }
            ImGui::EndPopup();
        }

        // Delayed positioning for newly added nodes
        if (g_nodes_requiring_initial_position.count(effect_ptr->id)) {
            ImVec2 initial_pos = g_new_node_initial_positions[effect_ptr->id];
            ImNodes::SetNodeScreenSpacePos(effect_ptr->id, initial_pos);

            g_nodes_requiring_initial_position.erase(effect_ptr->id);
            g_new_node_initial_positions.erase(effect_ptr->id);
        }

        if (effect_ptr->GetOutputPinCount() > 0) {
            ImNodes::BeginOutputAttribute(effect_ptr->id * 10); ImGui::Text("out"); ImNodes::EndOutputAttribute();
        }
        for (int i = 0; i < effect_ptr->GetInputPinCount(); ++i) {
            ImNodes::BeginInputAttribute(effect_ptr->id * 10 + 1 + i); ImGui::Text("in %d", i); ImNodes::EndInputAttribute();
        }
        ImNodes::EndNode();
    }
    int link_id_counter = 1;
    for (const auto& effect_ptr : g_scene) {
        if (!effect_ptr) continue;

        // Handle ShaderEffect inputs
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            const auto& inputs = se->GetInputs();
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (inputs[i]) {
                    ImNodes::Link(link_id_counter++, inputs[i]->id * 10, se->id * 10 + 1 + i);
                }
            }
        }
        // Handle OutputNode input
        else if (auto* on = dynamic_cast<OutputNode*>(effect_ptr.get())) {
            if (on->GetInputEffect()) {
                ImNodes::Link(link_id_counter++, on->GetInputEffect()->id * 10, on->id * 10 + 1);
            }
        }
    }

    static ImVec2 popup_pos;
    // Context menu for adding new nodes, triggered by right-click on the editor canvas
    // Ensuring this is called within BeginNodeEditor / EndNodeEditor
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        popup_pos = ImGui::GetMousePos();
        ImGui::OpenPopup("AddNodeContextMenu");
    }

    if (ImGui::BeginPopup("AddNodeContextMenu")) {
        if (ImGui::BeginMenu("Add Effect")) {
            if (ImGui::BeginMenu("Generators")) {
                if (ImGui::MenuItem("Basic Plasma")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreatePlasmaBasicEffect(), popup_pos);
                if (ImGui::MenuItem("Simple Color")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateSimpleColorEffect(), popup_pos);
                if (ImGui::MenuItem("Value Noise")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateValueNoiseEffect(), popup_pos);
                if (ImGui::MenuItem("Noise Generator")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateNoiseEffect(), popup_pos);
                if (ImGui::MenuItem("Raymarch Sphere")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateRaymarchSphereEffect(), popup_pos);
                if (ImGui::MenuItem("Circular Audio Viz")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateCircularAudioVizEffect(), popup_pos);
                if (ImGui::MenuItem("Organic Audio Viz")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateOrganicAudioVizEffect(), popup_pos);
                if (ImGui::MenuItem("Organic Fractal Tree")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateOrganicFractalTreeEffect(), popup_pos);
                if (ImGui::MenuItem("Bezier Fractal Visualizer")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateBezierFractalVisualizerEffect(), popup_pos);
                if (ImGui::MenuItem("Debug Color")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateDebugColorEffect(), popup_pos);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Filters")) {
                if (ImGui::MenuItem("Master Color")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateMasterColorEffect(), popup_pos);
                if (ImGui::MenuItem("Color Correction")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateColorCorrectionEffect(), popup_pos);
                if (ImGui::MenuItem("Sharpen")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateSharpenEffect(), popup_pos);
                if (ImGui::MenuItem("Grain")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateGrainEffect(), popup_pos);
                if (ImGui::MenuItem("Chromatic Aberration")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateChromaticAberrationEffect(), popup_pos);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Post-Processing")) {
                if (ImGui::MenuItem("Bloom")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateBloomEffect(), popup_pos);
                if (ImGui::MenuItem("Dither")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateDitherEffect(), popup_pos);
                if (ImGui::MenuItem("Kaleidoscope")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateKaleidoscopeEffect(), popup_pos);
                if (ImGui::MenuItem("Movement")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateMovementEffect(), popup_pos);
                if (ImGui::MenuItem("Posterize")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreatePosterizeEffect(), popup_pos);
                if (ImGui::MenuItem("Tone Mapping")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateToneMappingEffect(), popup_pos);
                if (ImGui::MenuItem("Vignette")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateVignetteEffect(), popup_pos);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Image")) {
                if (ImGui::MenuItem("Image Loader")) CreateAndPlaceNode(RaymarchVibe::NodeTemplates::CreateImageLoaderEffect(), popup_pos);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Output")) {
            if (ImGui::MenuItem("Scene Output")) CreateAndPlaceNode(std::make_unique<OutputNode>(), popup_pos);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Arrange Nodes Here")) {
            const float GRID_SPACING_X = 250.0f;
            const float GRID_SPACING_Y = 200.0f;
            const int GRID_COLUMNS = 4;
            
            int index = 0;
            for (const auto& effect_ptr : g_scene) {
                if (!effect_ptr) continue;
                
                float x = popup_pos.x + (index % GRID_COLUMNS) * GRID_SPACING_X;
                float y = popup_pos.y + (index / GRID_COLUMNS) * GRID_SPACING_Y;
                
                ImNodes::SetNodeScreenSpacePos(effect_ptr->id, ImVec2(x, y));
                index++;
            }
        }
        ImGui::EndPopup();
    }

    ImNodes::EndNodeEditor();

    // Handle deletion of nodes via Delete key
    if (ImGui::IsKeyReleased(ImGuiKey_Delete))
    {
        const int num_selected_nodes = ImNodes::NumSelectedNodes();
        if (num_selected_nodes > 0)
        {
            std::vector<int> selected_node_ids(num_selected_nodes);
            ImNodes::GetSelectedNodes(selected_node_ids.data());
            for (const int node_id : selected_node_ids)
            {
                MarkNodeForDeletion(node_id);
            }
        }
    }

    // Handle link creation (should remain associated with the canvas where interaction happens)
    int start_attr, end_attr;
    if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
        // ... (link creation logic remains the same) ...
        int start_node_id = start_attr / 10;
        int end_node_id = end_attr / 10;
        Effect* start_effect = FindEffectById(start_node_id);
        Effect* end_effect = FindEffectById(end_node_id);
        bool start_is_output = (start_attr % 10 == 0);
        bool end_is_input = (end_attr % 10 != 0);

        if (start_effect && end_effect && start_node_id != end_node_id) {
            if (start_is_output && end_is_input) {
                int input_pin_index = (end_attr % 10) - 1;
                end_effect->SetInputEffect(input_pin_index, start_effect);
            } else if (!start_is_output && !end_is_input) {
                int input_pin_index = (start_attr % 10) - 1;
                start_effect->SetInputEffect(input_pin_index, end_effect);
            }
        }
    }

    // Handle Ctrl+Click on node to break links
    // Check if the main node editor canvas (where nodes are) is hovered.
    // Using IsWindowHovered with RootAndChildWindows might be more robust if popups interfere.
    // However, GetHoveredNode should only return a valid ID if a node (not empty canvas) is truly under mouse.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int hovered_node_id = -1;
        // ImNodes::GetHoveredNode() might be what we need, or iterate nodes and check ImGui::IsItemHovered()
        // For ImNodes, it's better to use its API if available for this specific check,
        // or if it refers to a different kind of hover state.
        // A simpler way for ImNodes:
        ImNodes::IsNodeHovered(&hovered_node_id); // This updates hovered_node_id if a node is hovered by mouse

        if (hovered_node_id != -1) {
            Effect* clicked_effect = FindEffectById(hovered_node_id);
            if (clicked_effect) {
                g_consoleLog += "Ctrl+Clicked on node ID: " + std::to_string(clicked_effect->id) + " Name: " + clicked_effect->GetEffectName() + ". Breaking links.\n";
                // 1. Clear inputs OF the clicked node
                if (auto* se_clicked = dynamic_cast<ShaderEffect*>(clicked_effect)) {
                    for (int i = 0; i < se_clicked->GetInputPinCount(); ++i) {
                        se_clicked->SetInputEffect(i, nullptr);
                    }
                }

                // 2. Clear inputs TO the clicked node from other nodes
                for (const auto& effect_ptr_unique : g_scene) {
                    if (auto* se_target = dynamic_cast<ShaderEffect*>(effect_ptr_unique.get())) {
                        if (se_target == clicked_effect) continue; // Already handled its own inputs

                        const auto& inputs = se_target->GetInputs(); // Need to ensure GetInputs provides enough info or direct access
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            if (inputs[i] && inputs[i]->id == clicked_effect->id) {
                                se_target->SetInputEffect(i, nullptr);
                            }
                        }
                    }
                }
            }
        }
    }

    ImGui::EndChild(); // End NodeEditorCanvas

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushID("NodePropertiesSplitter");
    ImGui::InvisibleButton("##splitter", ImVec2(splitter_thickness, content_avail.y));
    if (ImGui::IsItemActive()) {
        sidebar_width = ImClamp(sidebar_width - ImGui::GetIO().MouseDelta.x, ImMin(sidebar_min_width, sidebar_max_width), sidebar_max_width);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        sidebar_width = sidebar_default_width;
    }
    const bool splitter_hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
    if (splitter_hot) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    const ImGuiCol splitter_color_id = ImGui::IsItemActive() ? ImGuiCol_SeparatorActive : (splitter_hot ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
    const ImU32 splitter_col = ImGui::GetColorU32(splitter_color_id);
    const ImVec2 splitter_min = ImGui::GetItemRectMin();
    const ImVec2 splitter_max = ImGui::GetItemRectMax();
    const float splitter_mid_x = (splitter_min.x + splitter_max.x) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(splitter_mid_x, splitter_min.y), ImVec2(splitter_mid_x, splitter_max.y), splitter_col, splitter_hot ? 2.0f : 1.0f);
    ImGui::PopID();

    ImGui::SameLine(0.0f, 0.0f);

    // Sidebar (Right Side) - will contain Properties and Instructions. Width 0 fills the space left of the divider.
    ImGui::BeginChild("SidebarChild", ImVec2(0.0f, 0.0f), true); // true for border

    // --- Node Properties Panel ---
    ImGui::Text("Node Properties");
    ImGui::Separator();

    const int num_selected_nodes = ImNodes::NumSelectedNodes();
    std::vector<int> selected_node_ids;
    if (num_selected_nodes > 0) {
        selected_node_ids.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(selected_node_ids.data());
    }

    Effect* transitionNode = nullptr;
    if (selected_node_ids.size() == 1)
        transitionNode = FindEffectById(selected_node_ids[0]);
    g_transitions.RenderSettingsUI(transitionNode);

    // ImNodes selection, not g_selectedEffect. File load writes g_selectedEffect without
    // changing this, so it still only adds a node and does not start a blend.
    static int lastNodeEditorSelection = -1;
    if (selected_node_ids.size() == 1) {
        int node_id = selected_node_ids[0];
        Effect* current_effect = FindEffectById(node_id);
        if (current_effect) {
            const bool nodePickChanged = node_id != lastNodeEditorSelection;
            lastNodeEditorSelection = node_id;
            // Update global selected effect if it's different
            if (g_selectedEffect != current_effect) {
                if (nodePickChanged) {
                    bool outputBefore = false;
                    PresentedEffect(&outputBefore);
                    Effect* outgoing = g_transitions.IsActive() ? g_transitions.Target() : g_lastShownEffect;
                    const bool forceCut = ImGui::GetIO().KeyShift;
                    g_selectedEffect = current_effect;
                    bool outputAfter = false;
                    PresentedEffect(&outputAfter);
                    if (!outputBefore && !outputAfter)
                        g_transitions.OnPresentedChanged(outgoing, current_effect, forceCut);
                } else {
                    g_selectedEffect = current_effect;
                }
                // If it's a ShaderEffect, update the editor text
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    g_editor.SetText(se->GetShaderSource());
                    ClearErrorMarkers(); // Clear previous errors
                    // Optionally, display compile errors if any for this shader
                    const std::string& compileLog = se->GetCompileErrorLog();
                    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                        g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                    }
                }
            }

            // Display editable name
            char name_buffer[128];
            strncpy(name_buffer, current_effect->GetEffectName().c_str(), sizeof(name_buffer) - 1);
            name_buffer[sizeof(name_buffer) - 1] = '\0'; // Ensure null termination
            if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
                current_effect->SetEffectName(name_buffer);
            }

            // Display ID (read-only)
            ImGui::Text("ID: %d", current_effect->id);
            ImGui::Separator();

            // Render specific UI for the effect
            current_effect->RenderUI();

        } else {
            ImGui::Text("Error: Selected node ID %d not found.", node_id);
            g_selectedEffect = nullptr; // Clear global selection if node not found
            g_transitions.Cancel();
            lastNodeEditorSelection = -1;
        }
    } else if (selected_node_ids.size() > 1) {
        ImGui::Text("%zu nodes selected.", selected_node_ids.size()); // Use %zu for size_t
        ImGui::TextWrapped("Multi-selection actions not yet implemented. Please select a single node to view/edit properties.");
        // Consider clearing g_selectedEffect or setting it to a primary if ImNodes supports that
        // For now, let's clear it to avoid confusion with shader editor
        lastNodeEditorSelection = -1;
        if (g_selectedEffect != nullptr && std::find(selected_node_ids.begin(), selected_node_ids.end(), g_selectedEffect->id) == selected_node_ids.end()){
             g_selectedEffect = nullptr; // Clear if current g_selectedEffect is not in multi-selection
             g_transitions.Cancel();
        }
    } else {
        ImGui::Text("No node selected.");
        lastNodeEditorSelection = -1;
        if (g_selectedEffect != nullptr) { // If a node was previously selected
             g_selectedEffect = nullptr;
             g_transitions.Cancel();
        }
    }

    ImGui::Separator(); // Separator after Node Properties section

    // --- Instructions Panel ---
    ImGui::Text("Instructions");
    ImGui::Separator();
    ImGui::TextWrapped("Drag the divider: Resize Node Properties");
    ImGui::TextWrapped("Double-click the divider: Reset properties width");
    ImGui::TextWrapped("Right-click canvas: Add Node");
    ImGui::TextWrapped("Drag pin to pin: Create Link");
    ImGui::TextWrapped("Select Node: Edit Properties (above)");
    ImGui::TextWrapped("Shift+click node: Instant cut");
    ImGui::TextWrapped("Ctrl+Click Node: Break all links to/from node");
    ImGui::Separator();
    ImGui::TextWrapped("Middle-mouse drag: Pan canvas");
    ImGui::TextWrapped("Mouse wheel: Zoom canvas");

    ImGui::EndChild(); // End SidebarChild

    ImGui::End(); // End Node Editor window
}

void RenderConsoleWindow() {
    ImGui::SetNextWindowSize(ImVec2(800, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Console");
    if (ImGui::Button("Clear")) { g_consoleLog = ""; }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) { ImGui::SetClipboardText(g_consoleLog.c_str()); }
    ImGui::Separator();
    ImGui::TextWrapped("%s", g_consoleLog.c_str());
    ImGui::End();
}

void RenderShadertoyWindow() {
    if (!g_showShadertoyWindow) return;

    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Shadertoy", &g_showShadertoyWindow);

    static char shadertoyIdBuffer[256] = "";

    ImGui::InputTextWithHint("API Key", "Enter your key", g_shadertoyApiKeyBuffer, sizeof(g_shadertoyApiKeyBuffer), ImGuiInputTextFlags_Password);
    ImGui::InputTextWithHint("ID/URL", "e.g., Ms2SD1 or full URL", shadertoyIdBuffer, sizeof(shadertoyIdBuffer));
    
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button("Load", ImVec2(-1, 0))) { // Full width button
        std::string idOrUrl = shadertoyIdBuffer;
        if (!idOrUrl.empty()) {
            std::string shaderId = ShadertoyIntegration::ExtractId(idOrUrl);
            if (!shaderId.empty()) {
                g_consoleLog = "Fetching Shadertoy " + shaderId + "...";
                std::string fetchError;
                std::string apiKey = g_shadertoyApiKeyBuffer;
                std::string fetchedCode = ShadertoyIntegration::FetchCode(shaderId, apiKey, fetchError);

                if (!fetchedCode.empty()) {
                    auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT, true);
                    newEffect->name = "Shadertoy - " + shaderId;
                    newEffect->SetSourceFilePath("shadertoy://" + shaderId);
                    newEffect->LoadShaderFromSource(fetchedCode);
                    newEffect->SetShadertoyMode(true);
                    newEffect->Load();

                    const std::string& compileLog = newEffect->GetCompileErrorLog();
                    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                        g_consoleLog = "Shadertoy '" + shaderId + "' fetched, but compilation failed. Log:\n" + compileLog;
                    } else {
                        g_editor.SetText(newEffect->GetShaderSource());
                        ClearErrorMarkers();
                        g_scene.push_back(std::move(newEffect));                        g_consoleLog = "Shadertoy '" + shaderId + "' fetched and applied!";
                        g_showShadertoyWindow = false; // Close window on success
                    }
                } else {
                    g_consoleLog = fetchError;
                    if (g_consoleLog.empty()) {
                        g_consoleLog = "Failed to retrieve code for Shadertoy ID: " + shaderId + ". Check API key and ID.";
                    }
                }
            } else {
                g_consoleLog = "Invalid Shadertoy ID or URL format.";
            }
        } else {
            g_consoleLog = "Please enter a Shadertoy ID or URL.";
        }
    }
    ImGui::PopStyleColor(4);

    ImGui::End();
}

void RenderFpsMeter() {
    if (!g_showFpsMeter) return;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 window_pos, window_pos_pivot;
    window_pos.x = work_pos.x + work_size.x - PAD;
    window_pos.y = work_pos.y + PAD;
    window_pos_pivot.x = 1.0f;
    window_pos_pivot.y = 0.0f;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    if (ImGui::Begin("FPS Overlay", nullptr, window_flags)) {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    }
    ImGui::End();
}

void RenderCameraHelpText() {
    if (!g_cameraControlsEnabled) return;

    const float DISTANCE = 30.0f;
    ImVec2 window_pos = ImVec2(DISTANCE, DISTANCE);
    ImVec2 window_pos_pivot = ImVec2(0.0f, 0.0f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Camera Controls Help", NULL, window_flags)) {
        ImGui::Text("Camera & Light Controls");
        ImGui::Separator();
        ImGui::Text("Orbit Camera: Left-Click + Drag");
        ImGui::Text("Pan Camera: Right-Click + Drag");
        ImGui::Text("Zoom Camera: Scroll Wheel");
        ImGui::Text("Orbit Light: Ctrl + Left-Click + Drag");
    }
    ImGui::End();
}

enum class TransportIcon { Play, Pause, Stop };

// Fixed-size transport button. The active one is the full color; the others keep a darker shade.
static bool TransportIconButton(const char* id, bool selected, const ImVec4& color, TransportIcon icon) {
    const ImVec2 size(40.0f, 26.0f);
    const ImVec4 face = selected ? color : ImVec4(color.x * 0.38f, color.y * 0.38f, color.z * 0.38f, 1.0f);
    const ImVec4 hover = selected
        ? ImVec4(ImMin(color.x + 0.10f, 1.0f), ImMin(color.y + 0.10f, 1.0f), ImMin(color.z + 0.10f, 1.0f), 1.0f)
        : ImVec4(color.x * 0.55f, color.y * 0.55f, color.z * 0.55f, 1.0f);
    const ImVec4 held = ImVec4(color.x * 0.78f, color.y * 0.78f, color.z * 0.78f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, face);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, held);
    const bool pressed = ImGui::Button(id, size);
    ImGui::PopStyleColor(3);

    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    const ImU32 col = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, selected ? 1.0f : 0.88f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (icon == TransportIcon::Play) {
        draw->AddTriangleFilled(ImVec2(c.x - 4.0f, c.y - 5.5f), ImVec2(c.x - 4.0f, c.y + 5.5f), ImVec2(c.x + 6.0f, c.y), col);
    } else if (icon == TransportIcon::Pause) {
        draw->AddRectFilled(ImVec2(c.x - 5.5f, c.y - 5.0f), ImVec2(c.x - 2.0f, c.y + 5.0f), col, 1.0f);
        draw->AddRectFilled(ImVec2(c.x + 2.0f, c.y - 5.0f), ImVec2(c.x + 5.5f, c.y + 5.0f), col, 1.0f);
    } else {
        draw->AddRectFilled(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), col, 1.5f);
    }
    return pressed;
}

static const ImVec4 kEqColorGreen(0.20f, 0.86f, 0.22f, 1.0f);
static const ImVec4 kEqColorYellow(0.95f, 0.85f, 0.15f, 1.0f);
static const ImVec4 kEqColorOrange(1.00f, 0.55f, 0.10f, 1.0f);
static const ImVec4 kEqColorRed(0.95f, 0.18f, 0.12f, 1.0f);
static const float kEqOrangeStopDb = 6.0f;
static const ImVec4 kEqOffGrab(0.38f, 0.38f, 0.38f, 1.0f);
static const ImVec4 kEqOffGrabActive(0.62f, 0.62f, 0.62f, 1.0f);

// -12..0 green to yellow, 0 yellow, 0..+6 yellow to orange, +6..+12 orange to red.
static ImVec4 EqDbColor(float db) {
    const float min_db = AudioSystem::kEqualizerMinDb;
    const float max_db = AudioSystem::kEqualizerMaxDb;
    db = ImClamp(db, min_db, max_db);
    if (db < 0.0f) {
        const float span = 0.0f - min_db;
        const float t = span > 0.0f ? (db - min_db) / span : 1.0f;
        return ImLerp(kEqColorGreen, kEqColorYellow, ImSaturate(t));
    }
    if (db < kEqOrangeStopDb) {
        return ImLerp(kEqColorYellow, kEqColorOrange, ImSaturate(db / kEqOrangeStopDb));
    }
    const float span = max_db - kEqOrangeStopDb;
    const float t = span > 0.0f ? (db - kEqOrangeStopDb) / span : 1.0f;
    return ImLerp(kEqColorOrange, kEqColorRed, ImSaturate(t));
}

static ImVec4 EqDbActiveColor(const ImVec4& color) {
    return ImLerp(color, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.22f);
}

static float EqCatmullRom(float p0, float p1, float p2, float p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

static ImVec2 EqCatmullRom(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t) {
    return ImVec2(EqCatmullRom(p0.x, p1.x, p2.x, p3.x, t), EqCatmullRom(p0.y, p1.y, p2.y, p3.y, t));
}

// Thin bar inside the track from the 0 dB centre to the grab. Matches ImGui's vertical slider insets.
static void DrawEqLevelFill(const ImVec2& item_min, const ImVec2& item_max, float db, const ImVec4& color) {
    if (std::fabs(db) < 0.05f) {
        return;
    }
    const float grab_padding = 2.0f;
    const float frame_h = item_max.y - item_min.y;
    const float slider_sz = frame_h - grab_padding * 2.0f;
    if (slider_sz < 1.0f) {
        return;
    }
    const float grab_sz = ImMin(ImGui::GetStyle().GrabMinSize, slider_sz);
    const float usable_top = item_min.y + grab_padding + grab_sz * 0.5f;
    const float usable_bottom = item_max.y - grab_padding - grab_sz * 0.5f;
    const float min_db = AudioSystem::kEqualizerMinDb;
    const float max_db = AudioSystem::kEqualizerMaxDb;
    const float span = max_db - min_db;
    const float t = span > 0.0f ? ImSaturate((db - min_db) / span) : 0.5f;
    const float grab_pos = ImLerp(usable_top, usable_bottom, 1.0f - t);
    const float zero_pos = (usable_top + usable_bottom) * 0.5f;

    float y0 = db > 0.0f ? grab_pos + grab_sz * 0.5f : zero_pos;
    float y1 = db > 0.0f ? zero_pos : grab_pos - grab_sz * 0.5f;
    const float y_min = item_min.y + 1.0f;
    const float y_max = item_max.y - 1.0f;
    y0 = ImClamp(y0, y_min, y_max);
    y1 = ImClamp(y1, y_min, y_max);
    if (y1 - y0 < 1.0f) {
        return;
    }

    const float inner_l = item_min.x + grab_padding;
    const float inner_r = item_max.x - grab_padding;
    const float bar_w = ImMin(3.0f, ImMax(1.0f, inner_r - inner_l));
    const float mid_x = (item_min.x + item_max.x) * 0.5f;
    const ImVec2 fill_min(mid_x - bar_w * 0.5f, y0);
    const ImVec2 fill_max(mid_x + bar_w * 0.5f, y1);
    ImVec4 fill = color;
    fill.w = 0.40f;
    ImGui::GetWindowDrawList()->AddRectFilled(fill_min, fill_max, ImGui::GetColorU32(fill));
}

static void DrawEqResponseGraph(const ImVec2& panel_min, const ImVec2& panel_max, const float* center_x, const float* band_db, int band_count, float preamp_db, bool eq_on) {
    const float border_px = 1.0f;
    if (band_count < 2 || panel_max.x <= panel_min.x + border_px * 2.0f || panel_max.y <= panel_min.y + border_px * 2.0f) {
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    // One pixel of slack so the border's anti-aliased edge is not shaved off.
    ImGui::PushClipRect(ImVec2(panel_min.x - 1.0f, panel_min.y - 1.0f), ImVec2(panel_max.x + 1.0f, panel_max.y + 1.0f), true);

    const float panel_rounding = 2.0f;
    const ImU32 bg = ImGui::GetColorU32(ImVec4(0.03f, 0.07f, 0.03f, 1.0f));
    draw->AddRectFilled(panel_min, panel_max, bg, panel_rounding);

    // Lines sit inside the 1 px frame so the stroke stays visible on top.
    const ImVec2 inner_min(panel_min.x + border_px, panel_min.y + border_px);
    const ImVec2 inner_max(panel_max.x - border_px, panel_max.y - border_px);
    ImGui::PushClipRect(inner_min, inner_max, true);

    const ImU32 grid_col = eq_on ? IM_COL32(90, 150, 80, 80) : IM_COL32(150, 150, 150, 55);
    const float min_db = AudioSystem::kEqualizerMinDb;
    const float max_db = AudioSystem::kEqualizerMaxDb;
    const float db_span = max_db - min_db;
    const auto db_to_y = [&](float db) {
        const float t = db_span > 0.0f ? ImSaturate((db - min_db) / db_span) : 0.5f;
        return ImLerp(inner_max.y, inner_min.y, t);
    };

    const float zero_y = db_to_y(0.0f);
    draw->AddLine(ImVec2(inner_min.x, zero_y), ImVec2(inner_max.x, zero_y), grid_col, 1.0f);
    for (int i = 0; i < band_count; ++i) {
        const float x = center_x[i];
        draw->AddLine(ImVec2(x, inner_min.y), ImVec2(x, inner_max.y), grid_col, 1.0f);
    }

    ImVec4 preamp_color = eq_on ? EqDbColor(preamp_db) : ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    preamp_color.w = eq_on ? 0.35f : 0.28f;
    const float preamp_y = db_to_y(preamp_db);
    draw->AddLine(ImVec2(inner_min.x, preamp_y), ImVec2(inner_max.x, preamp_y), ImGui::GetColorU32(preamp_color), 1.0f);

    ImVec2 points[AudioSystem::kEqualizerBandCount];
    float dbs[AudioSystem::kEqualizerBandCount];
    for (int i = 0; i < band_count; ++i) {
        dbs[i] = band_db[i];
        points[i] = ImVec2(center_x[i], db_to_y(dbs[i]));
    }

    const int segments = 8;
    const ImU32 grey_curve = IM_COL32(170, 170, 170, 150);
    const auto stroke_curve = [&](float thickness, bool glow) {
        for (int span = 0; span < band_count - 1; ++span) {
            const int i0 = span > 0 ? span - 1 : 0;
            const int i3 = (span + 2 < band_count) ? span + 2 : band_count - 1;
            ImVec2 previous = points[span];
            for (int step = 1; step <= segments; ++step) {
                const float t = static_cast<float>(step) / static_cast<float>(segments);
                const ImVec2 next = EqCatmullRom(points[i0], points[span], points[span + 1], points[i3], t);
                const float mid_t = (static_cast<float>(step) - 0.5f) / static_cast<float>(segments);
                const float mid_db = EqCatmullRom(dbs[i0], dbs[span], dbs[span + 1], dbs[i3], mid_t);
                ImU32 col = grey_curve;
                if (eq_on) {
                    ImVec4 color = EqDbColor(mid_db);
                    if (glow) {
                        color.w = 0.25f;
                    } else {
                        color.x = ImMin(1.0f, color.x + 0.10f);
                        color.y = ImMin(1.0f, color.y + 0.10f);
                        color.z = ImMin(1.0f, color.z + 0.05f);
                    }
                    col = ImGui::GetColorU32(color);
                }
                draw->AddLine(previous, next, col, thickness);
                previous = next;
            }
        }
    };
    if (eq_on) {
        stroke_curve(6.5f, true);
    }
    stroke_curve(3.0f, false);

    ImGui::PopClipRect();
    const ImVec2 border_min(panel_min.x + border_px * 0.5f, panel_min.y + border_px * 0.5f);
    const ImVec2 border_max(panel_max.x - border_px * 0.5f, panel_max.y - border_px * 0.5f);
    const ImU32 border_col = ImGui::GetColorU32(eq_on ? ImVec4(0.25f, 0.45f, 0.25f, 1.0f) : ImVec4(0.42f, 0.42f, 0.42f, 1.0f));
    draw->AddRect(border_min, border_max, border_col, panel_rounding, 0, border_px);
    ImGui::PopClipRect();
}

// Small-caps card title with a short green tick on the left.
static void DrawCardTitle(const char* title) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float text_h = ImGui::GetTextLineHeight();
    const float tick_w = 3.0f;
    draw->AddRectFilled(ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + tick_w, p.y + text_h - 1.0f), ImGui::GetColorU32(ImVec4(0.35f, 0.82f, 0.38f, 1.0f)), 1.0f);

    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const ImU32 col = ImGui::GetColorU32(ImVec4(0.76f, 0.78f, 0.82f, 1.0f));
    float x = p.x + tick_w + 7.0f;
    for (const char* c = title; *c != '\0'; ++c) {
        char glyph[2] = {*c, '\0'};
        draw->AddText(font, size, ImVec2(x, p.y), col, glyph);
        x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph).x + 1.15f;
    }
    ImGui::Dummy(ImVec2(ImMax(1.0f, x - p.x), text_h));
}

// slider_h is the fader travel. thin_labels switches the frequency captions to the small font.
// The band-area rect is measured once and shared by the curve and the sliders.
static void RenderGraphicEqualizer(float slider_h, bool thin_labels) {
    slider_h = ImMax(150.0f, slider_h);
    const float graph_h = 50.0f;
    const int band_count = AudioSystem::kEqualizerBandCount;
    const ImGuiStyle& style = ImGui::GetStyle();

    bool eq_on = g_audioSystem.IsEqualizerEnabled();
    const float on_w = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("On").x;
    const float reset_w = ImGui::CalcTextSize("Reset").x + style.FramePadding.x * 2.0f;
    const float controls_w = on_w + style.ItemSpacing.x + reset_w;
    ImGui::AlignTextToFramePadding();
    DrawCardTitle("EQUALIZER");
    ImGui::SameLine();
    const float header_rest = ImGui::GetContentRegionAvail().x;
    if (header_rest > controls_w) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + header_rest - controls_w);
    }
    if (ImGui::Checkbox("On##Equalizer", &eq_on)) {
        g_audioSystem.SetEqualizerEnabled(eq_on);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##Equalizer")) {
        g_audioSystem.ResetEqualizer();
    }

    const float avail = ImGui::GetContentRegionAvail().x;
    const float scale_w = ImMax(ImGui::CalcTextSize("+12").x, ImGui::CalcTextSize("-12").x);
    const float pre_w = 36.0f;
    const float divider_slot = 16.0f;
    const float bands_w = ImMax(1.0f, avail - scale_w - style.ItemSpacing.x - pre_w - divider_slot);
    const float col_w = bands_w / static_cast<float>(band_count);
    const float band_slider_w = ImMin(44.0f, ImMax(12.0f, col_w - 8.0f));

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float scale_x = origin.x;
    const float pre_x = scale_x + scale_w + style.ItemSpacing.x;
    const float bands_x = pre_x + pre_w + divider_slot;
    const float divider_x = pre_x + pre_w + divider_slot * 0.5f;
    float center_x[AudioSystem::kEqualizerBandCount];
    for (int i = 0; i < band_count; ++i) {
        center_x[i] = bands_x + (static_cast<float>(i) + 0.5f) * col_w;
    }
    const ImVec2 graph_min(bands_x, origin.y);
    const ImVec2 graph_max(bands_x + bands_w, origin.y + graph_h);
    ImGui::Dummy(ImVec2(avail, graph_h));
    const float slider_y = ImGui::GetCursorScreenPos().y;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.03f, 0.07f, 0.03f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.16f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.20f, 0.10f, 1.0f));

    float preamp_db = g_audioSystem.GetEqualizerPreampDb();
    const ImVec4 preamp_grab = eq_on ? EqDbColor(preamp_db) : kEqOffGrab;
    const ImVec4 preamp_grab_active = eq_on ? EqDbActiveColor(preamp_grab) : kEqOffGrabActive;
    ImGui::SetCursorScreenPos(ImVec2(pre_x, slider_y));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, preamp_grab);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, preamp_grab_active);
    if (ImGui::VSliderFloat("##EqPreamp", ImVec2(pre_w, slider_h), &preamp_db, AudioSystem::kEqualizerMinDb, AudioSystem::kEqualizerMaxDb, "")) {
        g_audioSystem.SetEqualizerPreampDb(preamp_db);
    }
    const ImVec2 preamp_rect_min = ImGui::GetItemRectMin();
    const ImVec2 preamp_rect_max = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Preamp: %+.1f dB", g_audioSystem.GetEqualizerPreampDb());
    }
    ImGui::PopStyleColor(2);
    if (eq_on) {
        DrawEqLevelFill(preamp_rect_min, preamp_rect_max, preamp_db, EqDbColor(preamp_db));
    }

    float band_db[AudioSystem::kEqualizerBandCount];
    for (int i = 0; i < band_count; ++i) {
        ImGui::PushID(i);
        band_db[i] = g_audioSystem.GetEqualizerBandDb(i);
        const ImVec4 band_grab = eq_on ? EqDbColor(band_db[i]) : kEqOffGrab;
        const ImVec4 band_grab_active = eq_on ? EqDbActiveColor(band_grab) : kEqOffGrabActive;
        ImGui::SetCursorScreenPos(ImVec2(center_x[i] - band_slider_w * 0.5f, slider_y));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, band_grab);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, band_grab_active);
        if (ImGui::VSliderFloat("##EqBand", ImVec2(band_slider_w, slider_h), &band_db[i], AudioSystem::kEqualizerMinDb, AudioSystem::kEqualizerMaxDb, "")) {
            g_audioSystem.SetEqualizerBandDb(i, band_db[i]);
        }
        const ImVec2 band_rect_min = ImGui::GetItemRectMin();
        const ImVec2 band_rect_max = ImGui::GetItemRectMax();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%.0f Hz: %+.1f dB", AudioSystem::kEqualizerBands[i].hz, g_audioSystem.GetEqualizerBandDb(i));
        }
        ImGui::PopStyleColor(2);
        if (eq_on) {
            DrawEqLevelFill(band_rect_min, band_rect_max, band_db[i], EqDbColor(band_db[i]));
        }
        ImGui::PopID();
    }
    ImGui::PopStyleColor(3);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 scale_col = ImGui::GetColorU32(ImVec4(0.55f, 0.86f, 0.40f, eq_on ? 1.0f : 0.45f));
    const float text_h = ImGui::GetTextLineHeight();
    draw->AddText(ImVec2(scale_x, slider_y), scale_col, "+12");
    draw->AddText(ImVec2(scale_x, slider_y + (slider_h - text_h) * 0.5f), scale_col, "0");
    draw->AddText(ImVec2(scale_x, slider_y + slider_h - text_h), scale_col, "-12");
    draw->AddLine(ImVec2(divider_x, slider_y + 2.0f), ImVec2(divider_x, slider_y + slider_h - 2.0f), IM_COL32(90, 110, 90, 140), 1.0f);

    if (g_audioSmallFont != nullptr) {
        ImGui::PushFont(g_audioSmallFont);
    }
    char pre_buf[32];
    snprintf(pre_buf, sizeof(pre_buf), "%+.1f", preamp_db);
    const float db_w = ImGui::CalcTextSize("dB").x;
    const float val_w = ImGui::CalcTextSize(pre_buf).x;
    const float read_x = pre_x + (pre_w - ImMax(db_w, val_w)) * 0.5f;
    const float read_y = graph_min.y + graph_h - ImGui::GetTextLineHeight() * 2.0f - 2.0f;
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(read_x + (ImMax(db_w, val_w) - db_w) * 0.5f, read_y), IM_COL32(140, 150, 140, 200), "dB");
    const ImVec4 pre_col = eq_on ? EqDbColor(preamp_db) : ImVec4(0.62f, 0.62f, 0.62f, 1.0f);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(read_x + (ImMax(db_w, val_w) - val_w) * 0.5f, read_y + ImGui::GetTextLineHeight()), ImGui::GetColorU32(pre_col), pre_buf);
    if (g_audioSmallFont != nullptr) {
        ImGui::PopFont();
    }

    DrawEqResponseGraph(graph_min, graph_max, center_x, band_db, band_count, preamp_db, eq_on);

    const float label_y = slider_y + slider_h + style.ItemSpacing.y;
    const bool use_small = thin_labels && g_audioSmallFont != nullptr;
    if (use_small) {
        ImGui::PushFont(g_audioSmallFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.90f, 0.48f, eq_on ? 1.0f : 0.45f));
    auto place_label = [&](const char* label, float center) {
        const float label_w = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorScreenPos(ImVec2(center - label_w * 0.5f, label_y));
        ImGui::TextUnformatted(label);
    };
    place_label("Pre", pre_x + pre_w * 0.5f);
    for (int i = 0; i < band_count; ++i) {
        place_label(AudioSystem::kEqualizerBands[i].label, center_x[i]);
    }
    ImGui::PopStyleColor();
    if (use_small) {
        ImGui::PopFont();
    }
}

// Fixed hue across 20 Hz..20 kHz: magenta/red, through green/cyan, out to blue/violet.
static ImVec4 SpectrumHueColor(float freq_t, float value, float whiten) {
    float hue = freq_t * 0.80f + 0.92f;
    hue -= std::floor(hue);
    float r = 0.0f, g = 0.0f, b = 0.0f;
    ImGui::ColorConvertHSVtoRGB(hue, 0.90f, ImSaturate(value), r, g, b);
    const float w = ImSaturate(whiten);
    return ImVec4(r + (1.0f - r) * w, g + (1.0f - g) * w, b + (1.0f - b) * w, 1.0f);
}

// Bin 0 is DC and is never read. Positions below bin 1 fade up from silence so
// sub-bin bass bars don't all copy the same sample.
static float SpectrumBinMagnitude(const float* fft, int bin_count, float bin_pos) {
    if (fft == nullptr || bin_count < 2) {
        return 0.0f;
    }
    if (bin_pos <= 1.0f) {
        return fft[1] * ImSaturate(bin_pos);
    }
    const float last = static_cast<float>(bin_count - 1);
    if (bin_pos >= last) {
        return fft[bin_count - 1];
    }
    const int i0 = static_cast<int>(bin_pos);
    const float t = bin_pos - static_cast<float>(i0);
    return fft[i0] * (1.0f - t) + fft[i0 + 1] * t;
}

// Max magnitude of the bins inside [bin_lo, bin_hi]. Ranges narrower than one
// bin interpolate between neighbours instead of stair-stepping.
static float SpectrumBarMagnitude(const float* fft, int bin_count, float bin_lo, float bin_hi) {
    if (fft == nullptr || bin_count < 2 || !(bin_hi > bin_lo)) {
        return 0.0f;
    }
    const float mid = 0.5f * (bin_lo + bin_hi);
    if ((bin_hi - bin_lo) < 1.0f || bin_hi <= 1.0f) {
        return SpectrumBinMagnitude(fft, bin_count, mid);
    }
    int first = static_cast<int>(std::ceil(bin_lo));
    int last = static_cast<int>(std::floor(bin_hi));
    if (first < 1) {
        first = 1;
    }
    if (last >= bin_count) {
        last = bin_count - 1;
    }
    if (last < first) {
        return SpectrumBinMagnitude(fft, bin_count, ImMax(mid, 1.0f));
    }
    float mag = 0.0f;
    for (int i = first; i <= last; ++i) {
        if (fft[i] > mag) {
            mag = fft[i];
        }
    }
    return mag;
}

static void RenderSpectrumMonitor(float panel_width, float height) {
    static constexpr int kSpecBars = 64;
    static float levels[kSpecBars] = {};
    static float peaks[kSpecBars] = {};
    static float peak_vel[kSpecBars] = {};
    static float peak_hold[kSpecBars] = {};

    const float plot_h = ImMax(48.0f, height);
    ImGui::InvisibleButton("##SpectrumMonitor", ImVec2(ImMax(1.0f, panel_width), plot_h));
    const ImVec2 panel_min = ImGui::GetItemRectMin();
    const ImVec2 panel_max = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    const float* fft = nullptr;
    int bin_count = 0;
    const auto& fft_data = g_audioSystem.GetFFTData();
    if (!fft_data.empty()) {
        fft = fft_data.data();
        bin_count = static_cast<int>(fft_data.size());
    }
    const ma_uint32 sample_rate_u = g_audioSystem.GetCurrentInputSampleRate();
    const float sample_rate = sample_rate_u > 0 ? static_cast<float>(sample_rate_u) : 48000.0f;
    const float bins_per_hz = static_cast<float>(AudioSystem::FFT_SIZE) / sample_rate;

    float dt = ImGui::GetIO().DeltaTime;
    if (!(dt > 0.0f)) {
        dt = 1.0f / 60.0f;
    }
    dt = ImMin(dt, 0.050f);
    const float release = 1.0f - std::exp(-dt / 0.180f);
    const float now = static_cast<float>(ImGui::GetTime());

    const float f_min = 20.0f;
    const float f_max = 20000.0f;
    const float log_min = std::log(f_min);
    const float log_span = std::log(f_max) - log_min;
    const auto hz_at = [&](float t) {
        return std::exp(log_min + log_span * ImSaturate(t));
    };

    float center_hz[kSpecBars];
    float bass_sum = 0.0f;
    int bass_n = 0;
    float max_level = 0.0f;
    for (int i = 0; i < kSpecBars; ++i) {
        const float f0 = hz_at(static_cast<float>(i) / static_cast<float>(kSpecBars));
        const float f1 = hz_at(static_cast<float>(i + 1) / static_cast<float>(kSpecBars));
        const float fc = hz_at((static_cast<float>(i) + 0.5f) / static_cast<float>(kSpecBars));
        center_hz[i] = fc;

        float mag = 0.0f;
        if (f0 < sample_rate * 0.5f) {
            const float bin_lo = f0 * bins_per_hz;
            const float bin_hi = ImMin(f1, sample_rate * 0.5f) * bins_per_hz;
            mag = SpectrumBarMagnitude(fft, bin_count, bin_lo, bin_hi);
        }
        if (!std::isfinite(mag) || mag < 0.0f) {
            mag = 0.0f;
        }
        const float tilt = (fc > 1000.0f) ? 3.0f * std::log2(fc / 1000.0f) : 0.0f;
        const float db = 20.0f * std::log10(mag / AudioSystem::BAND_REFERENCE + 1.0e-6f) + tilt;
        const float level = ImSaturate((db + 60.0f) / 60.0f);

        if (level >= levels[i]) {
            levels[i] = level;
        } else {
            levels[i] += (level - levels[i]) * release;
        }
        if (!std::isfinite(levels[i])) {
            levels[i] = 0.0f;
        }

        if (levels[i] >= peaks[i]) {
            peaks[i] = levels[i];
            peak_vel[i] = 0.0f;
            peak_hold[i] = 0.40f;
        } else if (peak_hold[i] > 0.0f) {
            peak_hold[i] = ImMax(0.0f, peak_hold[i] - dt);
        } else {
            peak_vel[i] += 3.4f * dt;
            peaks[i] -= peak_vel[i] * dt;
            if (peaks[i] <= levels[i]) {
                peaks[i] = levels[i];
                peak_vel[i] = 0.0f;
            }
        }

        if (fc <= 160.0f) {
            bass_sum += levels[i];
            ++bass_n;
        }
        if (levels[i] > max_level) {
            max_level = levels[i];
        }
    }
    const float bass = (bass_n > 0) ? bass_sum / static_cast<float>(bass_n) : 0.0f;

    if (panel_max.x <= panel_min.x + 4.0f || panel_max.y <= panel_min.y + 8.0f) {
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImGui::PushClipRect(ImVec2(panel_min.x - 4.0f, panel_min.y - 4.0f), ImVec2(panel_max.x + 4.0f, panel_max.y + 4.0f), true);

    const float panel_rounding = 2.0f;
    draw->AddRectFilled(panel_min, panel_max, ImGui::GetColorU32(ImVec4(0.03f, 0.07f, 0.03f, 1.0f)), panel_rounding);

    const float border_room = 1.0f;
    const ImVec2 inner_min(panel_min.x + border_room, panel_min.y + border_room);
    const ImVec2 inner_max(panel_max.x - border_room, panel_max.y - border_room);
    ImGui::PushClipRect(inner_min, inner_max, true);

    const float label_h = ImGui::GetTextLineHeight();
    const float plot_top = inner_min.y + label_h;
    const float plot_bot = inner_max.y;
    const float plot_left = inner_min.x;
    const float plot_right = inner_max.x;
    const bool plot_ok = (plot_bot > plot_top + 8.0f) && (plot_right > plot_left + 4.0f);

    int hover_bar = -1;
    if (plot_ok) {
        const float bar_area = (plot_bot - plot_top) * 0.74f;
        const float baseline = plot_top + bar_area;
        const float bar_span = ImMax(1.0f, bar_area - 2.0f);
        const float refl_span = ImMax(1.0f, plot_bot - baseline - 1.0f);
        const float slot = (plot_right - plot_left) / static_cast<float>(kSpecBars);
        const float bar_w = ImMax(1.0f, slot - 1.0f);
        const ImU32 grid_col = IM_COL32(80, 140, 70, 55);
        const ImU32 base_col = IM_COL32(130, 190, 110, 100);

        ImGui::PushClipRect(ImVec2(inner_min.x, plot_top), inner_max, true);

        const float grid_hz[] = {50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f};
        float grid_x[IM_ARRAYSIZE(grid_hz)];
        for (int g = 0; g < IM_ARRAYSIZE(grid_hz); ++g) {
            const float t = (std::log(grid_hz[g]) - log_min) / log_span;
            grid_x[g] = ImLerp(plot_left, plot_right, ImSaturate(t));
            draw->AddLine(ImVec2(grid_x[g], plot_top), ImVec2(grid_x[g], plot_bot), grid_col, 1.0f);
        }
        draw->AddLine(ImVec2(plot_left, baseline), ImVec2(plot_right, baseline), base_col, 1.0f);

        ImVec2 tops[kSpecBars];
        for (int i = 0; i < kSpecBars; ++i) {
            const float freq_t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kSpecBars);
            const float cx = plot_left + (static_cast<float>(i) + 0.5f) * slot;
            const float x0 = cx - bar_w * 0.5f;
            const float x1 = cx + bar_w * 0.5f;
            const float y_top = baseline - levels[i] * bar_span;
            tops[i] = ImVec2(cx, y_top);

            const ImVec4 top_col = SpectrumHueColor(freq_t, 0.62f + 0.38f * levels[i], levels[i] * levels[i] * 0.80f);
            const ImVec4 bot_col = SpectrumHueColor(freq_t, 0.18f + 0.12f * levels[i], 0.0f);

            if (levels[i] > 0.01f) {
                ImVec4 glow = top_col;
                glow.w = 0.15f;
                draw->AddRectFilled(ImVec2(x0 - 1.5f, y_top), ImVec2(x1 + 1.5f, baseline), ImGui::GetColorU32(glow));
                if (levels[i] >= 0.80f) {
                    const float hot = (levels[i] - 0.80f) / 0.20f;
                    ImVec4 hot_glow = top_col;
                    hot_glow.w = 0.15f + hot * 0.12f;
                    const float extra = 3.0f + hot * 7.0f;
                    draw->AddRectFilled(ImVec2(x0 - 3.0f, y_top - extra), ImVec2(x1 + 3.0f, y_top + 4.0f), ImGui::GetColorU32(hot_glow));
                }
                draw->AddRectFilledMultiColor(
                    ImVec2(x0, y_top), ImVec2(x1, baseline),
                    ImGui::GetColorU32(top_col), ImGui::GetColorU32(top_col),
                    ImGui::GetColorU32(bot_col), ImGui::GetColorU32(bot_col));

                ImVec4 refl_top = bot_col;
                refl_top.w = 0.25f;
                ImVec4 refl_bot = bot_col;
                refl_bot.w = 0.0f;
                const float refl_h = levels[i] * refl_span;
                draw->AddRectFilledMultiColor(
                    ImVec2(x0, baseline + 1.0f), ImVec2(x1, baseline + 1.0f + refl_h),
                    ImGui::GetColorU32(refl_top), ImGui::GetColorU32(refl_top),
                    ImGui::GetColorU32(refl_bot), ImGui::GetColorU32(refl_bot));
            }

            if (peaks[i] > 0.02f) {
                const float peak_y = baseline - peaks[i] * bar_span;
                ImVec4 cap = SpectrumHueColor(freq_t, 1.0f, 0.75f);
                ImVec4 cap_glow = cap;
                cap_glow.w = 0.30f;
                draw->AddLine(ImVec2(x0 - 0.5f, peak_y), ImVec2(x1 + 0.5f, peak_y), ImGui::GetColorU32(cap_glow), 5.0f);
                draw->AddLine(ImVec2(x0, peak_y), ImVec2(x1, peak_y), ImGui::GetColorU32(cap), 2.0f);
            }
        }

        if (max_level > 0.02f) {
            const auto stroke_curve = [&](float thickness, float alpha, float whiten) {
                const int steps = 4;
                for (int span = 0; span < kSpecBars - 1; ++span) {
                    const int i0 = span > 0 ? span - 1 : 0;
                    const int i3 = (span + 2 < kSpecBars) ? span + 2 : kSpecBars - 1;
                    ImVec2 previous = tops[span];
                    for (int step = 1; step <= steps; ++step) {
                        const float t = static_cast<float>(step) / static_cast<float>(steps);
                        const ImVec2 next = EqCatmullRom(tops[i0], tops[span], tops[span + 1], tops[i3], t);
                        const float mid_t = (static_cast<float>(step) - 0.5f) / static_cast<float>(steps);
                        const float mid_level = ImSaturate(EqCatmullRom(levels[i0], levels[span], levels[span + 1], levels[i3], mid_t));
                        if (mid_level < 0.02f) {
                            previous = next;
                            continue;
                        }
                        const float freq_t = ImSaturate((static_cast<float>(span) + mid_t) / static_cast<float>(kSpecBars - 1));
                        ImVec4 color = SpectrumHueColor(freq_t, 0.70f + 0.30f * mid_level, whiten);
                        color.x = ImMin(1.0f, color.x + 0.08f);
                        color.y = ImMin(1.0f, color.y + 0.08f);
                        color.z = ImMin(1.0f, color.z + 0.04f);
                        color.w = alpha * (0.35f + 0.65f * mid_level);
                        draw->AddLine(previous, next, ImGui::GetColorU32(color), thickness);
                        previous = next;
                    }
                }
            };

            stroke_curve(6.5f, 0.22f, 0.0f);
            stroke_curve(3.0f, 1.0f, 0.28f);
        }

        ImGui::PopClipRect();

        const char* grid_text[] = {"50", "100", "200", "500", "1k", "2k", "5k", "10k"};
        const ImU32 label_col = IM_COL32(150, 190, 140, 170);
        for (int g = 0; g < IM_ARRAYSIZE(grid_text); ++g) {
            const ImVec2 text_sz = ImGui::CalcTextSize(grid_text[g]);
            const float tx = ImClamp(grid_x[g] - text_sz.x * 0.5f, inner_min.x, ImMax(inner_min.x, inner_max.x - text_sz.x));
            draw->AddText(ImVec2(tx, inner_min.y), label_col, grid_text[g]);
        }

        if (hovered && slot > 0.0f) {
            hover_bar = static_cast<int>((mouse.x - plot_left) / slot);
            hover_bar = ImClamp(hover_bar, 0, kSpecBars - 1);
        }
    }

    ImGui::PopClipRect();

    const float wobble = 0.82f + 0.18f * std::sin(now * 3.5f);
    const float breath = bass * wobble;
    const float border_px = 1.0f + breath * 2.2f;
    const ImVec2 border_min(panel_min.x + border_px * 0.5f, panel_min.y + border_px * 0.5f);
    const ImVec2 border_max(panel_max.x - border_px * 0.5f, panel_max.y - border_px * 0.5f);
    const ImU32 border_col = ImGui::GetColorU32(ImLerp(ImVec4(0.25f, 0.45f, 0.25f, 1.0f), ImVec4(0.75f, 1.0f, 0.70f, 1.0f), ImSaturate(breath)));
    draw->AddRect(border_min, border_max, border_col, panel_rounding, 0, border_px);

    ImGui::PopClipRect();

    if (hover_bar >= 0) {
        const float hz = center_hz[hover_bar];
        const float shown_db = -60.0f + levels[hover_bar] * 60.0f;
        if (hz >= 1000.0f) {
            ImGui::SetTooltip("%.2f kHz   %+.1f dB", hz / 1000.0f, shown_db);
        } else {
            ImGui::SetTooltip("%.0f Hz   %+.1f dB", hz, shown_db);
        }
    }
}

// MM:SS, or H:MM:SS once the file runs past an hour. Truncates, matching a player clock.
static void FormatPlaybackClock(char* buf, size_t buf_size, float seconds) {
    if (!(seconds > 0.0f)) {
        seconds = 0.0f;
    }
    int whole = static_cast<int>(seconds);
    const int hours = whole / 3600;
    whole %= 3600;
    const int minutes = whole / 60;
    const int secs = whole % 60;
    if (hours > 0) {
        snprintf(buf, buf_size, "%d:%02d:%02d", hours, minutes, secs);
    } else {
        snprintf(buf, buf_size, "%02d:%02d", minutes, secs);
    }
}

static const char* AudioFileBasename(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return "";
    }
    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

enum class AudioWindowLayout { Thin, Stacked, Wide };

// 20 px of hysteresis so a scrollbar appearing at the breakpoint cannot flip the layout.
static AudioWindowLayout AudioWindowLayoutFor(float width) {
    static AudioWindowLayout mode = AudioWindowLayout::Stacked;
    switch (mode) {
    case AudioWindowLayout::Wide:
        if (width < 1060.0f) {
            mode = (width < 460.0f) ? AudioWindowLayout::Thin : AudioWindowLayout::Stacked;
        }
        break;
    case AudioWindowLayout::Thin:
        if (width >= 500.0f) {
            mode = (width >= 1120.0f) ? AudioWindowLayout::Wide : AudioWindowLayout::Stacked;
        }
        break;
    case AudioWindowLayout::Stacked:
        if (width < 480.0f) {
            mode = AudioWindowLayout::Thin;
        } else if (width >= 1100.0f) {
            mode = AudioWindowLayout::Wide;
        }
        break;
    }
    return mode;
}

static void BeginAudioCard(const char* id, const ImVec2& size, bool auto_height) {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.34f, 0.35f, 0.38f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.13f, 1.0f));
    ImGuiChildFlags child_flags = ImGuiChildFlags_Border;
    if (auto_height) {
        child_flags |= ImGuiChildFlags_AutoResizeY;
    }
    const ImGuiWindowFlags window_flags = auto_height ? ImGuiWindowFlags_NoScrollbar : ImGuiWindowFlags_None;
    ImGui::BeginChild(id, size, child_flags, window_flags);
}

static float EndAudioCard() {
    ImGui::EndChild();
    const float height = ImGui::GetItemRectSize().y;
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    return height;
}

// Header, 50 px graph, and the frequency captions sit around the faders.
// outer_h is the card's outer height. The extra pixels keep the auto card from growing a scrollbar.
static float EqualizerSliderHeightForOuter(float outer_h, bool thin_labels) {
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float label_h = (thin_labels && g_audioSmallFont != nullptr) ? g_audioSmallFont->FontSize : ImGui::GetTextLineHeight();
    const float chrome = 9.0f * 2.0f + ImGui::GetFrameHeight() + spacing * 3.0f + 50.0f + label_h + 8.0f;
    return ImMax(150.0f, outer_h - chrome);
}

static float EqualizerSliderHeightFromInner(float inner_h, bool thin_labels) {
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float label_h = (thin_labels && g_audioSmallFont != nullptr) ? g_audioSmallFont->FontSize : ImGui::GetTextLineHeight();
    const float chrome = ImGui::GetFrameHeight() + spacing * 3.0f + 50.0f + label_h + 6.0f;
    return ImMax(150.0f, inner_h - chrome);
}

static float SourceSegmentWidth() {
    const float label = ImMax(ImGui::CalcTextSize("Microphone").x, ImGui::CalcTextSize("Audio File").x);
    return ImFloor(label + 28.0f);
}

static bool DrawSourceSegment(const char* id, const char* label, bool selected, bool left, float width) {
    const float height = ImGui::GetFrameHeight();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImVec4 fill = selected ? ImVec4(0.16f, 0.48f, 0.24f, 1.0f) : ImVec4(0.14f, 0.15f, 0.17f, 1.0f);
    if (held) {
        fill = selected ? ImVec4(0.12f, 0.36f, 0.18f, 1.0f) : ImVec4(0.18f, 0.19f, 0.22f, 1.0f);
    } else if (hovered) {
        fill = selected ? ImVec4(0.22f, 0.58f, 0.32f, 1.0f) : ImVec4(0.22f, 0.24f, 0.27f, 1.0f);
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float rounding = 5.0f;
    const ImDrawFlags corners = left ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
    draw->AddRectFilled(a, b, ImGui::GetColorU32(fill), rounding, corners);
    draw->AddRect(a, b, ImGui::GetColorU32(ImVec4(0.32f, 0.38f, 0.34f, 1.0f)), rounding, corners, 1.0f);

    const ImVec2 label_size = ImGui::CalcTextSize(label);
    const float center_x = (a.x + b.x) * 0.5f;
    const float center_y = (a.y + b.y) * 0.5f;
    const float text_x = center_x - label_size.x * 0.5f;
    const ImU32 text_col = selected ? IM_COL32(236, 248, 236, 255) : IM_COL32(176, 182, 188, 255);
    draw->AddText(ImVec2(text_x, center_y - label_size.y * 0.5f), text_col, label);
    if (selected) {
        draw->AddCircleFilled(ImVec2(text_x - 8.0f, center_y), 3.0f, IM_COL32(214, 245, 206, 255));
    }
    return pressed;
}

static void DrawSourceToggle(bool show_label) {
    if (show_label) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Source");
        ImGui::SameLine(0.0f, 8.0f);
    }
    const float half = SourceSegmentWidth();
    const auto current = g_audioSystem.GetCurrentAudioSource();
    if (DrawSourceSegment("##SrcMic", "Microphone", current == AudioSystem::AudioSource::Microphone, true, half)) {
        if (current != AudioSystem::AudioSource::Microphone) {
            g_audioSystem.SetCurrentAudioSource(AudioSystem::AudioSource::Microphone);
        }
    }
    ImGui::SameLine(0.0f, 0.0f);
    if (DrawSourceSegment("##SrcFile", "Audio File", current == AudioSystem::AudioSource::AudioFile, false, half)) {
        if (current != AudioSystem::AudioSource::AudioFile) {
            g_audioSystem.SetCurrentAudioSource(AudioSystem::AudioSource::AudioFile);
        }
    }
}

static float SourceToggleWidth(bool show_label) {
    float width = SourceSegmentWidth() * 2.0f;
    if (show_label) {
        width += ImGui::CalcTextSize("Source").x + 8.0f;
    }
    return width;
}

static void RenderAudioLinkRow(float content_w, bool thin) {
    const float content_x = ImGui::GetCursorPosX();
    const bool show_label = !thin;
    const float group_w = SourceToggleWidth(show_label);

    ImGui::Checkbox("Audio Link", &g_enableAudioLink);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Feeds audio into shaders (iAudioBands / iAudioAmp)");
    }

    const float checkbox_w = ImGui::GetItemRectSize().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const bool fits = checkbox_w + gap + group_w <= content_w;
    if (fits) {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(content_x + content_w - group_w);
    } else {
        ImGui::SetCursorPosX(content_x + ImMax(0.0f, content_w - group_w));
    }
    DrawSourceToggle(show_label);
}

// Drag anywhere on the track. While the button is active the knob follows the mouse
// so it does not wait a frame for GetPlaybackProgress().
static void DrawSeekTimeline(float width, float progress, float duration, bool with_times) {
    char elapsed[16];
    char total[16];
    const float safe_duration = duration > 0.0f ? duration : 0.0f;
    FormatPlaybackClock(elapsed, sizeof(elapsed), ImSaturate(progress) * safe_duration);
    FormatPlaybackClock(total, sizeof(total), safe_duration);

    const float row_y = ImGui::GetCursorPosY();
    const float text_h = ImGui::GetTextLineHeight();
    const float widget_h = 26.0f;
    const float gap = 8.0f;
    const ImVec4 time_color(0.78f, 0.86f, 0.76f, 1.0f);
    float track_w = ImMax(1.0f, width);
    if (with_times) {
        const float elapsed_w = ImMax(ImGui::CalcTextSize(elapsed).x, ImGui::CalcTextSize("00:00").x);
        const float total_w = ImMax(ImGui::CalcTextSize(total).x, ImGui::CalcTextSize("00:00").x);
        track_w = ImMax(1.0f, width - elapsed_w - total_w - gap * 2.0f);

        ImGui::SetCursorPosY(row_y + (widget_h - text_h) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, time_color);
        const float elapsed_x = ImGui::GetCursorPosX();
        ImGui::TextUnformatted(elapsed);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosX(elapsed_x + elapsed_w + gap);
        ImGui::SetCursorPosY(row_y);
    }

    ImGui::InvisibleButton("##Seek", ImVec2(track_w, widget_h));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    const float span = ImMax(1.0f, b.x - a.x);
    const float mouse_t = ImSaturate((ImGui::GetIO().MousePos.x - a.x) / span);
    float t = ImSaturate(progress);
    if (active && safe_duration > 0.0f) {
        t = mouse_t;
        g_audioSystem.SetPlaybackProgress(t);
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float cy = (a.y + b.y) * 0.5f;
    const float track_h = 6.0f;
    const ImVec2 track_min(a.x, cy - track_h * 0.5f);
    const ImVec2 track_max(b.x, cy + track_h * 0.5f);
    draw->AddRectFilled(track_min, track_max, IM_COL32(24, 28, 26, 255), 3.0f);
    const float head_x = ImLerp(a.x, b.x, t);
    if (head_x > a.x + 1.0f) {
        const ImDrawFlags fill_corners = (t > 0.995f) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersLeft;
        draw->AddRectFilled(track_min, ImVec2(head_x, track_max.y), IM_COL32(72, 186, 86, 255), 3.0f, fill_corners);
    }
    const float knob_r = (hovered || active) ? 7.0f : 5.5f;
    const ImVec2 knob(head_x, cy);
    draw->AddCircleFilled(knob, knob_r, IM_COL32(230, 250, 230, 255));
    draw->AddCircle(knob, knob_r, IM_COL32(64, 190, 78, 255), 0, 2.0f);

    if ((hovered || active) && safe_duration > 0.0f) {
        char tip[16];
        FormatPlaybackClock(tip, sizeof(tip), mouse_t * safe_duration);
        ImGui::SetTooltip("%s", tip);
    }

    if (with_times) {
        ImGui::SameLine(0.0f, gap);
        ImGui::SetCursorPosY(row_y + (widget_h - text_h) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, time_color);
        ImGui::TextUnformatted(total);
        ImGui::PopStyleColor();
    }
}

static void RenderTransportRow(bool thin) {
    const float progress = g_audioSystem.GetPlaybackProgress();
    const float duration = g_audioSystem.GetPlaybackDuration();
    const auto transport = g_audioSystem.GetTransportState();
    const float row_y = ImGui::GetCursorPosY();
    const ImVec4 play_color(0.20f, 0.70f, 0.20f, 1.0f);
    const ImVec4 pause_color(0.90f, 0.48f, 0.08f, 1.0f);
    const ImVec4 stop_color(0.80f, 0.20f, 0.20f, 1.0f);

    if (TransportIconButton("##Play", transport == AudioSystem::TransportState::Playing, play_color, TransportIcon::Play)) {
        g_audioSystem.Play();
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (TransportIconButton("##Pause", transport == AudioSystem::TransportState::Paused, pause_color, TransportIcon::Pause)) {
        g_audioSystem.Pause();
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (TransportIconButton("##Stop", transport == AudioSystem::TransportState::Stopped, stop_color, TransportIcon::Stop)) {
        g_audioSystem.Stop();
    }

    char elapsed[16];
    char total[16];
    const float safe_duration = duration > 0.0f ? duration : 0.0f;
    FormatPlaybackClock(elapsed, sizeof(elapsed), ImSaturate(progress) * safe_duration);
    FormatPlaybackClock(total, sizeof(total), safe_duration);

    const float stop_right = ImGui::GetItemRectMax().x;
    const float content_right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float rest = content_right - stop_right - 12.0f;
    const float elapsed_w = ImMax(ImGui::CalcTextSize(elapsed).x, ImGui::CalcTextSize("00:00").x);
    const float total_w = ImMax(ImGui::CalcTextSize(total).x, ImGui::CalcTextSize("00:00").x);
    const bool inline_timeline = !thin && rest >= elapsed_w + total_w + 16.0f + 48.0f;

    if (inline_timeline) {
        ImGui::SameLine(0.0f, 12.0f);
        DrawSeekTimeline(ImGui::GetContentRegionAvail().x, progress, duration, true);
        return;
    }

    char clock[40];
    snprintf(clock, sizeof(clock), "%s / %s", elapsed, total);
    const float clock_w = ImGui::CalcTextSize(clock).x;
    const float room = content_right - stop_right - 8.0f;
    if (room >= clock_w) {
        ImGui::SameLine(0.0f, 8.0f);
        const float shift = room - clock_w;
        if (shift > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + shift);
        }
        const float text_h = ImGui::GetTextLineHeight();
        ImGui::SetCursorPosY(row_y + (26.0f - text_h) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.86f, 0.76f, 1.0f));
        ImGui::TextUnformatted(clock);
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.86f, 0.76f, 1.0f));
        ImGui::TextUnformatted(clock);
        ImGui::PopStyleColor();
    }
    DrawSeekTimeline(ImGui::GetContentRegionAvail().x, progress, duration, false);
}

static void RenderNowPlayingBody(bool thin) {
    DrawCardTitle("NOW PLAYING");

    if (g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::Microphone) {
        ImGui::TextDisabled("Input Device");
        const auto& devices = g_audioSystem.GetCaptureDeviceGUINames();
        if (devices.empty()) {
            ImGui::TextUnformatted("No capture devices found.");
            return;
        }
        const int selectedDeviceIndex = g_audioSystem.GetSelectedCaptureDeviceIndex();
        const char* preview = (selectedDeviceIndex >= 0 && (size_t)selectedDeviceIndex < devices.size()) ? devices[selectedDeviceIndex] : "None";
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##InputDevice", preview)) {
            for (size_t i = 0; i < devices.size(); ++i) {
                const bool is_selected = (selectedDeviceIndex == (int)i);
                if (ImGui::Selectable(devices[i], is_selected)) {
                    if (selectedDeviceIndex != (int)i) {
                        g_audioSystem.SetSelectedCaptureDeviceIndex(static_cast<int>(i));
                        g_audioSystem.InitializeAndStartSelectedCaptureDevice();
                    }
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return;
    }

    const char* path = g_audioSystem.GetAudioFilePathBuffer();
    const char* base = AudioFileBasename(path);
    const bool has_name = base[0] != '\0';
    const char* title = has_name ? base : "No file";
    const bool loaded = g_audioSystem.IsAudioFileLoaded();
    const std::string& error = g_audioSystem.GetLastError();
    const bool failed = !loaded && !error.empty();
    const char* status = loaded ? "Loaded" : (failed ? "Error" : "Not Loaded");

    const float text_h = ImGui::GetTextLineHeight();
    const ImVec2 status_size = ImGui::CalcTextSize(status);
    const float pill_h = text_h + 8.0f;
    const float pill_w = status_size.x + 24.0f;
    const float title_h = (g_audioTitleFont != nullptr) ? g_audioTitleFont->FontSize : text_h;
    const float line_h = ImMax(title_h, pill_h);
    const float row_w = ImMax(1.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 row = ImGui::GetCursorScreenPos();
    const float name_w = ImMax(0.0f, row_w - pill_w - 8.0f);

    if (g_audioTitleFont != nullptr) {
        ImGui::PushFont(g_audioTitleFont);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, has_name ? ImVec4(0.96f, 0.97f, 0.96f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const float title_y = row.y + (line_h - title_h) * 0.5f;
    ImGui::RenderTextEllipsis(
        ImGui::GetWindowDrawList(),
        ImVec2(row.x, title_y),
        ImVec2(row.x + name_w, title_y + title_h),
        row.x + name_w,
        row.x + name_w,
        title,
        nullptr,
        nullptr);
    ImGui::PopStyleColor();
    if (g_audioTitleFont != nullptr) {
        ImGui::PopFont();
    }

    ImVec4 pill_fill(0.16f, 0.16f, 0.18f, 1.0f);
    ImVec4 pill_border(0.42f, 0.42f, 0.46f, 1.0f);
    ImVec4 pill_text(0.70f, 0.70f, 0.74f, 1.0f);
    ImVec4 pill_dot(0.55f, 0.55f, 0.58f, 1.0f);
    if (loaded) {
        pill_fill = ImVec4(0.08f, 0.24f, 0.12f, 1.0f);
        pill_border = ImVec4(0.35f, 0.75f, 0.40f, 1.0f);
        pill_text = ImVec4(0.78f, 0.95f, 0.78f, 1.0f);
        pill_dot = ImVec4(0.45f, 0.90f, 0.50f, 1.0f);
    } else if (failed) {
        pill_fill = ImVec4(0.30f, 0.09f, 0.09f, 1.0f);
        pill_border = ImVec4(0.85f, 0.28f, 0.24f, 1.0f);
        pill_text = ImVec4(0.98f, 0.76f, 0.72f, 1.0f);
        pill_dot = ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
    }
    const float pill_y = row.y + (line_h - pill_h) * 0.5f;
    const ImVec2 pill_min(row.x + row_w - pill_w, pill_y);
    const ImVec2 pill_max(pill_min.x + pill_w, pill_y + pill_h);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float pill_rounding = pill_h * 0.5f;
    draw->AddRectFilled(pill_min, pill_max, ImGui::GetColorU32(pill_fill), pill_rounding);
    draw->AddRect(pill_min, pill_max, ImGui::GetColorU32(pill_border), pill_rounding, 0, 1.0f);
    const float pill_cy = (pill_min.y + pill_max.y) * 0.5f;
    draw->AddCircleFilled(ImVec2(pill_min.x + 9.0f, pill_cy), 3.0f, ImGui::GetColorU32(pill_dot));
    draw->AddText(ImVec2(pill_min.x + 16.0f, pill_cy - status_size.y * 0.5f), ImGui::GetColorU32(pill_text), status);
    if (failed && ImGui::IsMouseHoveringRect(pill_min, pill_max)) {
        ImGui::SetTooltip("%s", error.c_str());
    }
    ImGui::Dummy(ImVec2(row_w, line_h));

    const ImGuiStyle& style = ImGui::GetStyle();
    char* audioFilePath = g_audioSystem.GetAudioFilePathBuffer();
    const float browse_w = ImFloor(ImGui::CalcTextSize("Browse").x + style.FramePadding.x * 2.0f);
    const float load_w = ImFloor(ImGui::CalcTextSize("Load").x + style.FramePadding.x * 2.0f);
    const float path_row_w = ImGui::GetContentRegionAvail().x;
    const float path_w = path_row_w - browse_w - load_w - style.ItemSpacing.x * 2.0f;
    const bool one_row = path_w >= 24.0f;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.66f, 0.68f, 0.70f, 1.0f));
    ImGui::SetNextItemWidth(one_row ? path_w : -1.0f);
    ImGui::InputText("##AudioFilePath", audioFilePath, AUDIO_FILE_PATH_BUFFER_SIZE);
    ImGui::PopStyleColor();
    if (one_row) {
        ImGui::SameLine();
    }
    if (ImGui::Button("Browse##AudioFile", ImVec2(browse_w, 0.0f))) {
        ImGuiFileDialog::Instance()->OpenDialog("ChooseAudioFileDlgKey", "Choose Audio File", ".mp3,.wav", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button("Load##AudioFile", ImVec2(load_w, 0.0f))) {
        g_audioSystem.LoadWavFile(audioFilePath);
    }
    ImGui::PopStyleColor(4);

    if (loaded) {
        RenderTransportRow(thin);
    }
}

static ImVec4 LevelSegmentColor(float t) {
    t = ImSaturate(t);
    if (t < 0.62f) {
        return kEqColorGreen;
    }
    if (t < 0.82f) {
        return ImLerp(kEqColorGreen, kEqColorYellow, (t - 0.62f) / 0.20f);
    }
    return ImLerp(kEqColorYellow, kEqColorRed, (t - 0.82f) / 0.18f);
}

static void RenderLevelMeter(float width) {
    static float peak = 0.0f;
    float amp = g_audioSystem.GetCurrentAmplitude();
    if (!(amp >= 0.0f)) {
        amp = 0.0f;
    }
    const float shown = ImSaturate(amp);
    float dt = ImGui::GetIO().DeltaTime;
    if (dt < 0.0f) {
        dt = 0.0f;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }
    if (shown >= peak) {
        peak = shown;
    } else {
        peak = ImMax(0.0f, peak - dt * 0.18f);
    }

    const float db = (amp <= 1.0e-6f) ? -60.0f : ImMax(-60.0f, 20.0f * std::log10(amp));
    char readout[48];
    snprintf(readout, sizeof(readout), "%.0f%%  %+.1f dB", amp * 100.0f, db);

    const float label_w = ImGui::CalcTextSize("Level").x;
    const float read_w = ImMax(ImGui::CalcTextSize(readout).x, ImGui::CalcTextSize("000%  -00.0 dB").x);
    const float gap = 8.0f;
    const float row_h = ImGui::GetFrameHeight();
    float meter_w = width - label_w - read_w - gap * 2.0f;
    if (meter_w < 8.0f) {
        meter_w = ImMax(0.0f, width - label_w - gap);
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float text_h = ImGui::GetTextLineHeight();
    const float text_y = origin.y + (row_h - text_h) * 0.5f;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddText(ImVec2(origin.x, text_y), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Level");

    if (meter_w >= 8.0f) {
        const int segments = 30;
        float gap_px = 1.5f;
        float seg_w = (meter_w - gap_px * static_cast<float>(segments - 1)) / static_cast<float>(segments);
        if (seg_w < 1.5f) {
            gap_px = 1.0f;
            seg_w = (meter_w - gap_px * static_cast<float>(segments - 1)) / static_cast<float>(segments);
        }
        if (seg_w < 1.0f) {
            gap_px = 0.0f;
            seg_w = meter_w / static_cast<float>(segments);
        }
        const float seg_h = 9.0f;
        const float meter_x = origin.x + label_w + gap;
        const float meter_y = origin.y + (row_h - seg_h) * 0.5f;
        for (int i = 0; i < segments; ++i) {
            const float center_t = (static_cast<float>(i) + 0.5f) / static_cast<float>(segments);
            ImVec4 color = LevelSegmentColor(center_t);
            if (center_t > shown) {
                color.x *= 0.22f;
                color.y *= 0.22f;
                color.z *= 0.22f;
            }
            const float x = meter_x + static_cast<float>(i) * (seg_w + gap_px);
            draw->AddRectFilled(ImVec2(x, meter_y), ImVec2(x + seg_w, meter_y + seg_h), ImGui::GetColorU32(color), 1.0f);
        }
        if (peak >= 0.005f) {
            const float tick_x = meter_x + ImSaturate(peak) * meter_w;
            draw->AddLine(ImVec2(tick_x, meter_y - 2.0f), ImVec2(tick_x, meter_y + seg_h + 2.0f), IM_COL32(236, 236, 220, 230), 2.0f);
        }
    }

    const float read_x = origin.x + width - ImGui::CalcTextSize(readout).x;
    draw->AddText(ImVec2(read_x, text_y), IM_COL32(214, 224, 208, 255), readout);
    ImGui::Dummy(ImVec2(ImMax(1.0f, width), row_h));
}

// plot_h < 0 fills whatever vertical space the card has left under the title.
static void RenderSpectrumBody(float plot_h) {
    DrawCardTitle("SPECTRUM");
    const float width = ImMax(1.0f, ImGui::GetContentRegionAvail().x);
    float height = plot_h;
    if (height < 0.0f) {
        const float meter_h = ImGui::GetFrameHeight();
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        height = ImMax(80.0f, ImGui::GetContentRegionAvail().y - meter_h - spacing - 1.0f);
    }
    RenderSpectrumMonitor(width, height);
    RenderLevelMeter(width);
}

static void RenderAudioStacked(bool thin) {
    BeginAudioCard("##now", ImVec2(0.0f, 0.0f), true);
    RenderNowPlayingBody(thin);
    EndAudioCard();

    BeginAudioCard("##spectrum", ImVec2(0.0f, 0.0f), true);
    RenderSpectrumBody(170.0f);
    EndAudioCard();

    const float budget = ImGui::GetContentRegionAvail().y;
    const float slider_h = thin ? 150.0f : EqualizerSliderHeightForOuter(budget, false);
    BeginAudioCard("##eq", ImVec2(0.0f, 0.0f), true);
    RenderGraphicEqualizer(slider_h, thin);
    EndAudioCard();
}

static void RenderAudioWideColumns() {
    const float gap = 8.0f;
    const float content_w = ImMax(1.0f, ImGui::GetContentRegionAvail().x);
    const float body_h = ImMax(220.0f, ImFloor(ImGui::GetContentRegionAvail().y));
    const float col_w = ImMax(1.0f, ImFloor((content_w - gap) * 0.5f));
    const float right_w = ImMax(1.0f, content_w - col_w - gap);
    const ImVec2 origin = ImGui::GetCursorPos();

    BeginAudioCard("##now", ImVec2(col_w, 0.0f), true);
    RenderNowPlayingBody(false);
    const float now_h = EndAudioCard();

    const float spec_h = ImMax(220.0f, body_h - now_h - gap);
    ImGui::SetCursorPos(ImVec2(origin.x, origin.y + now_h + gap));
    BeginAudioCard("##spectrum", ImVec2(col_w, spec_h), false);
    RenderSpectrumBody(-1.0f);
    EndAudioCard();

    ImGui::SetCursorPos(ImVec2(origin.x + col_w + gap, origin.y));
    BeginAudioCard("##eq", ImVec2(right_w, body_h), false);
    const float slider_h = EqualizerSliderHeightFromInner(ImGui::GetContentRegionAvail().y, false);
    RenderGraphicEqualizer(slider_h, false);
    EndAudioCard();

    const float used = ImMax(body_h, now_h + gap + spec_h);
    ImGui::SetCursorPos(ImVec2(origin.x, origin.y + used));
}

void RenderAudioReactivityWindow() {
    ImGui::SetNextWindowSize(ImVec2(720.0f, 820.0f), ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Audio Reactivity", &g_showAudioWindow);
    if (visible) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        const float content_w = ImGui::GetContentRegionAvail().x;
        const AudioWindowLayout layout = AudioWindowLayoutFor(content_w);
        RenderAudioLinkRow(content_w, layout == AudioWindowLayout::Thin);
        ImGui::Separator();
        if (layout == AudioWindowLayout::Wide) {
            RenderAudioWideColumns();
        } else {
            RenderAudioStacked(layout == AudioWindowLayout::Thin);
        }
        ImGui::PopStyleVar();
    }

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGuiFileDialog::Instance()->Display("ChooseAudioFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_audioSystem.SetAudioFilePath(filePathName.c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::End();
}

// Main Application
int main(int argc, char** argv) {
    // Parse CLI flags early
    ParseCommandLineArgs(argc, argv);

    // Always log the parsed flags for verification
    std::cout << "Command line parsed: verbose=" << (g_verboseLogging ? "ON" : "OFF")
              << ", load='" << g_initialShaderPath << "'" << std::endl;
    // Set GLFW error callback BEFORE glfwInit()
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Add GLFW window hints for OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    ShaderEffect::InitializeDummyTexture(); // Initialize the dummy texture for all shader effects
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetDropCallback(window, drop_callback);

    g_renderer.Init();
    g_transitions.Init(g_renderer, SCR_WIDTH, SCR_HEIGHT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr; // Disable imgui.ini
    io.IniFilename = nullptr; // Disable imgui.ini
    io.Fonts->AddFontDefault();
    ImFontConfig audio_title_cfg;
    audio_title_cfg.SizePixels = 17.0f;
    g_audioTitleFont = io.Fonts->AddFontDefault(&audio_title_cfg);
    ImFontConfig audio_small_cfg;
    audio_small_cfg.SizePixels = 11.0f;
    g_audioSmallFont = io.Fonts->AddFontDefault(&audio_small_cfg);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Commenting these out again as they cause "not declared in scope" errors,
    // suggesting a deeper issue with ImGui version/include paths.
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    ImGui::StyleColorsDark();
    // When viewports are enabled we tweak WindowRounding/WindowBg to make them look like main window.
    // ImGuiStyle& style = ImGui::GetStyle(); // Unused variable
    // Commenting out viewport-specific style changes as ViewportsEnable flag is causing issues
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    // {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    g_themes.applyTheme("Bess Dark"); // Apply default theme
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    g_audioSystem.Initialize();
    g_audioSystem.RegisterListener(&g_videoRecorder); // Connect audio system to video recorder

    // Load initial shader (default or CLI-provided)
    auto defaultEffect = std::make_unique<ShaderEffect>(g_initialShaderPath, SCR_WIDTH, SCR_HEIGHT);
    if (g_initialShaderPath != "shaders/raymarch_v2.frag") {
        defaultEffect->name = std::filesystem::path(g_initialShaderPath).filename().string();
        if (defaultEffect->name.empty()) defaultEffect->name = "Custom Shader";
        VLog(std::string("Loading shader from: ") + g_initialShaderPath);
    } else {
        defaultEffect->name = "Raymarch Plasma v2";
    }
    defaultEffect->startTime = 0.0f;
    defaultEffect->endTime = g_timelineState.totalDuration_seconds; // Default duration
    g_scene.push_back(std::move(defaultEffect));

    // Load all effects in the scene (just the default for now)
    for (const auto& effect_ptr : g_scene) {
        effect_ptr->Load();
    }

    // Select the default effect and load its code into the editor
    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get();
        if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
            ClearErrorMarkers();
            const std::string& compileLog = se->GetCompileErrorLog();
            if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                g_consoleLog += "Default shader (" + se->name + ") issue: " + compileLog + "\n";
            } else {
                g_consoleLog += "Default shader (" + se->name + ") loaded successfully.\n";
            }
        }
    }

    // No auto-linking needed for a single node setup

    float deltaTime = 0.0f, lastFrameTime = 0.0f;
    // static bool first_time_docking = true; // Unused variable
    static size_t last_scene_size = 0;

    while(!glfwWindowShouldClose(window)) {
        // --- Process Drag and Drop Queue ---
        std::vector<DroppedFile> files_to_process;
        {
            std::lock_guard<std::mutex> lock(g_dropped_files_mutex);
            if (!g_dropped_files_queue.empty()) {
                files_to_process.swap(g_dropped_files_queue);
            }
        }

        for (const auto& file_info : files_to_process) {
            g_consoleLog += "Dropped shader file: " + file_info.path + "\n";
            auto newEffect = std::make_unique<ShaderEffect>(file_info.path, SCR_WIDTH, SCR_HEIGHT);
            newEffect->name = std::filesystem::path(file_info.path).filename().string();
            
            // CreateAndPlaceNode will load the shader and set g_selectedEffect to it
            CreateAndPlaceNode(std::move(newEffect), file_info.drop_pos);

            // After adding and loading, check for compilation errors
            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                const std::string& compileLog = se->GetCompileErrorLog();
                if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                    g_consoleLog += "ERROR loading dropped shader '" + se->name + "':\n" + compileLog + "\n";
                    // Set error markers in editor for the newly selected effect
                    g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                } else {
                    g_consoleLog += "Successfully loaded dropped shader: " + se->name + "\n";
                    ClearErrorMarkers(); // Clear any previous error markers
                }
            }
        }


        // Check if a node was added to reset the console log spam guard
        if (g_scene.size() > last_scene_size) {
            g_no_output_logged = false;
        }
        last_scene_size = g_scene.size();

        // Process deferred deletions at the start of the frame
        if (!g_nodes_to_delete.empty()) {
            for (int node_id : g_nodes_to_delete) {
                // First, remove any connections TO this node from other nodes
                for (const auto& effect : g_scene) {
                    if (!effect) continue;

                    // Handle ShaderEffect inputs
                    if (auto* se = dynamic_cast<ShaderEffect*>(effect.get())) {
                        const auto& inputs = se->GetInputs();
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            if (inputs[i] && inputs[i]->id == node_id) {
                                se->SetInputEffect(i, nullptr);
                            }
                        }
                    }
                    // Handle OutputNode input
                    else if (auto* on = dynamic_cast<OutputNode*>(effect.get())) {
                        if (on->GetInputEffect() && on->GetInputEffect()->id == node_id) {
                            on->SetInputEffect(0, nullptr);
                        }
                    }
                }

                // If the deleted node was selected, deselect it.
                // Snapshot a blend source before its FBO is destroyed.
                Effect* dying = FindEffectById(node_id);
                if (dying) {
                    g_transitions.NotifyRemoved(dying);
                    if (g_lastShownEffect == dying) g_lastShownEffect = nullptr;
                }
                if (g_selectedEffect && g_selectedEffect->id == node_id) {
                    g_selectedEffect = nullptr;
                }

                // Now, find and remove the node from the scene
                auto it = std::remove_if(g_scene.begin(), g_scene.end(), [node_id](const std::unique_ptr<Effect>& effect) {
                    return effect && effect->id == node_id;
                });
                if (it != g_scene.end()) {
                    g_scene.erase(it, g_scene.end());
                }
            }
            g_nodes_to_delete.clear();
        }

        float currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // --- Offline Rendering Logic ---
        if (g_videoRecorder.is_recording() && g_offlineRendering) {
            deltaTime = 1.0f / 60.0f; // Fixed 60 FPS time step
            g_offlineTime += deltaTime;
            
            // Sync audio file playback position and extract audio for recording.
            // Samples are pulled only when Record Audio is on. The device callback is not running:
            // StopActiveDevice joins it, and add_audio_frame is the main-thread producer.
            if (g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::AudioFile && g_audioSystem.IsAudioFileLoaded()) {
                g_audioSystem.StopActiveDevice();

                float duration = g_audioSystem.GetPlaybackDuration();
                if (duration > 0.0f)
                    g_audioSystem.SetPlaybackProgress(g_offlineTime / duration);

                if (g_recordAudio) {
                    const int sampleRate = static_cast<int>(g_audioSystem.GetCurrentInputSampleRate());
                    const int channels = static_cast<int>(g_audioSystem.GetCurrentInputChannels());
                    if (sampleRate > 0 && channels > 0) {
                        g_offlineAudioRemainder += sampleRate;
                        const int framesNeeded = static_cast<int>(g_offlineAudioRemainder / 60);
                        g_offlineAudioRemainder %= 60;
                        if (framesNeeded > 0) {
                            std::vector<float> audioBuffer(static_cast<size_t>(framesNeeded) * static_cast<size_t>(channels));
                            const ma_uint64 framesRead = g_audioSystem.ReadOfflineAudio(audioBuffer.data(), framesNeeded);
                            if (framesRead > 0)
                                g_videoRecorder.add_audio_frame(audioBuffer.data(), static_cast<int>(framesRead));
                        }
                    }
                }
            }
        }

        // A 165 Hz display cannot vsync to 60 fps (165/60 is not an integer). Sampling
        // whichever refresh falls near a 1/60 boundary records pictures 2 or 3 refreshes
        // apart and the file plays those gaps as equal steps. While recording, step time
        // by exactly 1/60 and capture only that step. In-between refreshes repeat it.
        bool recordTick = false;
        const bool realtimeRecording = g_videoRecorder.is_recording() && !g_offlineRendering;
        if (!g_videoRecorder.is_recording())
            g_audioSystem.EndRecordPull();
        if (!realtimeRecording) {
            g_recordingClockLive = false;
        } else if (!g_recordingClockLive) {
            g_recordingClockLive = true;
            g_recordingTimeOrigin = g_timelineState.isEnabled
                ? static_cast<double>(g_timelineState.currentTime_seconds)
                : glfwGetTime();
            g_recordingTimeAccum = 0.0;
            g_recordingTicks = -1;
            recordTick = true;
        } else {
            constexpr double kRecordStep = 1.0 / 60.0;
            g_recordingTimeAccum += static_cast<double>(deltaTime);
            if (g_recordingTimeAccum >= kRecordStep) {
                g_recordingTimeAccum -= kRecordStep;
                if (g_recordingTimeAccum > kRecordStep)
                    g_recordingTimeAccum = kRecordStep;
                recordTick = true;
            }
        }
        if (recordTick)
            ++g_recordingTicks;
        const double recordingShownTime = g_recordingTimeOrigin
            + (g_recordingTicks < 0 ? 0.0 : static_cast<double>(g_recordingTicks) / 60.0);

        // --- Hot-reloading Check (every second) ---
        static float hot_reload_timer = 0.0f;
        hot_reload_timer += deltaTime;
        if (hot_reload_timer > 1.0f) {
            for (const auto& effect_ptr : g_scene) {
                if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
                    if (se->CheckForUpdatesAndReload()) {
                        const std::string& compileLog = se->GetCompileErrorLog();
                        if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                            g_consoleLog += "Hot-reload ERROR for " + se->GetEffectName() + ":\n" + compileLog + "\n";
                            if (se == g_selectedEffect) {
                                g_editor.SetText(se->GetShaderSource());
                                g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                            }
                        } else {
                            g_consoleLog += "Hot-reloaded shader: " + se->GetEffectName() + "\n";
                            if (se == g_selectedEffect) {
                                g_editor.SetText(se->GetShaderSource());
                                ClearErrorMarkers();
                            }
                        }
                    }
                }
            }
            hot_reload_timer = 0.0f;
        }

        // File takes: the shader hears the same 1/60 block that is written into the file.
        // Held refreshes keep that picture, so the envelope must not decay between ticks.
        const bool filePull = g_audioSystem.IsRecordPullActive();
        if (filePull && recordTick)
            pullRealtimeFileAudio();
        if (!filePull || recordTick)
            g_audioSystem.ProcessAudio(filePull ? (1.0f / 60.0f) : deltaTime);

        // Advance g_timelineState.currentTime_seconds based on its own UI controls (play/pause)
        // This happens if the timeline's UI playback controls are active AND it's not paused.
        if (g_timelineControlActive && !g_timeline_paused) {
            if (realtimeRecording) {
                // g_recordingTicks was just advanced, so 0 is the opening frame.
                if (recordTick && g_recordingTicks > 0)
                    g_timelineState.currentTime_seconds += 1.0f / 60.0f;
            } else {
                g_timelineState.currentTime_seconds += deltaTime;
            }
        }

        // Loop timeline's current time if its UI controls are active or if it's the master time source.
        // This ensures that even if it's just being scrubbed (UI active) but not master, it still loops.
        // And if it IS master, it must loop.
        if (g_timelineControlActive || g_timelineState.isEnabled) {
             // Basic looping, ensure it stays within [0, totalDuration)
             if (g_timelineState.totalDuration_seconds > 0.0f) {
                 g_timelineState.currentTime_seconds = fmod(g_timelineState.currentTime_seconds, g_timelineState.totalDuration_seconds);
                 if (g_timelineState.currentTime_seconds < 0.0f) {
                     g_timelineState.currentTime_seconds += g_timelineState.totalDuration_seconds;
                 }
             } else {
                 g_timelineState.currentTime_seconds = 0.0f; // Avoid issues if totalDuration is zero or negative
             }
        }

        // Determine the time value to be used by effects (iTime)
        // This is the core of "Timeline Control Logic" for shaders
        float currentTimeForEffects;
        if (g_videoRecorder.is_recording() && g_offlineRendering) {
             currentTimeForEffects = g_offlineTime;
        } else if (realtimeRecording) {
             currentTimeForEffects = g_timelineState.isEnabled
                 ? g_timelineState.currentTime_seconds
                 : static_cast<float>(recordingShownTime);
        } else {
             currentTimeForEffects = g_timelineState.isEnabled ? g_timelineState.currentTime_seconds : (float)glfwGetTime();
        }

        processInput(window);

        std::vector<Effect*> activeEffects;
        if (g_timelineState.isEnabled) {
            for(const auto& effect_ptr : g_scene) {
                if(effect_ptr && currentTimeForEffects >= effect_ptr->startTime && currentTimeForEffects < effect_ptr->endTime) {
                    activeEffects.push_back(effect_ptr.get());
                }
            }
        } else {
            // If timeline master control is not enabled, all effects are considered active
            for(const auto& effect_ptr : g_scene) {
                if(effect_ptr) {
                    activeEffects.push_back(effect_ptr.get());
                }
            }
        }
        std::vector<Effect*> renderQueue = GetRenderOrder(activeEffects);
        float audioAmp = g_enableAudioLink ? g_audioSystem.GetCurrentAmplitude() : 0.0f;
        const auto& audioBands = g_audioSystem.GetAudioBands();
        const auto& audioBandsAtt = g_audioSystem.GetAudioBandsAtt();

        // Spherical to Cartesian conversion for camera position
        glm::vec3 cameraPos;
        cameraPos.x = g_cameraTarget.x + g_cameraRadius * sin(g_cameraPolar) * cos(g_cameraAzimuth);
        cameraPos.y = g_cameraTarget.y + g_cameraRadius * cos(g_cameraPolar);
        cameraPos.z = g_cameraTarget.z + g_cameraRadius * sin(g_cameraPolar) * sin(g_cameraAzimuth);

        glm::mat4 viewMatrix = glm::lookAt(cameraPos, g_cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 cameraMatrix = glm::inverse(viewMatrix);

        // Spherical to Cartesian conversion for light position
        glm::vec3 lightPos;
        lightPos.x = g_lightRadius * sin(g_lightPolar) * cos(g_lightAzimuth);
        lightPos.y = g_lightRadius * cos(g_lightPolar);
        lightPos.z = g_lightRadius * sin(g_lightPolar) * sin(g_lightAzimuth);

        checkGLError("Before Effect Render Loop");
        checkGLError("Before Effect Render Loop");
        checkGLError("Before Effect Render Loop");
        // Held refreshes during a recording repeat the last picture. Running the
        // shader again would advance feedback effects between saved frames.
        if (!realtimeRecording || recordTick) {
        for (Effect* effect_ptr : renderQueue) {
            if(auto* se = dynamic_cast<ShaderEffect*>(effect_ptr)) {
                se->SetDisplayResolution(SCR_WIDTH, SCR_HEIGHT);
                se->SetMouseState(g_mouseState[0], g_mouseState[1], g_mouseState[2], g_mouseState[3]);
                se->SetDeltaTime(realtimeRecording ? (1.0f / 60.0f) : deltaTime);
                se->IncrementFrameCount();
                se->SetAudioAmplitude(audioAmp);
                se->SetAudioBands(audioBands);
                se->SetAudioBandsAtt(audioBandsAtt);
                se->SetCameraState(cameraPos, cameraMatrix);
                se->SetLightPosition(lightPos);
            }
            effect_ptr->Update(currentTimeForEffects); 
            effect_ptr->Render();
        }
        }
        checkGLError("After Effect Render Loop");
        checkGLError("After Effect Render Loop");
        checkGLError("After Effect Render Loop");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkGLError("After Unbinding FBOs (to default)");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        RenderMenuBar();
        if (g_showGui) {
            if (g_showShaderEditorWindow) RenderShaderEditorWindow();
            // if (g_showEffectPropertiesWindow) RenderEffectPropertiesWindow(); // Call removed
            if (g_showConsoleWindow) RenderConsoleWindow();
            if (g_showTimelineWindow) RenderTimelineWindow();
            if (g_showNodeEditorWindow) RenderNodeEditorWindow();
            if (g_showAudioWindow) RenderAudioReactivityWindow();
            if (g_showShadertoyWindow) RenderShadertoyWindow();
            if (g_showFpsMeter) RenderFpsMeter();
            RenderCameraHelpText();
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkGLError("Before Main Screen Clear");
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        checkGLError("After Main Screen Clear");
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        bool outputNodeDrives = false;
        Effect* finalOutputEffect = PresentedEffect(&outputNodeDrives);
        if (!finalOutputEffect && !renderQueue.empty()) {
            finalOutputEffect = renderQueue.back();
        }

        if (finalOutputEffect) {
            checkGLError("Before Final RenderFullscreenTexture");
            GLuint finalTextureID = finalOutputEffect->GetOutputTexture();
            // A blend only replaces a selection-driven picture. A Scene Output pin still wins,
            // which is the Cut behaviour. Both nodes were already rendered into their own FBOs.
            if (g_transitions.IsActive()) {
                if (!outputNodeDrives && g_transitions.Target() == g_selectedEffect) {
                    float transitionStep = deltaTime;
                    if (realtimeRecording)
                        transitionStep = recordTick ? (1.0f / 60.0f) : 0.0f;
                    const GLuint blended = g_transitions.Compose(transitionStep);
                    if (blended != 0)
                        finalTextureID = blended;
                } else {
                    g_transitions.Cancel();
                }
            }
            g_lastShownEffect = (!outputNodeDrives && g_transitions.IsActive() && g_transitions.Target() != nullptr)
                ? g_transitions.Target()
                : finalOutputEffect;

            g_renderer.RenderFullscreenTexture(finalTextureID);
            checkGLError("After Final RenderFullscreenTexture");
        } else {
            if (!g_no_output_logged) {
                std::cout << "No finalOutputEffect determined for rendering." << std::endl;
                g_no_output_logged = true;
            }
        }

        if (g_videoRecorder.is_recording() && (g_offlineRendering || recordTick)) {
            g_videoRecorder.add_video_frame_from_pbo(1.0f / 60.0f);
        }

        glDisable(GL_BLEND);
        checkGLError("After Disabling Blend, Before ImGui Render");
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_scene.clear();
    g_transitions.Shutdown();
    g_videoRecorder.stop_recording();
    g_audioSystem.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// All callbacks and other helpers here
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    static bool space_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!space_pressed) { g_showGui = !g_showGui; space_pressed = true; }
    } else { space_pressed = false; }

    static bool f1_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
        if (!f1_pressed) {
            if (g_videoRecorder.is_recording())
                g_videoRecorder.stop_recording();
            else
                requestStartRecording();
            f1_pressed = true;
        }
    } else { f1_pressed = false; }

    static bool f2_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
        if (!f2_pressed) { g_showNodeEditorWindow = !g_showNodeEditorWindow; f2_pressed = true; }
    } else { f2_pressed = false; }

    static bool f3_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
        if (!f3_pressed) { g_showAudioWindow = !g_showAudioWindow; f3_pressed = true; }
    } else { f3_pressed = false; }

    static bool f4_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS) {
        if (!f4_pressed) { g_showFpsMeter = !g_showFpsMeter; f4_pressed = true; }
    } else { f4_pressed = false; }
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
    for (const auto& effect_ptr : g_scene) {
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            se->ResizeFrameBuffer(width, height);
        }
    }
    g_transitions.Resize(width, height);
}
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (g_cameraControlsEnabled) {
            float xoffset = xpos - g_lastMouseX;
            float yoffset = g_lastMouseY - ypos; // reversed since y-coordinates go from top to bottom
            g_lastMouseX = xpos;
            g_lastMouseY = ypos;

            if (g_isDragging) { // Right-click drag to pan target
                float sensitivity = 0.005f;
                xoffset *= sensitivity;
                yoffset *= sensitivity;

                // Calculate camera position to get view-aligned axes
                glm::vec3 cameraPos;
                cameraPos.x = g_cameraTarget.x + g_cameraRadius * sin(g_cameraPolar) * cos(g_cameraAzimuth);
                cameraPos.y = g_cameraTarget.y + g_cameraRadius * cos(g_cameraPolar);
                cameraPos.z = g_cameraTarget.z + g_cameraRadius * sin(g_cameraPolar) * sin(g_cameraAzimuth);

                glm::vec3 viewDir = glm::normalize(g_cameraTarget - cameraPos);
                glm::vec3 right = glm::normalize(glm::cross(viewDir, glm::vec3(0.0f, 1.0f, 0.0f)));
                glm::vec3 up = glm::normalize(glm::cross(right, viewDir));

                g_cameraTarget -= right * xoffset;
                g_cameraTarget += up * yoffset;

            } else if (g_isTilting) { // Left-click drag to orbit
                float sensitivity = 0.005f;
                xoffset *= sensitivity;
                yoffset *= sensitivity;

                g_cameraAzimuth -= xoffset;
                g_cameraPolar += yoffset;

                // Clamp polar angle to avoid flipping
                g_cameraPolar = glm::clamp(g_cameraPolar, 0.1f, glm::pi<float>() - 0.1f);
            } else if (g_isRotatingLight) { // Ctrl + Left-click drag to orbit light
                float sensitivity = 0.005f;
                xoffset *= sensitivity;
                yoffset *= sensitivity;

                g_lightAzimuth -= xoffset;
                g_lightPolar += yoffset;

                // Clamp polar angle to avoid flipping
                g_lightPolar = glm::clamp(g_lightPolar, 0.1f, glm::pi<float>() - 0.1f);
            }
        } else {
            int height;
            glfwGetWindowSize(window, NULL, &height);
            g_mouseState[0] = (float)xpos;
            g_mouseState[1] = (float)height - (float)ypos;
        }
    }
}
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)window; 
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (g_cameraControlsEnabled) {
            if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                if (action == GLFW_PRESS) {
                    g_isDragging = true;
                    glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
                } else if (action == GLFW_RELEASE) {
                    g_isDragging = false;
                }
            } else if (button == GLFW_MOUSE_BUTTON_LEFT) {
                if (action == GLFW_PRESS) {
                    if (mods & GLFW_MOD_CONTROL) {
                        g_isRotatingLight = true;
                    } else {
                        g_isTilting = true;
                    }
                    glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
                } else if (action == GLFW_RELEASE) {
                    g_isTilting = false;
                    g_isRotatingLight = false;
                }
            }
        } else {
            if(button == GLFW_MOUSE_BUTTON_LEFT) {
                if(action == GLFW_PRESS) {
                    g_mouseState[2] = g_mouseState[0];
                    g_mouseState[3] = g_mouseState[1];
                } else if (action == GLFW_RELEASE) {
                    g_mouseState[2] = -std::abs(g_mouseState[2]);
                    g_mouseState[3] = -std::abs(g_mouseState[3]);
                }
            }
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window; (void)xoffset;
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (g_cameraControlsEnabled) {
            float sensitivity = 0.5f;
            g_cameraRadius -= (float)yoffset * sensitivity;
            if (g_cameraRadius < 0.1f) {
                g_cameraRadius = 0.1f;
            }
        }
    }
}

TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) {
    TextEditor::ErrorMarkers markers;
    std::stringstream ss(log);
    std::string line;
    std::regex r(R"((\d+):(\d+)\s*:\s*(.*))");
    std::regex r2(R"(ERROR:\s*(\d+):(\d+)\s*:)");
    std::smatch m;
    auto trim_local = [](const std::string& s){ auto f=s.find_first_not_of(" \t\r\n"); return (f==std::string::npos)?"":s.substr(f, s.find_last_not_of(" \t\r\n")-f+1);};
    while(std::getline(ss, line)) {
        if(std::regex_search(line, m, r) && m.size() > 2) {
            try { markers[std::stoi(m[2].str())] = trim_local(m[3].str()); } 
            catch (const std::invalid_argument& ia) { /* g_consoleLog += "GLSL Parser: Invalid argument for stoi: " + std::string(ia.what()) + "\n"; */ }
            catch (const std::out_of_range& oor) { /* g_consoleLog += "GLSL Parser: Out of range for stoi: " + std::string(oor.what()) + "\n"; */ }
        } else if (std::regex_search(line, m, r2) && m.size() > 2) {
             try { markers[std::stoi(m[2].str())] = trim_local(line); } 
             catch (const std::invalid_argument& ia) { /* g_consoleLog += "GLSL Parser: Invalid argument for stoi: " + std::string(ia.what()) + "\n"; */ }
             catch (const std::out_of_range& oor) { /* g_consoleLog += "GLSL Parser: Out of range for stoi: " + std::string(oor.what()) + "\n"; */ }
        }
    }
    return markers;
}
void ClearErrorMarkers() {
    g_editor.SetErrorMarkers(TextEditor::ErrorMarkers());
}
std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects) {
    if (activeEffects.empty()) return {};
    std::vector<Effect*> sortedOrder;
    std::map<int, Effect*> nodeMap;
    std::map<int, std::vector<int>> adjList;
    std::map<int, int> inDegree;
    for (Effect* effect : activeEffects) {
        if (!effect) continue;
        nodeMap[effect->id] = effect;
        inDegree[effect->id] = 0;
        adjList[effect->id] = {};
    }
    for (Effect* effect : activeEffects) {
        if (!effect) continue;
        if (auto* se = dynamic_cast<ShaderEffect*>(effect)) {
            const auto& inputs = se->GetInputs();
            for (Effect* inputEffect : inputs) {
                if (inputEffect && nodeMap.count(inputEffect->id)) {
                    adjList[inputEffect->id].push_back(se->id);
                    inDegree[se->id]++;
                }
            }
        }
    }
    std::queue<Effect*> q;
    for (Effect* effect : activeEffects) {
        if (!effect) continue;
        if (inDegree[effect->id] == 0) q.push(effect);
    }
    while (!q.empty()) {
        Effect* u = q.front(); q.pop();
        sortedOrder.push_back(u);
        for (int v_id : adjList[u->id]) {
            if (nodeMap.count(v_id)) {
                inDegree[v_id]--;
                if (inDegree[v_id] == 0) q.push(nodeMap[v_id]);
            }
        }
    }
    if (sortedOrder.size() != activeEffects.size()) {
        std::cerr << "Error: Cycle detected in node graph!" << std::endl;
        g_consoleLog = "ERROR: Cycle detected in node graph! Rendering may be incorrect.";
        }
        return sortedOrder;
    }
    
    void drop_callback(GLFWwindow* window, int count, const char** paths) {
        // Rate limiting: prevent rapid drag operations
        auto now = std::chrono::steady_clock::now();
        if (now - g_last_drop_time < g_drop_cooldown) {
            std::cout << "[DROP] Ignoring rapid consecutive drop - rate limited" << std::endl;
            return; // Ignore drops that are too close together
        }
        g_last_drop_time = now;

        std::lock_guard<std::mutex> lock(g_dropped_files_mutex);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        ImVec2 drop_pos = ImVec2((float)xpos, (float)ypos);

        for (int i = 0; i < count; i++) {
            std::filesystem::path p = paths[i];
            if (p.extension() == ".frag") {
                try {
                    // Validate file exists
                    if (!std::filesystem::exists(p)) {
                        std::cout << "[DROP ERROR] File does not exist: " << p.string() << std::endl;
                        g_consoleLog += "ERROR: Dropped file does not exist: " + p.string() + "\n";
                        continue;
                    }

                    // Validate file size (reasonable limit to prevent massive strings)
                    auto file_size = std::filesystem::file_size(p);
                    const size_t MAX_SHADER_SIZE = 10 * 1024 * 1024; // 10MB limit
                    if (file_size > MAX_SHADER_SIZE) {
                        std::cout << "[DROP ERROR] File too large (" << file_size << " bytes): " << p.string() << std::endl;
                        g_consoleLog += "ERROR: Shader file too large (" + std::to_string(file_size) + " bytes, max " + std::to_string(MAX_SHADER_SIZE) + "): " + p.filename().string() + "\n";
                        continue;
                    }

                    // Validate file is readable
                    std::ifstream test_file(p, std::ios::binary);
                    if (!test_file.is_open()) {
                        std::cout << "[DROP ERROR] Cannot read file: " << p.string() << std::endl;
                        g_consoleLog += "ERROR: Cannot read dropped file: " + p.filename().string() + "\n";
                        continue;
                    }
                    test_file.close();

                    std::cout << "[DROP] Validated shader file: " << p.string() << " (" << file_size << " bytes)" << std::endl;

                    g_dropped_files_queue.push_back({p.string(), drop_pos});
                }
                catch (const std::filesystem::filesystem_error& e) {
                    std::cout << "[DROP ERROR] Filesystem error for " << paths[i] << ": " << e.what() << std::endl;
                    g_consoleLog += "ERROR: Filesystem error checking dropped file '" + std::string(paths[i]) + "': " + std::string(e.what()) + "\n";
                }
                catch (const std::exception& e) {
                    std::cout << "[DROP ERROR] Unexpected error validating " << paths[i] << ": " << e.what() << std::endl;
                    g_consoleLog += "ERROR: Unexpected validation error for dropped file '" + std::string(paths[i]) + "': " + std::string(e.what()) + "\n";
                }
            }
        }
    }
    void SaveScene(const std::string& filePath) {
        nlohmann::json sceneJson;
    
        // Serialize TimelineState
    sceneJson["timelineState"] = g_timelineState; // Uses to_json for TimelineState
    WriteTransitionObject(sceneJson["transition"], g_transitions.Global(), false);

    // Serialize actual effects from g_scene
    sceneJson["effects"] = nlohmann::json::array();
    for(const auto& effect_ptr : g_scene) { // Changed variable name for clarity
        if(effect_ptr) {
            sceneJson["effects"].push_back(effect_ptr->Serialize());
        }
    }
    std::ofstream o(filePath);
    if (!o.is_open()) {
        g_consoleLog = "Error: Could not open file for saving scene: " + filePath;
        return;
    }
    o << std::setw(4) << sceneJson << std::endl;
    if (!o.good()) {
        g_consoleLog = "Error: Failed to write scene to file: " + filePath;
    } else {
        g_consoleLog = "Scene saved to: " + filePath; // Success message
    }
}
void LoadScene(const std::string& filePath) {
    std::ifstream i(filePath);
    if (!i.is_open()) {
        g_consoleLog = "Error: Could not open scene file: " + filePath;
        return;
    }
    nlohmann::json sceneJson;
    try {
        i >> sceneJson;
    } catch(const nlohmann::json::parse_error& e) {
        g_consoleLog = "Error parsing scene file: " + std::string(e.what());
        return;
    }

    g_transitions.Cancel();
    g_lastShownEffect = nullptr;
    g_scene.clear();
    g_selectedEffect = nullptr;
    g_editor.SetText("");
    ClearErrorMarkers();
    g_transitions.Global() = TransitionSettings{};
    if (sceneJson.contains("transition"))
        ReadTransitionObject(sceneJson["transition"], g_transitions.Global(), false);

    if (sceneJson.contains("timelineState")) {
        g_timelineState = sceneJson["timelineState"].get<TimelineState>();
    }

    std::map<int, Effect*> oldIdToNewEffectMap;

    // First pass: create all effects
    if (sceneJson.contains("effects") && sceneJson["effects"].is_array()) {
        for (const auto& effectJson : sceneJson["effects"]) {
            std::string type = effectJson.value("type", "Unknown");
            std::unique_ptr<Effect> newEffect = nullptr;

            if (type == "ShaderEffect") {
                newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT);
            } else if (type == "OutputNode") {
                newEffect = std::make_unique<OutputNode>();
            }

            if (newEffect) {
                int oldId = effectJson.value("id", -1);
                newEffect->Deserialize(effectJson);
                newEffect->Load();
                oldIdToNewEffectMap[oldId] = newEffect.get();
                g_scene.push_back(std::move(newEffect));
            }
        }
    }

    // Second pass: link effects
    for (auto& effect_ptr : g_scene) {
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            const auto& input_ids = se->GetDeserializedInputIds();
            for (size_t i = 0; i < input_ids.size(); ++i) {
                int old_id = input_ids[i];
                if (oldIdToNewEffectMap.count(old_id)) {
                    se->SetInputEffect(i, oldIdToNewEffectMap[old_id]);
                }
            }
        } else if (auto* on = dynamic_cast<OutputNode*>(effect_ptr.get())) {
            int old_id = on->GetDeserializedInputId();
            if (oldIdToNewEffectMap.count(old_id)) {
                on->SetInputEffect(0, oldIdToNewEffectMap[old_id]);
            }
        }
    }

    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get();
        if(auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
        }
    }

    int max_id = 0;
    for (const auto& effect_ptr : g_scene) {
        if (effect_ptr && effect_ptr->id > max_id) {
            max_id = effect_ptr->id;
        }
    }
    Effect::UpdateNextId(max_id + 1);
    g_consoleLog = "Scene loaded from: " + filePath;
}
