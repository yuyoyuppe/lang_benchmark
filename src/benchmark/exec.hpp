#pragma once

#include <string_view>
#include <string>

struct exec_result {
    unsigned long exit_code = 1;
    std::string   std_out;
};

exec_result exec(std::string_view cmd, bool capture_stdout = true);
