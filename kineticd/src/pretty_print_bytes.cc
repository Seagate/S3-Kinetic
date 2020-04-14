#include "pretty_print_bytes.h"

#include <string>

static const int kMaxBytesToDisplay = 50;

std::string com::seagate::kinetic::PrettyPrintBytes(const std::string& bytes) {
    std::string result("<");

    const char *byte_array = bytes.c_str();
    static const char *hexLookupTable = "0123456789ABCDEF";

    int bytes_to_display = bytes.length();
    if (bytes_to_display > kMaxBytesToDisplay) {
        bytes_to_display = kMaxBytesToDisplay;
    }

    for (int i = 0; i < bytes_to_display; i++) {
        char b = byte_array[i];

        if (isprint(b)) {
            result.push_back(b);
        } else {
            result.append("\\x");
            result.push_back(hexLookupTable[(b >> 4) & 0xF]);
            result.push_back(hexLookupTable[b & 0xF]);
        }
    }

    result.append(">");
    return result;
}
