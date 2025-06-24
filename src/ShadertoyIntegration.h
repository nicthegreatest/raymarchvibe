#ifndef SHADERTOYINTEGRATION_H
#define SHADERTOYINTEGRATION_H

#include <string>
// We forward declare or keep includes minimal here.
// The actual implementation in .cpp will include httplib and nlohmann/json.

class ShadertoyIntegration {
public:
    // Fetches shader code from Shadertoy API.
    // shaderId: The 6-character Shadertoy ID.
    // apiKey: Your Shadertoy API key.
    // errorMsg: Output string for any errors encountered.
    // Returns the shader code string, or an empty string on failure.
    static std::string FetchCode(const std::string& shaderId, const std::string& apiKey, std::string& errorMsg);

    // Extracts the 6-character Shadertoy ID from a full URL or just an ID string.
    // idOrUrl: The input string which can be a URL or an ID.
    // Returns the extracted ID, or an empty string if invalid.
    static std::string ExtractId(const std::string& idOrUrl);
};

#endif // SHADERTOYINTEGRATION_H
