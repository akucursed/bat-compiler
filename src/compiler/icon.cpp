#include "icon.hpp"
#include <vector>
#include <fstream>
#include <cstdint>

namespace batexe::compiler {

#pragma pack(push, 2)
struct IcoHeader {
    uint16_t reserved;
    uint16_t type;
    uint16_t count;
};

struct IcoEntry {
    uint8_t width;
    uint8_t height;
    uint8_t color_count;
    uint8_t reserved;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t bytes_in_res;
    uint32_t image_offset;
};

struct GrpIconEntry {
    uint8_t width;
    uint8_t height;
    uint8_t color_count;
    uint8_t reserved;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t bytes_in_res;
    uint16_t id;
};

struct GrpIconHeader {
    uint16_t reserved;
    uint16_t type;
    uint16_t count;
};
#pragma pack(pop)

bool inject_icon_into_handle(HANDLE hUpdate, const std::string& ico_path, std::string& out_error) {
    if (!hUpdate) {
        out_error = "invalid resource handle";
        return false;
    }

    std::ifstream ico_file(ico_path, std::ios::binary | std::ios::ate);
    if (!ico_file.is_open()) {
        out_error = "cannot open icon: " + ico_path;
        return false;
    }

    std::streamsize ico_size = ico_file.tellg();
    if (ico_size < sizeof(IcoHeader)) {
        out_error = "invalid icon size: " + ico_path;
        return false;
    }

    ico_file.seekg(0, std::ios::beg);
    std::vector<uint8_t> ico_data(static_cast<size_t>(ico_size));
    if (!ico_file.read(reinterpret_cast<char*>(ico_data.data()), ico_size)) {
        out_error = "cannot read icon: " + ico_path;
        return false;
    }

    const auto* header = reinterpret_cast<const IcoHeader*>(ico_data.data());
    if (header->type != 1 || header->count == 0) {
        out_error = "invalid icon format: " + ico_path;
        return false;
    }

    size_t min_header_size = sizeof(IcoHeader) + header->count * sizeof(IcoEntry);
    if (ico_data.size() < min_header_size) {
        out_error = "corrupted icon header: " + ico_path;
        return false;
    }

    const auto* entries = reinterpret_cast<const IcoEntry*>(ico_data.data() + sizeof(IcoHeader));

    for (uint16_t i = 0; i < header->count; ++i) {
        const auto& entry = entries[i];
        if (entry.image_offset + entry.bytes_in_res > ico_data.size()) {
            out_error = "icon bounds error: " + ico_path;
            return false;
        }

        const uint8_t* icon_bytes = ico_data.data() + entry.image_offset;
        uint16_t icon_id = static_cast<uint16_t>(i + 1);

        if (!UpdateResourceW(hUpdate, MAKEINTRESOURCEW(3), MAKEINTRESOURCEW(icon_id),
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                             const_cast<uint8_t*>(icon_bytes), entry.bytes_in_res)) {
            out_error = "UpdateResource RT_ICON failed (" + std::to_string(GetLastError()) + ")";
            return false;
        }
    }

    size_t grp_size = sizeof(GrpIconHeader) + header->count * sizeof(GrpIconEntry);
    std::vector<uint8_t> grp_buf(grp_size);

    auto* grp_hdr = reinterpret_cast<GrpIconHeader*>(grp_buf.data());
    grp_hdr->reserved = 0;
    grp_hdr->type = 1;
    grp_hdr->count = header->count;

    auto* grp_entries = reinterpret_cast<GrpIconEntry*>(grp_buf.data() + sizeof(GrpIconHeader));
    for (uint16_t i = 0; i < header->count; ++i) {
        const auto& entry = entries[i];
        auto& gent = grp_entries[i];

        gent.width = entry.width;
        gent.height = entry.height;
        gent.color_count = entry.color_count;
        gent.reserved = entry.reserved;
        gent.planes = entry.planes;
        gent.bit_count = entry.bit_count;
        gent.bytes_in_res = entry.bytes_in_res;
        gent.id = static_cast<uint16_t>(i + 1);
    }

    if (!UpdateResourceW(hUpdate, MAKEINTRESOURCEW(14), MAKEINTRESOURCEW(1),
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                         grp_buf.data(), static_cast<DWORD>(grp_buf.size()))) {
        out_error = "UpdateResource RT_GROUP_ICON failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }

    return true;
}

bool inject_icon_into_pe(const std::string& pe_path, const std::string& ico_path, std::string& out_error) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, pe_path.c_str(), -1, NULL, 0);
    if (wlen <= 0) {
        out_error = "path encoding error";
        return false;
    }
    std::wstring w_pe_path(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, pe_path.c_str(), -1, w_pe_path.data(), wlen);

    HANDLE hUpdate = BeginUpdateResourceW(w_pe_path.c_str(), FALSE);
    if (!hUpdate) {
        out_error = "BeginUpdateResource failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }

    if (!inject_icon_into_handle(hUpdate, ico_path, out_error)) {
        EndUpdateResourceW(hUpdate, TRUE);
        return false;
    }

    if (!EndUpdateResourceW(hUpdate, FALSE)) {
        out_error = "EndUpdateResource failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }

    return true;
}

}
