#include "exec.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstring>

using namespace std;

exec_result exec(string_view cmd, bool capture_stdout) {
    PROCESS_INFORMATION pi{};
    char                buf[4096];
    exec_result         result;

    if(cmd.size() >= sizeof(buf)) {
        result.exit_code = 1;
        result.std_out   = "exec(): command too long\n";
        return result;
    }

    memcpy(buf, cmd.data(), cmd.size());
    buf[cmd.size()] = '\0';

    auto close_handle = [](HANDLE & h) {
        if(h) {
            CloseHandle(h);
            h = nullptr;
        }
    };

    STARTUPINFOA si{.cb = sizeof(si)};
    if(!capture_stdout) {
        if(!CreateProcessA(nullptr, buf, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            result.exit_code = GetLastError();
            result.std_out   = "exec(): CreateProcessA failed\n";
            return result;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &result.exit_code);
        close_handle(pi.hProcess);
        close_handle(pi.hThread);
        return result;
    }

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE              rd{}, wr{};
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = wr;

    if(!CreateProcessA(nullptr, buf, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        result.exit_code = GetLastError();
        result.std_out   = "exec(): CreateProcessA failed\n";
        close_handle(rd);
        close_handle(wr);
        return result;
    }

    close_handle(wr);

    DWORD n;
    while(ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n)
        result.std_out.append(buf, n);

    WaitForSingleObject(pi.hProcess, INFINITE);
    close_handle(rd);
    GetExitCodeProcess(pi.hProcess, &result.exit_code);
    close_handle(pi.hProcess);
    close_handle(pi.hThread);
    return result;
}
