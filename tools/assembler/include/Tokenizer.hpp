#pragma once

#include <string>
#include <optional>

struct ParsedLine {
    std::optional<std::string> label;
    std::optional<std::string> mnemonic;
    std::optional<std::string> operand;
};

class Tokenizer {
public:
    static ParsedLine parse_line(const std::string& raw_line) {
        ParsedLine result;
        std::string line = raw_line;

        // Strip comments (; or //)
        auto comment_pos = line.find(';');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return result; // Empty line
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        if (line.empty()) return result;

        // Check for label (ends with ':')
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            result.label = line.substr(0, colon_pos);
            line = line.substr(colon_pos + 1);
            
            // Trim remaining line after label
            start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return result;
            line = line.substr(start);
        }

        // Parse mnemonic and operand
        size_t space_pos = line.find_first_of(" \t");
        if (space_pos == std::string::npos) {
            result.mnemonic = line;
        } else {
            result.mnemonic = line.substr(0, space_pos);
            
            // Trim operand string
            size_t op_start = line.find_first_not_of(" \t", space_pos);
            if (op_start != std::string::npos) {
                result.operand = line.substr(op_start);
            }
        }

        return result;
    }
};