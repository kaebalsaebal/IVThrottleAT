#pragma once
#include <windows.h>
namespace ce059 {
int initialize(HMODULE module) noexcept;
void deactivate() noexcept;
int status() noexcept;
}
