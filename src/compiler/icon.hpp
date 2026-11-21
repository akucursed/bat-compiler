#pragma once
#include <windows.h>
#include <string>

namespace batexe::compiler {

bool inject_icon_into_handle(HANDLE hUpdate, const std::string& ico_path, std::string& out_error);
bool inject_icon_into_pe(const std::string& pe_path, const std::string& ico_path, std::string& out_error);

}
