#include <windows.h>
#include <urlmon.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

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
        {LR"(https://cdn.discordapp.com/attachments/1426732682129313853/1426732790250213478/MFTECmd.exe?ex=68ec4bf4&is=68eafa74&hm=24706c777bc12b93f2ea964b62c3346250a1a9cea536e238735e867de6cc7b46&)", L"MFTECmd.exe"},
        {LR"(https://cdn.discordapp.com/attachments/1426732682129313853/1426732816879849572/MFTECmd.runtimeconfig.json?ex=68ec4bfa&is=68eafa7a&hm=cd654a8d1895b5b112225763dda7daa91abb1679b74f9d39ad6a36efcd8551b3&)", L"MFTECmd.runtimeconfig.json"},
        {LR"(https://cdn.discordapp.com/attachments/1426732682129313853/1426732847628292207/MFTECmd.dll?ex=68ec4c02&is=68eafa82&hm=ad6cadcf7b16860ae5e17d8798c871c79f7c6bc9f4c10b3ce511824b6d0ac62c&)", L"MFTECmd.dll"}
    };

    for (const auto& [url, name] : files) {
        std::filesystem::path dest = folder / name;
        URLDownloadToFileW(NULL, url.c_str(), dest.c_str(), 0, NULL);
    }
}

void __run_mftecmd__(const std::filesystem::path& folder) {
    wchar_t cmdLine[1024];
    wsprintfW(cmdLine, L"/C MFTECmd.exe -f c:\\$MFT --csv .");

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
        std::wcerr << L"Failed to execute hidden CMD. Error: " << GetLastError() << L"\n";
    }
}

bool __find_usn_in_file__(const std::filesystem::path& filePath, SYSTEMTIME& stUsn) {
    std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
    if (!ifs) return false;

    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    if (!ifs.read(buffer.data(), size)) return false;

    const char* data = buffer.data();
    const char* end = data + buffer.size();
    const char* pattern = "$UsnJrnl";
    size_t pattern_len = 8;

    for (const char* p = data; p <= end - pattern_len; ++p) {
        if (std::memcmp(p, pattern, pattern_len) == 0) {
            const char* dateIt = std::search(p, end, "202", "202" + 3);
            if (dateIt == end || std::distance(dateIt, end) < 19) return false;

            std::string dateStr(dateIt, dateIt + 19);
            std::wstring wdateStr(dateStr.begin(), dateStr.end());
            swscanf_s(wdateStr.c_str(), L"%4hu-%2hu-%2hu %2hu:%2hu:%2hu",
                &stUsn.wYear, &stUsn.wMonth, &stUsn.wDay,
                &stUsn.wHour, &stUsn.wMinute, &stUsn.wSecond);
            return true;
        }
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
            std::wcout << L"[#] USNJrnl created on (UTC): "
                << stUsn.wYear << L"-" << stUsn.wMonth << L"-" << stUsn.wDay << L" "
                << stUsn.wHour << L":" << stUsn.wMinute << L":" << stUsn.wSecond << L"\n";

            ULONGLONG usnTicks = SystemTimeToULL(stUsn);
            ULONGLONG bootTicks = SystemTimeToULL(stBoot);

            if (usnTicks > bootTicks) {
                double diffMinutes = static_cast<double>(usnTicks - bootTicks) / (10'000'000 * 60);
                if (diffMinutes > 5.0 && !deletedShown) {
                    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
                    std::wcout << L"\n[!] UsnJrnl has been DELETED!!\n";
                    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                    deletedShown = true;
                }
            }

            foundUsn = true;
            break;
        }
    }

    if (!foundUsn) {
        std::wcout << L"[/] No USNJrnl logs found\n";
    }
    else if (!deletedShown) {
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::wcout << L"\n[OK] UsnJrnl has not been deleted.\n";
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
