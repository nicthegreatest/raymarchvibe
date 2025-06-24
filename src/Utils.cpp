#include "Utils.h"

// You might need other standard includes here if you add more complex utilities.
// For Trim, <string> (via Utils.h) and potentially <cctype> or <algorithm> might be used,
// but a simple find_first_not_of/find_last_not_of is often sufficient.

std::string Utils::Trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v"; // Characters to consider as whitespace

    const size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) {
        return ""; // No content left after trimming, or string was all whitespace
    }

    const size_t strEnd = str.find_last_not_of(whitespace);
    const size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

// Define other utility functions here.
// For example:
/*
bool Utils::IsNumber(const std::string& s) {
    return !s.empty() && std::find_if(s.begin(), 
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}
*/
