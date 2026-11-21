#include "bytecode.hpp"
#include "optimizer.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace batexe::compiler {

namespace {

void write_u32(ByteVector& buf, uint32_t val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + 4);
}

void write_u64(ByteVector& buf, uint64_t val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + 8);
}

void write_i32(ByteVector& buf, int32_t val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + 4);
}

void write_str(ByteVector& buf, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    write_u32(buf, len);
    if (len > 0) {
        buf.insert(buf.end(), s.data(), s.data() + len);
    }
}

uint32_t read_u32(const Byte* ptr, size_t size, size_t& offset) {
    if (offset + 4 > size) return 0;
    uint32_t val = *reinterpret_cast<const uint32_t*>(ptr + offset);
    offset += 4;
    return val;
}

uint64_t read_u64(const Byte* ptr, size_t size, size_t& offset) {
    if (offset + 8 > size) return 0;
    uint64_t val = *reinterpret_cast<const uint64_t*>(ptr + offset);
    offset += 8;
    return val;
}

int32_t read_i32(const Byte* ptr, size_t size, size_t& offset) {
    if (offset + 4 > size) return 0;
    int32_t val = *reinterpret_cast<const int32_t*>(ptr + offset);
    offset += 4;
    return val;
}

std::string read_str(const Byte* ptr, size_t size, size_t& offset) {
    if (offset + 4 > size) return "";
    uint32_t len = *reinterpret_cast<const uint32_t*>(ptr + offset);
    offset += 4;
    if (offset + len > size) return "";
    std::string s(reinterpret_cast<const char*>(ptr + offset), len);
    offset += len;
    return s;
}

}

ByteVector serialize_bytecode(const Program& prog) {
    ByteVector buf;
    buf.reserve(1024 + prog.code.size() * 32);

    write_u32(buf, MAGIC_BYTECODE);
    write_u32(buf, BYTECODE_VERSION);
    write_u32(buf, static_cast<uint32_t>(prog.code.size()));

    for (const auto& inst : prog.code) {
        buf.push_back(static_cast<uint8_t>(inst.op));
        write_i32(buf, inst.target_pc);
        write_u32(buf, inst.line_number);
        write_str(buf, inst.arg1);
        write_str(buf, inst.arg2);
        write_str(buf, inst.arg3);
    }

    write_str(buf, prog.raw_script);

    uint64_t csum = compute_checksum(buf.data(), buf.size());
    write_u64(buf, csum);

    return buf;
}

std::optional<Program> deserialize_bytecode(const Byte* ptr, size_t size) {
    if (!ptr || size < 12 + 8) return std::nullopt;

    size_t payload_bytes = size - 8;
    size_t csum_offset = payload_bytes;
    uint64_t expected_csum = read_u64(ptr, size, csum_offset);
    uint64_t actual_csum = compute_checksum(ptr, payload_bytes);
    if (expected_csum != actual_csum) {
        return std::nullopt;
    }

    size_t offset = 0;
    uint32_t magic = read_u32(ptr, size, offset);
    if (magic != MAGIC_BYTECODE) return std::nullopt;

    uint32_t ver = read_u32(ptr, size, offset);
    if (ver != BYTECODE_VERSION) return std::nullopt;

    uint32_t count = read_u32(ptr, size, offset);
    Program prog;
    prog.code.reserve(count);

    for (uint32_t i = 0; i < count && offset < payload_bytes; ++i) {
        Instruction inst;
        inst.op = static_cast<OpCode>(ptr[offset++]);
        inst.target_pc = read_i32(ptr, size, offset);
        inst.line_number = read_u32(ptr, size, offset);
        inst.arg1 = read_str(ptr, size, offset);
        inst.arg2 = read_str(ptr, size, offset);
        inst.arg3 = read_str(ptr, size, offset);
        prog.code.push_back(std::move(inst));
    }

    if (offset < payload_bytes) {
        prog.raw_script = read_str(ptr, size, offset);
    }

    return prog;
}

std::string dump_disassembly(const Program& prog) {
    std::ostringstream ss;
    ss << "; bytecode dump (" << prog.code.size() << " instructions)\n\n";

    for (size_t i = 0; i < prog.code.size(); ++i) {
        const auto& inst = prog.code[i];
        ss << "  " << std::setw(4) << std::setfill('0') << i << ":  "
           << std::setw(16) << std::setfill(' ') << std::left << opcode_to_string(inst.op);

        if (!inst.arg1.empty()) ss << " \"" << inst.arg1 << "\"";
        if (!inst.arg2.empty()) ss << ", \"" << inst.arg2 << "\"";
        if (!inst.arg3.empty()) ss << ", \"" << inst.arg3 << "\"";
        if (inst.target_pc >= 0) ss << " -> [PC: " << inst.target_pc << "]";
        ss << "\n";
    }

    return ss.str();
}

}
