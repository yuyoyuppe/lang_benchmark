#include "exec.hpp"

#include <Windows.h>
#include <consoleapi2.h>
#include <algorithm>
#include <cstring>

using namespace std;

namespace {
void sanitize_terminal_output_inplace(string & s);

template <class F>
struct defer_t {
    F f;
    ~defer_t() { f(); }
};
template <class F>
defer_t<F> defer(F f) {
    return {f};
}
} // namespace

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

    SECURITY_ATTRIBUTES         sa{sizeof(sa), nullptr, TRUE};
    HANDLE                      in_rd{}, in_wr{};
    HANDLE                      out_rd{}, out_wr{};
    HPCON                       hpc{};
    PROCESS_INFORMATION         pi_local{};
    PPROC_THREAD_ATTRIBUTE_LIST attr_list{};
    bool                        attr_list_initialized = false;

    const auto cleanup = defer([&] {
        close_handle(pi_local.hThread);
        close_handle(pi_local.hProcess);
        if(attr_list) {
            if(attr_list_initialized)
                DeleteProcThreadAttributeList(attr_list);
            HeapFree(GetProcessHeap(), 0, attr_list);
            attr_list = nullptr;
        }
        if(hpc) {
            ClosePseudoConsole(hpc);
            hpc = nullptr;
        }
        close_handle(in_rd);
        close_handle(in_wr);
        close_handle(out_rd);
        close_handle(out_wr);
    });

    auto fail = [&](const char * msg, unsigned long code = GetLastError()) -> exec_result {
        result.exit_code = code;
        result.std_out   = msg;
        return result;
    };

    if(!CreatePipe(&in_rd, &in_wr, &sa, 0))
        return fail("exec(): CreatePipe failed\n");
    if(!CreatePipe(&out_rd, &out_wr, &sa, 0))
        return fail("exec(): CreatePipe failed\n");

    // Parent will only write to in_wr and read from out_rd.
    SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);

    const HRESULT hr = CreatePseudoConsole(COORD{120, 40}, in_rd, out_wr, 0, &hpc);
    close_handle(in_rd);
    close_handle(out_wr);
    if(FAILED(hr))
        return fail("exec(): CreatePseudoConsole failed\n", static_cast<unsigned long>(hr));

    size_t bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    attr_list = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, bytes));
    if(!attr_list)
        return fail("exec(): HeapAlloc failed\n");
    if(!InitializeProcThreadAttributeList(attr_list, 1, 0, &bytes))
        return fail("exec(): InitializeProcThreadAttributeList failed\n");
    attr_list_initialized = true;

    if(!UpdateProcThreadAttribute(
         attr_list, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), nullptr, nullptr)) {
        return fail("exec(): UpdateProcThreadAttribute failed\n");
    }

    STARTUPINFOEXA siex{
      .StartupInfo     = {.cb = sizeof(siex)},
      .lpAttributeList = attr_list,
    };

    if(!CreateProcessA(nullptr,
                       buf,
                       nullptr,
                       nullptr,
                       FALSE,
                       EXTENDED_STARTUPINFO_PRESENT,
                       nullptr,
                       nullptr,
                       &siex.StartupInfo,
                       &pi_local))
        return fail("exec(): CreateProcessA failed\n");

    // Read output without blocking forever:
    // - ConPTY won't necessarily close the output pipe until the pseudo console is closed.
    // - Read only when bytes are available; otherwise poll process state.
    for(;;) {
        DWORD      avail   = 0;
        const BOOL peek_ok = PeekNamedPipe(out_rd, nullptr, 0, nullptr, &avail, nullptr);
        if(peek_ok && avail) {
            DWORD n = 0;
            while(avail) {
                const DWORD to_read = min<DWORD>(static_cast<DWORD>(sizeof(buf)), avail);
                if(!ReadFile(out_rd, buf, to_read, &n, nullptr) || n == 0)
                    break;
                result.std_out.append(buf, n);
                avail -= n;
            }
            continue;
        }
        if(!peek_ok) {
            // Pipe broke or other read-side failure; stop polling output and just wait for process.
            break;
        }

        const DWORD w = WaitForSingleObject(pi_local.hProcess, 10);
        if(w == WAIT_OBJECT_0)
            break;
        if(w == WAIT_FAILED)
            break;
    }

    // Ensure ConPTY releases its side so the output pipe can drain and close.
    // Keep stdin open until we're done; closing it early can cause some programs to terminate
    // with STATUS_CONTROL_C_EXIT in non-debugger launches.
    ClosePseudoConsole(hpc);
    hpc = nullptr;
    close_handle(in_wr);

    // Drain remaining buffered output (if any).
    for(;;) {
        DWORD avail = 0;
        if(!PeekNamedPipe(out_rd, nullptr, 0, nullptr, &avail, nullptr) || avail == 0)
            break;
        DWORD       n       = 0;
        const DWORD to_read = min<DWORD>(static_cast<DWORD>(sizeof(buf)), avail);
        if(!ReadFile(out_rd, buf, to_read, &n, nullptr) || n == 0)
            break;
        result.std_out.append(buf, n);
    }

    WaitForSingleObject(pi_local.hProcess, INFINITE);
    GetExitCodeProcess(pi_local.hProcess, &result.exit_code);

    sanitize_terminal_output_inplace(result.std_out);

    return result;
}

namespace {
void sanitize_terminal_output_inplace(string & s) {
    auto   csi_final = [](unsigned char c) { return c >= 0x40 && c <= 0x7E; };
    size_t w         = 0;
    for(size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if(c == 0x1B) { // ESC
            if(i + 1 < s.size()) {
                const unsigned char n = static_cast<unsigned char>(s[i + 1]);
                if(n == '[') { // CSI ... final
                    for(i += 2; i < s.size() && !csi_final(static_cast<unsigned char>(s[i])); ++i) {}
                    if(i < s.size())
                        ++i;
                    continue;
                }
                if(n == ']') { // OSC ... BEL or ST (ESC \)
                    for(i += 2; i < s.size() && s[i] != '\a' && !(s[i] == 0x1B && i + 1 < s.size() && s[i + 1] == '\\');
                        ++i) {}
                    if(i < s.size())
                        i += (s[i] == '\a') ? 1 : 2;
                    continue;
                }
                if(n == 'P') { // DCS ... ST (ESC \)
                    for(i += 2; i < s.size() && !(s[i] == 0x1B && i + 1 < s.size() && s[i + 1] == '\\'); ++i) {}
                    if(i < s.size())
                        i += 2;
                    continue;
                }
                i += 2; // other ESC sequences: skip ESC + one byte
                continue;
            }
            ++i;
            continue;
        }

        if(c == '\a' || c == '\r' || (c < 0x20 && c != '\n' && c != '\t')) {
            ++i;
            continue;
        }

        s[w++] = s[i++];
    }
    s.resize(w);
}
}