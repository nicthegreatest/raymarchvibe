#ifndef UTILS_H
#define UTILS_H

#include <string> // For std::string

namespace Utils {

    // Trims leading and trailing whitespace from a string.
    // str: The string to trim.
    // Returns the trimmed string.
    std::string Trim(const std::string& str);

    // Add declarations for other general utility functions here in the future.
    // For example:
    // bool IsNumber(const std::string& s);
    // std::vector<std::string> SplitString(const std::string& s, char delimiter);

} // namespace Utils

#endif // UTILS_H
