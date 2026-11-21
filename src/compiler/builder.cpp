#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "builder.hpp"
#include "bytecode.hpp"
#include "optimizer.hpp"
#include "icon.hpp"
#include "../parser/parser.hpp"
#include "../common/crypto.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace batexe::compiler {

namespace {

ByteVector read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    std::streamsize size = file.tellg();
    if (size <= 0) return {};
    file.seekg(0, std::ios::beg);
    ByteVector buf(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buf.data()), size)) return {};
    return buf;
}

bool write_file_bytes(const std::string& path, const ByteVector& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

std::string read_file_text(const std::string& path) {
    ByteVector b = read_file_bytes(path);
    if (b.empty()) return "";
    return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

std::string get_self_executable_path() {
    wchar_t buf[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string path(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, path.data(), len, NULL, NULL);
    return path;
}

size_t get_pe_file_size(const uint8_t* data, size_t size) {
    if (size < sizeof(IMAGE_DOS_HEADER)) return size;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(data);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return size;
    if (static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > size) return size;

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(data + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return size;

    size_t pe_end = 0;
    const auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        size_t end = static_cast<size_t>(sec[i].PointerToRawData) + sec[i].SizeOfRawData;
        if (end > pe_end) pe_end = end;
    }
    return (pe_end > 0 && pe_end <= size) ? pe_end : size;
}

bool inject_res_file(HANDLE hUpdate, const std::string& res_path, std::string& out_err) {
    ByteVector res_bytes = read_file_bytes(res_path);
    if (res_bytes.empty()) {
        out_err = "failed to read resource file: " + res_path;
        return false;
    }

    const uint8_t* ptr = res_bytes.data();
    size_t size = res_bytes.size();
    size_t offset = 0;

    auto read_id_or_str = [&](size_t& off, LPCWSTR& out_res, std::wstring& out_holder) -> bool {
        if (off + 2 > size) return false;
        uint16_t first = *reinterpret_cast<const uint16_t*>(ptr + off);
        if (first == 0xFFFF) {
            if (off + 4 > size) return false;
            uint16_t id = *reinterpret_cast<const uint16_t*>(ptr + off + 2);
            out_res = MAKEINTRESOURCEW(id);
            off += 4;
            return true;
        } else {
            out_holder.clear();
            while (off + 2 <= size) {
                uint16_t ch = *reinterpret_cast<const uint16_t*>(ptr + off);
                off += 2;
                if (ch == 0) break;
                out_holder.push_back(static_cast<wchar_t>(ch));
            }
            out_res = out_holder.c_str();
            return true;
        }
    };

    while (offset + sizeof(DWORD) * 2 <= size) {
        DWORD data_size = *reinterpret_cast<const DWORD*>(ptr + offset);
        DWORD header_size = *reinterpret_cast<const DWORD*>(ptr + offset + 4);
        if (header_size < sizeof(DWORD) * 2 || offset + header_size > size) break;

        size_t entry_data_offset = offset + header_size;
        size_t next_entry_offset = entry_data_offset + ((data_size + 3) & ~3);

        size_t hdr_ptr = offset + 8;
        LPCWSTR type_ptr = nullptr;
        std::wstring type_str;
        if (!read_id_or_str(hdr_ptr, type_ptr, type_str)) break;

        LPCWSTR name_ptr = nullptr;
        std::wstring name_str;
        if (!read_id_or_str(hdr_ptr, name_ptr, name_str)) break;

        hdr_ptr = (hdr_ptr + 3) & ~3;
        if (hdr_ptr + 16 > offset + header_size) {
            offset = next_entry_offset;
            continue;
        }

        WORD lang_id = *reinterpret_cast<const WORD*>(ptr + hdr_ptr + 6);

        if (data_size > 0 && entry_data_offset + data_size <= size) {
            const void* data_ptr = ptr + entry_data_offset;
            UpdateResourceW(hUpdate, type_ptr, name_ptr, lang_id, const_cast<void*>(data_ptr), data_size);
        }

        offset = next_entry_offset;
    }
    return true;
}

bool compile_and_inject_rc(HANDLE hUpdate, const std::string& rc_path, std::string& out_err) {
    char temp_dir[MAX_PATH] = { 0 };
    GetTempPathA(MAX_PATH, temp_dir);
    char temp_res[MAX_PATH] = { 0 };
    GetTempFileNameA(temp_dir, "rc", 0, temp_res);
    std::string res_path = temp_res;
    size_t dot = res_path.find_last_of('.');
    if (dot != std::string::npos) res_path = res_path.substr(0, dot) + ".res";
    else res_path += ".res";
    DeleteFileA(temp_res);

    std::string cmd = "rc.exe /nologo /fo \"" + res_path + "\" \"" + rc_path + "\"";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        DeleteFileA(res_path.c_str());
        out_err = "rc.exe compilation failed for " + rc_path;
        return false;
    }

    bool ok = inject_res_file(hUpdate, res_path, out_err);
    DeleteFileA(res_path.c_str());
    return ok;
}

}

bool build_executable(const BuildOptions& opts, std::string& out_error) {
    std::string bat_source = read_file_text(opts.source_path);
    if (bat_source.empty()) {
        out_error = "cannot read file: " + opts.source_path;
        return false;
    }

    Program prog = parser::parse_script(bat_source);
    prog.raw_script = bat_source;
    prog = optimize_program(std::move(prog));

    if (opts.dump_disassembly) {
        std::cout << dump_disassembly(prog) << "\n";
    }

    ByteVector bytecode = serialize_bytecode(prog);
    if (bytecode.empty()) {
        out_error = "serialization failed";
        return false;
    }

    std::string key = opts.encryption_key.empty() ? crypto::generate_random_key(24) : opts.encryption_key;
    ByteVector encrypted_payload = crypto::encrypt(bytecode, key);

    std::string self_path = get_self_executable_path();
    ByteVector self_bytes = read_file_bytes(self_path);
    if (self_bytes.empty()) {
        out_error = "cannot read compiler stub: " + self_path;
        return false;
    }

    size_t base_size = get_pe_file_size(self_bytes.data(), self_bytes.size());
    ByteVector clean_pe(self_bytes.data(), self_bytes.data() + base_size);

    if (!write_file_bytes(opts.output_path, clean_pe)) {
        out_error = "cannot write target: " + opts.output_path;
        return false;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, opts.output_path.c_str(), -1, NULL, 0);
    if (wlen <= 0) {
        out_error = "path encoding error";
        return false;
    }
    std::wstring w_out_path(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, opts.output_path.c_str(), -1, w_out_path.data(), wlen);

    HANDLE hUpdate = BeginUpdateResourceW(w_out_path.c_str(), TRUE);
    if (!hUpdate) {
        out_error = "BeginUpdateResource failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }

    if (!opts.resource_path.empty()) {
        std::string lower_rc = opts.resource_path;
        for (char& c : lower_rc) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        std::string res_err;
        if (lower_rc.ends_with(".rc")) {
            if (!compile_and_inject_rc(hUpdate, opts.resource_path, res_err)) {
                EndUpdateResourceW(hUpdate, TRUE);
                out_error = res_err;
                return false;
            }
        } else {
            if (!inject_res_file(hUpdate, opts.resource_path, res_err)) {
                EndUpdateResourceW(hUpdate, TRUE);
                out_error = res_err;
                return false;
            }
        }
    }

    if (!opts.icon_path.empty()) {
        std::string icon_err;
        if (!inject_icon_into_handle(hUpdate, opts.icon_path, icon_err)) {
            EndUpdateResourceW(hUpdate, TRUE);
            out_error = "icon injection failed: " + icon_err;
            return false;
        }
    }

    uint32_t magic = MAGIC_FOOTER;
    uint32_t flags = FLAG_ENCRYPTED;
    if (opts.invisible_mode) {
        flags |= FLAG_INVISIBLE;
    }
    uint32_t payload_len = static_cast<uint32_t>(encrypted_payload.size());

    char key_buf[32] = { 0 };
    memcpy(key_buf, key.data(), std::min(key.size(), size_t(31)));

    size_t pad_size = static_cast<size_t>(opts.pad_kb) * 1024;
    size_t res_total_size = 4 + 4 + 4 + 32 + encrypted_payload.size() + pad_size;
    ByteVector res_buf;
    res_buf.reserve(res_total_size);

    auto append_u32 = [&](uint32_t v) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        res_buf.insert(res_buf.end(), p, p + 4);
    };

    append_u32(magic);
    append_u32(flags);
    append_u32(payload_len);
    res_buf.insert(res_buf.end(), reinterpret_cast<const uint8_t*>(key_buf), reinterpret_cast<const uint8_t*>(key_buf) + 32);
    res_buf.insert(res_buf.end(), encrypted_payload.begin(), encrypted_payload.end());

    if (pad_size > 0) {
        res_buf.resize(res_buf.size() + pad_size, 0x00);
    }

    if (!UpdateResourceW(hUpdate, MAKEINTRESOURCEW(10), MAKEINTRESOURCEW(RESOURCE_ID_PAYLOAD),
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                         res_buf.data(), static_cast<DWORD>(res_buf.size()))) {
        EndUpdateResourceW(hUpdate, TRUE);
        out_error = "UpdateResource failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }

    if (!EndUpdateResourceW(hUpdate, FALSE)) {
        out_error = "EndUpdateResource failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }

    return true;
}

std::optional<EmbeddedPayload> detect_embedded_payload() {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(RESOURCE_ID_PAYLOAD), MAKEINTRESOURCEW(10));
    if (hRes) {
        HGLOBAL hData = LoadResource(NULL, hRes);
        DWORD resSize = SizeofResource(NULL, hRes);
        if (hData && resSize >= 44) {
            const Byte* ptr = reinterpret_cast<const Byte*>(LockResource(hData));
            if (ptr) {
                uint32_t magic = *reinterpret_cast<const uint32_t*>(ptr);
                if (magic == MAGIC_FOOTER) {
                    uint32_t flags = *reinterpret_cast<const uint32_t*>(ptr + 4);
                    uint32_t payload_len = *reinterpret_cast<const uint32_t*>(ptr + 8);
                    std::string key(reinterpret_cast<const char*>(ptr + 12), 32);
                    key = key.c_str();

                    if (44 + payload_len <= resSize) {
                        const Byte* enc_data = ptr + 44;
                        ByteVector decrypted = crypto::decrypt(enc_data, payload_len, key);
                        auto opt_prog = deserialize_bytecode(decrypted.data(), decrypted.size());
                        if (opt_prog) {
                            EmbeddedPayload result;
                            result.program = std::move(*opt_prog);
                            result.flags = flags;
                            result.is_invisible = (flags & FLAG_INVISIBLE) != 0;
                            return result;
                        }
                    }
                }
            }
        }
    }

    std::string self_path = get_self_executable_path();
    ByteVector self_bytes = read_file_bytes(self_path);
    if (self_bytes.size() < 12 + 32) return std::nullopt;

    size_t footer_pos = self_bytes.size() - 12;
    uint32_t magic = *reinterpret_cast<const uint32_t*>(self_bytes.data() + footer_pos + 8);
    if (magic != MAGIC_FOOTER) return std::nullopt;

    uint32_t flags = *reinterpret_cast<const uint32_t*>(self_bytes.data() + footer_pos);
    uint32_t payload_len = *reinterpret_cast<const uint32_t*>(self_bytes.data() + footer_pos + 4);

    if (footer_pos < payload_len + 32) return std::nullopt;

    size_t key_offset = footer_pos - payload_len - 32;
    size_t payload_offset = key_offset + 32;

    std::string key(reinterpret_cast<const char*>(self_bytes.data() + key_offset), 32);
    key = key.c_str();

    const Byte* enc_data = self_bytes.data() + payload_offset;
    ByteVector decrypted = crypto::decrypt(enc_data, payload_len, key);

    auto opt_prog = deserialize_bytecode(decrypted.data(), decrypted.size());
    if (!opt_prog) return std::nullopt;

    EmbeddedPayload result;
    result.program = std::move(*opt_prog);
    result.flags = flags;
    result.is_invisible = (flags & FLAG_INVISIBLE) != 0;
    return result;
}

}
