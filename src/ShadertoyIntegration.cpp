#include "ShadertoyIntegration.h"

#include <iostream> // For std::cerr (debugging/logging)
#include <algorithm> // For std::all_of
#include <cctype>    // For ::isalnum

// --- External libraries for Shadertoy Fetching ---
// #define CPPHTTPLIB_OPENSSL_SUPPORT // Enable for HTTPS, ensure OpenSSL is linked (commented out as already defined in build system)
#include "httplib.h" // Make sure this is accessible in your include paths
#include <nlohmann/json.hpp> // Make sure this is accessible
using json = nlohmann::json;

std::string ShadertoyIntegration::FetchCode(const std::string& shaderId, const std::string& apiKey, std::string& errorMsg) {
    errorMsg.clear();
    std::string shaderCode;

    if (shaderId.empty()) {
        errorMsg = "Error: Shadertoy ID is empty.";
        return "";
    }
    if (apiKey.empty()) {
        // You might still allow fetching if the API doesn't strictly require a key for public shaders,
        // but it's good practice to require it. For now, let's make it optional for fetching.
        // errorMsg = "Warning: Shadertoy API key is empty. Fetching may fail for private shaders.";
        // For this example, we'll proceed, but in a real app, you might block this.
    }

    try {
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            httplib::SSLClient cli("www.shadertoy.com");
            cli.set_connection_timeout(10); // 10 seconds timeout
            cli.set_read_timeout(10);
            cli.set_write_timeout(10);
        #else
            // Fallback or error if SSL is not supported, as Shadertoy API requires HTTPS
            errorMsg = "Error: HTTPS (OpenSSL) support is not enabled in httplib. Cannot connect to Shadertoy API securely.";
            std::cerr << errorMsg << std::endl;
            // Forcing HTTP for local testing if absolutely necessary, but API will likely reject or redirect.
            // httplib::Client cli("www.shadertoy.com", 80); 
            return ""; // Fail if no SSL
        #endif

        std::string path = "/api/v1/shaders/" + shaderId + "?key=" + apiKey;
        std::cerr << "ShadertoyIntegration: Fetching from https://www.shadertoy.com" << path << std::endl;

        auto res = cli.Get(path.c_str());

        if (res) {
            if (res->status == 200) {
                json j = json::parse(res->body);
                if (j.contains("Error")) { // Check for API error response first
                     errorMsg = "Shadertoy API Error: " + j["Error"].get<std::string>();
                     std::cerr << errorMsg << std::endl;
                } else if (j.contains("Shader") && j["Shader"].is_object() &&
                    j["Shader"].contains("renderpass") && j["Shader"]["renderpass"].is_array() &&
                    !j["Shader"]["renderpass"].empty() &&
                    j["Shader"]["renderpass"][0].is_object() &&
                    j["Shader"]["renderpass"][0].contains("code") &&
                    j["Shader"]["renderpass"][0]["code"].is_string()) {
                    shaderCode = j["Shader"]["renderpass"][0]["code"].get<std::string>();
                    errorMsg = "Successfully fetched code for " + shaderId; // Success message
                } else {
                    errorMsg = "Error: JSON structure from Shadertoy unexpected for ID: " + shaderId;
                    if (!res->body.empty()) {
                         errorMsg += ". Response (first 300 chars): " + res->body.substr(0, 300);
                    }
                    std::cerr << errorMsg << std::endl;
                }
            } else {
                errorMsg = "Error fetching Shadertoy " + shaderId + ": HTTP Status " + std::to_string(res->status);
                if (!res->body.empty()) {
                    errorMsg += "\nResponse: " + res->body.substr(0, 300);
                }
                std::cerr << errorMsg << std::endl;
            }
        } else {
            auto err = res.error();
            errorMsg = "HTTP request failed for Shadertoy " + shaderId + ". Error: " + httplib::to_string(err);
            std::cerr << errorMsg << std::endl;
        }
    } catch (const json::parse_error& e) {
        errorMsg = "JSON parsing error for Shadertoy response: " + std::string(e.what());
        std::cerr << errorMsg << std::endl;
    } catch (const std::exception& e) {
        errorMsg = "Exception during Shadertoy fetch: " + std::string(e.what());
        std::cerr << errorMsg << std::endl;
    } catch (...) {
        errorMsg = "Unknown exception during Shadertoy fetch.";
        std::cerr << errorMsg << std::endl;
    }

    if (shaderCode.empty() && errorMsg.find("Error") == std::string::npos && errorMsg.find("Successfully") == std::string::npos) {
        // If no specific error was set but code is empty, set a generic failure message.
        errorMsg = "Failed to fetch or parse shader for " + shaderId + ". Check ID, API key, and network.";
        std::cerr << errorMsg << std::endl;
    }
    return shaderCode;
}

std::string ShadertoyIntegration::ExtractId(const std::string& idOrUrl) {
    std::string id = idOrUrl;
    // Trim whitespace first
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = id.find_first_not_of(whitespace);
    if (std::string::npos == start) {
        return ""; // Empty or all whitespace
    }
    size_t end = id.find_last_not_of(whitespace);
    id = id.substr(start, end - start + 1);

    // Try to extract from URL
    size_t lastSlash = id.find_last_of('/');
    if (lastSlash != std::string::npos) {
        id = id.substr(lastSlash + 1);
    }
    // Remove query parameters like ?key=...
    size_t questionMark = id.find('?');
    if (questionMark != std::string::npos) {
        id = id.substr(0, questionMark);
    }
    // A valid Shadertoy ID is typically 6 alphanumeric characters
    if (id.length() == 6 && std::all_of(id.begin(), id.end(), ::isalnum)) {
        return id;
    }
    return ""; // Return empty if not a valid-looking ID
}
