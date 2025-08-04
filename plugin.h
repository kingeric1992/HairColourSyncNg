#pragma once

#include <SKSE/SKSE.h>
#include <REL/Relocation.h>
#include <RE/Skyrim.h>

using namespace std::literals;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "version.h"

template <std::size_t T>
inline std::string prnAOB(const BYTE(&a)[T]) {
	return std::accumulate(a + 1, a + T, std::format("{:#02X}", a[0]),
		[](std::string&& s, BYTE b) { return std::format("{} {:#02X}", s, b); });
};
#define SOURCE(f) std::source_location::current(__builtin_LINE(), __builtin_COLUMN(), f)

void hook();