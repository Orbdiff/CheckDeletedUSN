#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <windows.h>
#include <urlmon.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <locale>
#include <codecvt>

bool __is_dotnet_runtime_installed__() {
    std::filesystem::path runtimePath = LR"(C:\Program Files\dotnet\shared\Microsoft.WindowsDesktop.App\9.0.9\)";
    return std::filesystem::exists(runtimePath);
}

bool __install_dotnet_runtime__() {
    std::wcout << L"[+] Downloading .NET Desktop Runtime 9.0.9...\n";
    const wchar_t* url = L"https://builds.dotnet.microsoft.com/dotnet/WindowsDesktop/9.0.9/windowsdesktop-runtime-9.0.9-win-x64.exe";
    std::filesystem::path tempPath = std::filesystem::temp_directory_path() / L"windowsdesktop-runtime-9.0.9-win-x64.exe";

    HRESULT hr = URLDownloadToFileW(nullptr, url, tempPath.c_str(), 0, nullptr);
    if (FAILED(hr)) {
        std::wcerr << L"[!] Failed to download .NET Runtime. HRESULT=" << hr << L"\n";
        return false;
    }

    std::wcout << L"[#] Installing .NET Runtime...\n";

    SHELLEXECUTEINFOW shEx{ sizeof(SHELLEXECUTEINFOW) };
    shEx.fMask = SEE_MASK_NOCLOSEPROCESS;
    shEx.lpFile = tempPath.c_str();
    shEx.lpParameters = L"/install /passive /norestart";
    shEx.nShow = SW_HIDE;

    if (!ShellExecuteExW(&shEx)) {
        std::wcerr << L"[!] Failed to start installer. Error: " << GetLastError() << L"\n";
        return false;
    }

    if (!shEx.hProcess) {
        std::wcerr << L"[!] Installer process handle is NULL.\n";
        return false;
    }

    std::wcout << L"[*] Installing, please wait...\n";
    WaitForSingleObject(shEx.hProcess, INFINITE);

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(shEx.hProcess, &exitCode)) {
        std::wcerr << L"[!] Failed to get installer exit code. Error: " << GetLastError() << L"\n";
        CloseHandle(shEx.hProcess);
        return false;
    }

    CloseHandle(shEx.hProcess);

    if (exitCode == 0) {
        std::wcout << L"[OK] .NET Runtime installed successfully.\n";
        try { std::filesystem::remove(tempPath); }
        catch (...) {}
        return true;
    }
    else {
        std::wcerr << L"[!] Installation failed. Exit code: " << exitCode << L"\n";
        return false;
    }
}

ULONGLONG SystemTimeToULL(const SYSTEMTIME& st) {
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

SYSTEMTIME __get_boot_time__() {
    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    ULONGLONG nowTicks = (static_cast<ULONGLONG>(ftNow.dwHighDateTime) << 32) | ftNow.dwLowDateTime;
    ULONGLONG uptimeMs = GetTickCount64();
    ULONGLONG bootTicks = nowTicks - uptimeMs * 10000ULL;

    FILETIME ftBoot{ static_cast<DWORD>(bootTicks & 0xFFFFFFFF), static_cast<DWORD>(bootTicks >> 32) };
    SYSTEMTIME stBoot{};
    FileTimeToSystemTime(&ftBoot, &stBoot);
    return stBoot;
}

void __prepare_folder__(const std::filesystem::path& folder) {
    if (std::filesystem::exists(folder))
        std::filesystem::remove_all(folder);
    std::filesystem::create_directory(folder);
}

void __download_files__(const std::filesystem::path& folder) {
    const std::vector<std::pair<std::wstring, std::wstring>> files{
        {LR"(https://cdn.discordapp.com/attachments/1426732682129313853/1426732790250213478/MFTECmd.exe?ex=68eeeef4&is=68ed9d74&hm=2661f3ca9f5954dac236b40dafe460fa5b299f6178271c97bd10b1634ed2671a&)", L"MFTECmd.exe"},
        {LR"(https://cdn.discordapp.com/attachments/1426732682129313853/1426732816879849572/MFTECmd.runtimeconfig.json?ex=68eeeefa&is=68ed9d7a&hm=b40e01af8b82a34aa3a9534de2e0ea142bab20a9c95b6f6deb526fd9df03f8bc&)", L"MFTECmd.runtimeconfig.json"},
        {LR"(https://cdn.discordapp.com/attachments/1426732682129313853/1426732847628292207/MFTECmd.dll?ex=68eeef02&is=68ed9d82&hm=37065fb984ac0b9d31c1bd880c95e624a38c579683c8cefeed754a813b2fe52f&)", L"MFTECmd.dll"}
    };

    for (const auto& [url, name] : files) {
        std::filesystem::path dest = folder / name;
        URLDownloadToFileW(NULL, url.c_str(), dest.c_str(), 0, NULL);
    }
}

void __run_mftecmd__(const std::filesystem::path& folder) {
    wchar_t cmdLine[1024];
    wsprintfW(cmdLine, L"/C MFTECmd.exe -f C:\\$MFT --csv \"%s\" --csvf MFT.csv", folder.c_str());

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    const wchar_t* app = L"C:\\Windows\\System32\\cmd.exe";

    BOOL ok = CreateProcessW(
        app,
        cmdLine,
        nullptr, nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        folder.c_str(),
        &si, &pi
    );

    if (ok) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        std::wcerr << L"[!] Failed to execute MFTECmd. Error: " << GetLastError() << L"\n";
    }
}

