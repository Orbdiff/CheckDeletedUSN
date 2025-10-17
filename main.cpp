#include "_usn_info.hh"
#include "privilege.h"

int main() {

    if (!EnableDebugPrivilege()) {
        std::wcerr << L"[!] Failed to enable SeDebugPrivilege (might require admin)\n";
    }

    __usn__show__creationtime__();
    system("pause");
    return 0;
}