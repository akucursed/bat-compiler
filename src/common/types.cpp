#include "types.hpp"

namespace batexe {

const char* opcode_to_string(OpCode op) {
    switch (op) {
        case OpCode::Nop:             return "NOP";
        case OpCode::Echo:            return "ECHO";
        case OpCode::Set:             return "SET";
        case OpCode::SetMath:         return "SET_MATH";
        case OpCode::Exec:            return "EXEC";
        case OpCode::Jump:            return "JUMP";
        case OpCode::JumpIfEq:        return "JUMP_IF_EQ";
        case OpCode::JumpIfNe:        return "JUMP_IF_NE";
        case OpCode::JumpIfGt:        return "JUMP_IF_GT";
        case OpCode::JumpIfLt:        return "JUMP_IF_LT";
        case OpCode::JumpIfGe:        return "JUMP_IF_GE";
        case OpCode::JumpIfLe:        return "JUMP_IF_LE";
        case OpCode::JumpIfExist:     return "JUMP_IF_EXIST";
        case OpCode::JumpIfNotExist:  return "JUMP_IF_NOT_EXIST";
        case OpCode::JumpIfDefined:   return "JUMP_IF_DEFINED";
        case OpCode::JumpIfNotDefined:return "JUMP_IF_NOT_DEFINED";
        case OpCode::Call:            return "CALL";
        case OpCode::Return:          return "RETURN";
        case OpCode::Exit:            return "EXIT";
        case OpCode::Cd:              return "CD";
        case OpCode::Pause:           return "PAUSE";
        case OpCode::Sleep:           return "SLEEP";
        case OpCode::Cls:             return "CLS";
        case OpCode::Title:           return "TITLE";
        case OpCode::Color:           return "COLOR";
        case OpCode::JumpIfErrorLevelGe: return "JUMP_IF_ERR_GE";
        case OpCode::JumpIfErrorLevelLt: return "JUMP_IF_ERR_LT";
        case OpCode::SetLocal:        return "SETLOCAL";
        case OpCode::EndLocal:        return "ENDLOCAL";
        case OpCode::MkDir:           return "MKDIR";
        case OpCode::Del:             return "DEL";
        case OpCode::Copy:            return "COPY";
        case OpCode::Move:            return "MOVE";
        case OpCode::RmDir:           return "RMDIR";
        case OpCode::PushD:           return "PUSHD";
        case OpCode::PopD:            return "POPD";
        case OpCode::Type:            return "TYPE";
        default:                      return "UNKNOWN";
    }
}

}