bool __find_usn_in_file__(const std::filesystem::path& filePath, SYSTEMTIME& stUsn) {
    std::wifstream ifs(filePath, std::ios::in | std::ios::binary);
    if (!ifs.is_open())
        return false;

    static constexpr size_t BUFFER_SIZE = 1 << 20; // 1 MB
    auto* buffer = new wchar_t[BUFFER_SIZE];
    ifs.rdbuf()->pubsetbuf(reinterpret_cast<wchar_t*>(buffer), BUFFER_SIZE);
    ifs.imbue(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));

    std::wstring line;
    std::wstring lastDate;
    bool found = false;

    while (std::getline(ifs, line)) {
        if (line.find(L".\\$Extend,$UsnJrnl:$J") == std::wstring::npos)
            continue;

        if (line.find(L'-') == std::wstring::npos || line.find(L':') == std::wstring::npos)
            continue;

        std::vector<std::wstring> columns;
        columns.reserve(30);
        size_t start = 0, end = 0;
        while ((end = line.find(L',', start)) != std::wstring::npos) {
            columns.push_back(line.substr(start, end - start));
            start = end + 1;
        }
        columns.push_back(line.substr(start));

        for (const auto& col : columns) {
            if (col.find(L'-') != std::wstring::npos && col.find(L':') != std::wstring::npos) {
                lastDate = col;
                found = true;
                break;
            }
        }

        if (found && !lastDate.empty())
            break;
    }

    delete[] buffer;

    if (!found || lastDate.empty())
        return false;

    size_t dotPos = lastDate.find(L'.');
    if (dotPos != std::wstring::npos)
        lastDate = lastDate.substr(0, dotPos);

    if (swscanf_s(lastDate.c_str(), L"%4hu-%2hu-%2hu %2hu:%2hu:%2hu",
        &stUsn.wYear, &stUsn.wMonth, &stUsn.wDay,
        &stUsn.wHour, &stUsn.wMinute, &stUsn.wSecond) == 6)
    {
        return true;
    }

    return false;
}

void __check_usn_journal__(const std::filesystem::path& folder, const SYSTEMTIME& stBoot) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    bool deletedShown = false;
    bool foundUsn = false;

    for (const auto& file : std::filesystem::directory_iterator(folder)) {
        if (file.path().extension() != L".csv") continue;

        SYSTEMTIME stUsn{};
        if (__find_usn_in_file__(file.path(), stUsn)) {
            std::wcout << L"[#] $UsnJrnl:$J entry detected in MFT at (UTC): "
                << stUsn.wYear << L"-" << stUsn.wMonth << L"-" << stUsn.wDay << L" "
                << stUsn.wHour << L":" << stUsn.wMinute << L":" << stUsn.wSecond << L"\n";

            ULONGLONG usnTicks = SystemTimeToULL(stUsn);
            ULONGLONG bootTicks = SystemTimeToULL(stBoot);

            if (usnTicks > bootTicks) {
                double diffMinutes = static_cast<double>(usnTicks - bootTicks) / (10'000'000 * 60);
                if (diffMinutes > 5.0 && !deletedShown) {
                    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
                    std::wcout << L"\n[!] UsnJrnl has been DELETED!\n";
                    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                    deletedShown = true;
                }
            }

            foundUsn = true;
            break;
        }
    }

    if (!foundUsn) {
        std::wcout << L"[/] No $UsnJrnl:$J entry found in MFT CSV\n";
    }
    else if (!deletedShown) {
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::wcout << L"\n[OK] $UsnJrnl:$J intact.\n";
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
}

void __usn__jrnl__main__() {
    std::filesystem::path folder = std::filesystem::current_path() / "MFTECmd_files";
    SYSTEMTIME stBoot = __get_boot_time__();

    std::wcout << L"[+] System boot time (UTC): "
        << stBoot.wYear << L"-" << stBoot.wMonth << L"-" << stBoot.wDay << L" "
        << stBoot.wHour << L":" << stBoot.wMinute << L":" << stBoot.wSecond << L"\n";

    __prepare_folder__(folder);
    __download_files__(folder);
    __run_mftecmd__(folder);
    __check_usn_journal__(folder, stBoot);
}

int wmain() {
    if (!__is_dotnet_runtime_installed__()) {
        if (!__install_dotnet_runtime__()) {
            std::wcerr << L"[!] Cannot continue without .NET Runtime.\n";
            system("pause");
            return 1;
        }
        Sleep(1000);
        system("cls");
    } else {
        std::wcout << L"[OK] .NET Desktop Runtime 9.0.9 already installed.\n";
        Sleep(1000);
        system("cls");
    }

    __usn__jrnl__main__();
    system("pause");
    return 0;
}
