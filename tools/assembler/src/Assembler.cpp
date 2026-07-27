#include "Assembler.hpp"
#include "Tokenizer.hpp"
#include "InstructionTable.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>

static AddressingMode infer_addressing_mode([[maybe_unused]] const std::string& mnemonic, const std::optional<std::string>& operand) {
    if (!operand.has_value() || operand->empty()) {
        return AddressingMode::Implied;
    }

    const std::string& op = *operand;

    if (op[0] == '#') {
        return AddressingMode::Immediate;
    }
    if (op[0] == '(' && op.back() == ')') {
        if (op.find(",X") != std::string::npos || op.find(",x") != std::string::npos) {
            return AddressingMode::IndirectX;
        }
        return AddressingMode::Indirect;
    }
    if (op[0] == '(' && op.find("),Y") != std::string::npos) {
        return AddressingMode::IndirectY;
    }
    if (op.find(",X") != std::string::npos || op.find(",x") != std::string::npos) {
        return AddressingMode::AbsoluteX;
    }
    if (op.find(",Y") != std::string::npos || op.find(",y") != std::string::npos) {
        return AddressingMode::AbsoluteY;
    }
    if (op[0] == '$') {
        return (op.substr(1).length() <= 2) ? AddressingMode::ZeroPage : AddressingMode::Absolute;
    }

    return AddressingMode::Absolute;
}

static uint16_t parse_operand_value(const std::string& op_str) {
    std::string s = op_str;
    if (s[0] == '#') s = s.substr(1);
    if (s[0] == '(') s = s.substr(1, s.length() - 2);
    
    auto comma = s.find(',');
    if (comma != std::string::npos) s = s.substr(0, comma);

    if (s[0] == '$') {
        return static_cast<uint16_t>(std::stoul(s.substr(1), nullptr, 16));
    } else {
        return static_cast<uint16_t>(std::stoul(s, nullptr, 10));
    }
}

bool Assembler::assemble_file(const std::string& filepath, std::vector<uint8_t>& out_machine_code) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open input file: " << filepath << "\n";
        return false;
    }

    const auto& inst_table = get_instruction_table();
    std::vector<std::string> raw_lines;
    std::string line;
    while (std::getline(file, line)) {
        raw_lines.push_back(line);
    }

    // Pass 1: Collect labels and calculate instruction byte sizes (Origin: 0x0400)
    std::unordered_map<std::string, uint16_t> symbol_table;
    uint16_t pc = 0x0400;

    for (const auto& raw_line : raw_lines) {
        ParsedLine parsed = Tokenizer::parse_line(raw_line);
        if (parsed.label.has_value()) {
            symbol_table[*parsed.label] = pc;
        }
        if (parsed.mnemonic.has_value()) {
            AddressingMode mode = infer_addressing_mode(*parsed.mnemonic, parsed.operand);
            const InstructionSpec* spec = nullptr;
            for (const auto& s : inst_table) {
                if (s.mnemonic == *parsed.mnemonic && s.mode == mode) {
                    spec = &s;
                    break;
                }
            }
            if (spec) {
                pc += spec->bytes;
            } else if (*parsed.mnemonic == "JMP") {
                pc += 3; // Default absolute JMP size
            } else {
                pc += 2;
            }
        }
    }

    // Pass 2: Emit bytecode
    size_t line_num = 0;
    for (const auto& raw_line : raw_lines) {
        line_num++;
        ParsedLine parsed = Tokenizer::parse_line(raw_line);

        if (!parsed.mnemonic.has_value()) {
            continue;
        }

        std::string mnemonic = *parsed.mnemonic;
        AddressingMode mode = infer_addressing_mode(mnemonic, parsed.operand);

        const InstructionSpec* matched_spec = nullptr;
        for (const auto& spec : inst_table) {
            if (spec.mnemonic == mnemonic && spec.mode == mode) {
                matched_spec = &spec;
                break;
            }
        }

        // Fallback resolution for JMP targeting a label symbol
        if (!matched_spec && mnemonic == "JMP" && parsed.operand.has_value()) {
            if (symbol_table.find(*parsed.operand) != symbol_table.end()) {
                for (const auto& spec : inst_table) {
                    if (spec.mnemonic == "JMP" && spec.mode == AddressingMode::Absolute) {
                        matched_spec = &spec;
                        break;
                    }
                }
            }
        }

        if (!matched_spec) {
            std::cerr << "Error [Line " << line_num << "]: Unknown instruction for '" << mnemonic << "'\n";
            return false;
        }

        out_machine_code.push_back(matched_spec->opcode);

        if (matched_spec->bytes > 1) {
            if (!parsed.operand.has_value()) {
                std::cerr << "Error [Line " << line_num << "]: Instruction '" << mnemonic << "' requires operand.\n";
                return false;
            }

            uint16_t val = 0;
            std::string op = *parsed.operand;
            if (symbol_table.find(op) != symbol_table.end()) {
                val = symbol_table[op];
            } else {
                val = parse_operand_value(op);
            }

            if (matched_spec->bytes == 2) {
                out_machine_code.push_back(static_cast<uint8_t>(val & 0xFF));
            } else if (matched_spec->bytes == 3) {
                out_machine_code.push_back(static_cast<uint8_t>(val & 0xFF));
                out_machine_code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            }
        }
    }

    return true;
}