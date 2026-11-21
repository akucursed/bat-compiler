#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <optional>

namespace batexe {

using Byte = uint8_t;
using ByteVector = std::vector<Byte>;

enum class OpCode : uint8_t {
    Nop             = 0x00,
    Echo            = 0x01,
    Set             = 0x02,
    SetMath         = 0x03,
    Exec            = 0x04,
    Jump            = 0x05,
    JumpIfEq        = 0x06,
    JumpIfNe        = 0x07,
    JumpIfGt        = 0x08,
    JumpIfLt        = 0x09,
    JumpIfGe        = 0x0A,
    JumpIfLe        = 0x0B,
    JumpIfExist     = 0x0C,
    JumpIfNotExist  = 0x0D,
    JumpIfDefined   = 0x0E,
    JumpIfNotDefined= 0x0F,
    Call            = 0x10,
    Return          = 0x11,
    Exit            = 0x12,
    Cd              = 0x13,
    Pause           = 0x14,
    Sleep           = 0x15,
    Cls             = 0x16,
    Title           = 0x17,
    Color           = 0x18,
    JumpIfErrorLevelGe = 0x19,
    JumpIfErrorLevelLt = 0x1A,
    SetLocal        = 0x1B,
    EndLocal        = 0x1C,
    MkDir           = 0x1D,
    Del             = 0x1E,
    Copy            = 0x1F,
    Move            = 0x20,
    RmDir           = 0x21,
    PushD           = 0x22,
    PopD            = 0x23,
    Type            = 0x24
};

const char* opcode_to_string(OpCode op);

struct Instruction {
    OpCode op{ OpCode::Nop };
    std::string arg1;
    std::string arg2;
    std::string arg3;
    int32_t target_pc{ -1 };
    uint32_t line_number{ 0 };
};

struct Program {
    std::vector<Instruction> code;
    std::unordered_map<std::string, int32_t> labels;
    std::string raw_script;
};

enum PayloadFlags : uint32_t {
    FLAG_NONE       = 0x00,
    FLAG_INVISIBLE  = 0x01,
    FLAG_ENCRYPTED  = 0x02
};

constexpr uint32_t MAGIC_BYTECODE = 0x42415456;
constexpr uint32_t MAGIC_FOOTER   = 0x554B4150;
constexpr uint32_t RESOURCE_ID_PAYLOAD = 1337;
constexpr uint32_t BYTECODE_VERSION = 4;

}
