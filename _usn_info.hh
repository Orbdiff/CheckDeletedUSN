#pragma once
#include <windows.h>
#include <string>
#include <iostream>
#include <iomanip>

std::string __convert__filetime__to__string__(const FILETIME& ft) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

    char buffer[64];
    sprintf_s(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
    return buffer;
}

SYSTEMTIME __get__boot__time__() {
    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);

    ULONGLONG nowTicks = (static_cast<ULONGLONG>(ftNow.dwHighDateTime) << 32) | ftNow.dwLowDateTime;
    ULONGLONG uptimeMs = GetTickCount64();
    ULONGLONG bootTicks = nowTicks - uptimeMs * 10000ULL;

    FILETIME ftBoot{
        static_cast<DWORD>(bootTicks & 0xFFFFFFFF),
        static_cast<DWORD>(bootTicks >> 32)
    };

    SYSTEMTIME stBootUTC{}, stBootLocal{};
    FileTimeToSystemTime(&ftBoot, &stBootUTC);
    SystemTimeToTzSpecificLocalTime(nullptr, &stBootUTC, &stBootLocal);
    return stBootLocal;
}

bool __get__usn__creationtime__(FILETIME& outCreationTime) {
    const std::wstring usnPath = L"C:\\$Extend\\$UsnJrnl:$J";

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(usnPath.c_str(), GetFileExInfoStandard, &fad)) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
            std::wcout << L"[!] $UsnJrnl:$J not found." << std::endl;
        else
            std::wcout << L"[!] Failed to open '$UsnJrnl:$J' (Error " << err << L")" << std::endl;
        return false;
    }

    outCreationTime = fad.ftCreationTime;
    return true;
}

void __analyze__usn__status__() {
    FILETIME ftCreation{};
    if (!__get__usn__creationtime__(ftCreation))
        return;

    SYSTEMTIME stBoot = __get__boot__time__();
    FILETIME ftBoot;
    SystemTimeToFileTime(&stBoot, &ftBoot);

    ULONGLONG bootTicks = (static_cast<ULONGLONG>(ftBoot.dwHighDateTime) << 32) | ftBoot.dwLowDateTime;
    ULONGLONG createTicks = (static_cast<ULONGLONG>(ftCreation.dwHighDateTime) << 32) | ftCreation.dwLowDateTime;

    std::wstring usnPath = L"C:\\$Extend\\$UsnJrnl:$J";

    std::wcout << L"Path: " << usnPath << std::endl;
    std::wcout << L"CreationTime: " << __convert__filetime__to__string__(ftCreation).c_str() << std::endl;
    std::wcout << L"BootTime: " << __convert__filetime__to__string__(ftBoot).c_str() << std::endl;

    if (createTicks > bootTicks)
        std::wcout << L"\n[!] $UsnJrnl:$J has a creation date later than the system boot, it is very likely that the USN Journal was deleted." << std::endl;
    else
        std::wcout << L"\n[+] $UsnJrnl:$J Intact!" << std::endl;
}

void __usn__show__creationtime__() {
    __analyze__usn__status__();
}