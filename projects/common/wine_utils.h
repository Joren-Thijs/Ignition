#pragma once

#ifdef _WIN32
#include <windows.h>

#include <stdexcept>
#include <string>

inline bool IsRunningInWine() {
    static bool is_running_in_wine = false;
    static bool wine_check_done = false;

    if (!wine_check_done) {
        is_running_in_wine = (GetProcAddress(GetModuleHandleA("ntdll"), "wine_get_version") != nullptr);
        wine_check_done = true;
    }
    
    return is_running_in_wine;
}

inline std::string WineGetDosFileName(const std::string& filename) {
    static LPWSTR (*CDECL wine_get_dos_file_name_ptr)(LPCSTR) = (decltype(wine_get_dos_file_name_ptr))
        GetProcAddress(GetModuleHandleA("KERNEL32"),
                       "wine_get_dos_file_name");

    if (!wine_get_dos_file_name_ptr) {
        throw std::runtime_error("wine_get_dos_file_name not found");
    }

    LPWSTR dos_name_w = wine_get_dos_file_name_ptr(filename.c_str());
    if (!dos_name_w) {
        return "";
    }

    std::wstring ws(dos_name_w);
    std::string dos_name(ws.begin(), ws.end());
    HeapFree( GetProcessHeap(), 0, dos_name_w );

    return dos_name;
    
}

inline std::string WineGetUnixFileName(const std::string& filename) {
    static LPSTR (*CDECL wine_get_unix_file_name_ptr)(LPCWSTR) = (decltype(wine_get_unix_file_name_ptr))
        GetProcAddress(GetModuleHandleA("KERNEL32"),
                       "wine_get_unix_file_name");
    
    if (!wine_get_unix_file_name_ptr) {
        throw std::runtime_error("wine_get_unix_file_name not found");
    }

    std::wstring filename_w(filename.begin(), filename.end());
    LPSTR unix_name_a = wine_get_unix_file_name_ptr(filename_w.c_str());
    if (!unix_name_a) {
        return "";
    }

    std::string unix_name(unix_name_a);
    HeapFree( GetProcessHeap(), 0, unix_name_a );
    
    return unix_name;
}
#endif