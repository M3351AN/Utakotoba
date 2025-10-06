// Copyright (c) 2025 渟雲. All rights reserved.
//
// This file is part of the Utakotoba project.
//
// Licensed under the GNU General Public License v3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   https://www.gnu.org/licenses/gpl-3.0.en.html
//
// -----------------------------------------------------------------------------
// File: utakotoba.cc
// Author: 渟雲(quq[at]outlook.it)
// Date: 2025-10-6
//
// Description:
//   This file contains functions for read netease cloudmusic client memory to
//   get lyrics.
//
// -----------------------------------------------------------------------------
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <Windows.h>
#include <tlHelp32.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace offsets {
// <?xml version="1.0" encoding="utf-8"?>
// <CheatTable CheatEngineTableVersion="46">
//   <CheatEntries>
//     <CheatEntry>
//       <ID>3</ID>
//       <Description>"lyric"</Description>
//       <ShowAsSigned>0</ShowAsSigned>
//       <VariableType>String</VariableType>
//       <Length>2048</Length>
//       <Unicode>1</Unicode>
//       <CodePage>0</CodePage>
//       <ZeroTerminate>1</ZeroTerminate>
//       <Address>"cloudmusic.dll"+01CCBB70</Address>
//       <Offsets>
//         <Offset>0</Offset>
//         <Offset>8</Offset>
//         <Offset>120</Offset>
//         <Offset>40</Offset>
//         <Offset>18</Offset>
//         <Offset>70</Offset>
//         <Offset>F8</Offset>
//         <Offset>210</Offset>
//       </Offsets>
//     </CheatEntry>
//   </CheatEntries>
//   <UserdefinedSymbols/>
// </CheatTable>

static constexpr std::uintptr_t kCtAddress = 0x01CCBB70;
static const std::vector<uintptr_t> kLyricsOffsets = {0x0,  0x8,  0x120, 0x40,
                                                      0x18, 0x70, 0xF8,  0x210};

}  // namespace offsets

namespace process {
HANDLE process_handle;

DWORD GetProcessIdByWindowTitle(const std::wstring& window_title) {
  HWND hWnd = FindWindowW(NULL, window_title.c_str());
  if (!hWnd) {
    return 0;
  }
  DWORD pid = 0;
  GetWindowThreadProcessId(hWnd, &pid);
  return pid;
}

uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* module_name) {
  HANDLE snapshot =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snapshot == INVALID_HANDLE_VALUE) return 0;

  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);

  if (Module32FirstW(snapshot, &me)) {
    do {
      if (_wcsicmp(me.szModule, module_name) == 0) {
        CloseHandle(snapshot);
        return reinterpret_cast<uintptr_t>(me.modBaseAddr);
      }
    } while (Module32NextW(snapshot, &me));
  }

  CloseHandle(snapshot);
  return 0;
}

template <typename T>
T ReadAddress(uintptr_t address) {
  T value{};
  SIZE_T bytes_read = 0;
  if (ReadProcessMemory(process::process_handle,
                        reinterpret_cast<LPCVOID>(address), &value,
                        sizeof(T), &bytes_read)) {
    return value;
  }
  return {};
}

inline std::wstring ReadWideString(uintptr_t ptr_address,
                                   size_t max_bytes = 2048) {
  if (ptr_address == 0) return L"";

  size_t max_chars = max_bytes / sizeof(wchar_t);
  std::wstring buffer;
  buffer.resize(max_chars);

  SIZE_T bytesRead = 0;
  if (ReadProcessMemory(process::process_handle,
                        reinterpret_cast<LPCVOID>(ptr_address), buffer.data(),
                        max_bytes, &bytesRead)) {
    buffer[max_chars - 1] = L'\0';
    return std::wstring(buffer.c_str());
  }

  return L"";
}

template <typename T>
T RefPtr(uintptr_t base_address, const std::vector<uintptr_t>& offsets) {
  uintptr_t address = base_address;
  for (size_t i = 0; i < offsets.size(); i++) {
    address = process::ReadAddress<uintptr_t>(address);
    address += offsets[offsets.size() - i - 1];
  }
  return static_cast<T>(address);
}
}  // namespace process

inline std::string WideToUtf8(const std::wstring& wstr) {
  if (wstr.empty()) return {};
  int length = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                static_cast<int>(wstr.size()), nullptr, 0,
                                nullptr, nullptr);
  std::string result(length, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
                      result.data(), length, nullptr, nullptr);
  return result;
}

auto main() -> int {
  SetConsoleOutputCP(CP_UTF8);

  DWORD process_id = process::GetProcessIdByWindowTitle(L"桌面歌词");

  process::process_handle = OpenProcess(
      PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE,
      process_id);

  uintptr_t base_address =
      process::GetModuleBaseAddress(process_id, L"cloudmusic.dll");
  if (process_id == 0) {
    printf("Failed to find process ID.\n");
    return 1;
  }
  if (process::process_handle == nullptr) {
    printf("Failed to open process.\n");
    return 1;
  }
  if (base_address == 0) {
    printf("Failed to get module base address.\n");
    return 1;
  }
  printf("Process ID: %lu\n", process_id);
  printf("Process Handle: %p\n", process::process_handle);
  printf("Module Base Address: %#llx\n", (uintptr_t)base_address);

  uintptr_t lyrics_address = process::RefPtr<uintptr_t>(
      base_address + offsets::kCtAddress, offsets::kLyricsOffsets);
  if (lyrics_address == 0) {
    printf("Failed to resolve lyrics address.\n");
    return 1;
  }
  printf("Lyrics Address: %#llx\n", (uintptr_t)lyrics_address);

  static std::wstring lyrics;
  while (true) {
    if (process::GetProcessIdByWindowTitle(L"桌面歌词") == 0) {
      printf("Process has exited.\n");
      return 0;
    }
    lyrics_address = process::RefPtr<uintptr_t>(
        base_address + offsets::kCtAddress, offsets::kLyricsOffsets);
    lyrics = process::ReadWideString(lyrics_address);

#ifdef NDEBUG
    system("cls");
#endif

    if (!lyrics.empty()) {
      std::string utf8 = WideToUtf8(lyrics);
      printf("%s\n", utf8.c_str());
    } else {
      printf("[No lyrics found]\n");
    }

    lyrics.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}
