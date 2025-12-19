#include <array>
#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <print>
#include <fstream>
#include <vector>
#include <charconv>
#include <cstring>
#include <chrono>
#include <optional>
#include <tuple>
#include <ranges>
#include <limits>
#include <span>

#include "exec.hpp"
#include "languages.hpp"
#include "chart.hpp"

using namespace std;

using sv = string_view;

struct tool_version_result {
    unsigned long    exit_code = 1;
    optional<string> version;
};

static string md_escape_inline_code(string_view s) {
    string out;
    out.reserve(s.size());
    for(const char c : s) {
        if(c == '`')
            out += "\\`";
        else if(c == '\r')
            continue;
        else
            out += c;
    }
    return out;
}

template <auto V>
constexpr auto enum_to_string() {
    sv s{__FUNCSIG__};
    return s.substr(s.rfind('<') + 1, s.rfind('>') - s.rfind('<') - 1);
}

template <int... i>
constexpr auto make_lang_names(index_sequence<i...>) {
    return array{enum_to_string<Lang(i)>()...};
}
constexpr sv lang_name(Lang l) { return make_lang_names(make_index_sequence<Lang::Count>{})[l]; }

template <Lang L>
struct Bench {
    using S = LangSpec<L>;
    static inline string src(int num_fns) {
        static string s;
        s.reserve(num_fns * 100);
        s.clear();

        if constexpr(requires { S::Prolog; })
            s += S::Prolog;

        for(int i = 0; i < num_fns; ++i) {
            s += vformat(sv{S::Function}, make_format_args(i, i));
            s += '\n';
        }

        s += S::MainStart;
        s += '\n';
        for(int i = 0; i < num_fns; ++i) {
            s += vformat(sv{S::SumStmt}, make_format_args(i));
            s += '\n';
        }
        s += S::MainEnd;

        return s;
    }
};

template <Lang L>
void gen_bench(auto filename, int num_fns) {
    ofstream{filename} << Bench<L>::src(num_fns);
    println("{} generated.", filename);
}

using duration_t = chrono::duration<double, milli>;

void clean() {
    error_code _;
    filesystem::remove("bench.exe", _);
    filesystem::remove("bench.pdb", _);
    filesystem::remove("bench.obj", _);
}

optional<duration_t> measure(auto cmd) {
    println("\nMeasuring: {}", cmd);
    array<duration_t, 5> times{};
    for(size_t i = 0; i < size(times); ++i) {
        const auto       start     = chrono::high_resolution_clock::now();
        const int        exit_code = exec(cmd, false).exit_code;
        const duration_t time_ms   = chrono::high_resolution_clock::now() - start;

        if(exit_code != 0) {
            println("...failed.");
            return {};
        }

        clean();

        times[i] = time_ms;
    }

    ranges::sort(times);
    return {times[times.size() / 2]}; // median
}

template <int... i>
auto bench_once(int num_fns, index_sequence<i...>) {
    const array filenames = {format("bench{}", LangSpec<Lang(i)>::Ext)...};
    (gen_bench<Lang(i)>(filenames[i], num_fns), ...);
    const array cmds = {vformat(sv{LangSpec<Lang(i)>::Cmd}, make_format_args(filenames[i]))...};
    const array raw  = {make_tuple(Lang(i), measure(cmds[i]))...};

    auto times = raw;
    ranges::stable_sort(times, {}, [](const auto & x) {
        const auto & t = get<1>(x);
        return t ? t->count() : numeric_limits<double>::infinity();
    });
    for(const auto & [lang, time] : times)
        if(time)
            println("{}: {}", lang_name(lang), *time);
        else
            println("{}: N/A", lang_name(lang));

    return array<optional<double>, size_t(Lang::Count)>{
      (get<1>(raw[i]) ? optional<double>{get<1>(raw[i])->count()} : nullopt)...};
}

template <Lang l>
tool_version_result tool_version() {
    if(const auto [exit_code, std_out] = exec(LangSpec<l>::VersionCmd); exit_code == 0) {
        auto non_empty = std_out | views::split('\n') | views::filter([](auto line) { return !line.empty(); });
        if(begin(non_empty) == end(non_empty))
            return tool_version_result{.exit_code = 0, .version = nullopt};

        const auto   fst_line = *begin(non_empty);
        const string version{begin(fst_line), end(fst_line)};
        return tool_version_result{.exit_code = 0, .version = version};
    } else
        return tool_version_result{.exit_code = exit_code, .version = nullopt};
}

template <int... i>
auto tools_versions(index_sequence<i...>) {
    return array<tool_version_result, size_t(Lang::Count)>{tool_version<Lang(i)>()...};
}

template <int... i>
void print_tools_versions(const auto & versions, index_sequence<i...>) {
    auto print_one = [&](Lang l) {
        const auto & r = versions[l];
        if(r.version)
            println("{}: {}", lang_name(l), *r.version);
        else
            println("Failed to obtain version for {} - returned {}", lang_name(l), r.exit_code);
    };
    (print_one(Lang(i)), ...);
}

template <int... i>
string tools_versions_md(const auto & versions, index_sequence<i...>) {
    string s;
    s += "### Tools\n\n";
    s += "| Language | Version |\n";
    s += "|---|---|\n";

    auto add_one = [&](Lang l) {
        const auto & r = versions[l];
        if(r.version)
            s += format("| {} | `{}` |\n", lang_name(l), md_escape_inline_code(*r.version));
        else
            s += format("| {} | N/A (exit code {}) |\n", lang_name(l), r.exit_code);
    };
    (add_one(Lang(i)), ...);
    return s;
}

int main(int argc, char * argv[]) {
    auto tmp_dir = filesystem::temp_directory_path();
    filesystem::current_path(tmp_dir);

    int        default_num_fns = 25'000;
    const bool custom_num_fns  = argc == 2;
    if(custom_num_fns)
        from_chars(argv[1], argv[1] + strlen(argv[1]), default_num_fns);

    constexpr auto all_langs = make_index_sequence<Lang::Count>{};
    const auto     versions  = tools_versions(all_langs);
    print_tools_versions(versions, all_langs);

    vector num_fns_to_measure = {default_num_fns};
    if(!custom_num_fns) {
        num_fns_to_measure = {10, 1000};
        for(int num_fns = 2000; num_fns < 32000; num_fns += 1000)
            num_fns_to_measure.push_back(num_fns);
    }

    array<vector<chart::point>, size_t(Lang::Count)> pts_by_lang;
    for(auto & v : pts_by_lang)
        v.reserve(num_fns_to_measure.size());

    for(auto num_fns : num_fns_to_measure) {
        println("\nGenerating bench sources with {} functions in {}:", num_fns, tmp_dir.string());
        const auto ms = bench_once(num_fns, all_langs);
        [&]<int... i>(index_sequence<i...>) { ((pts_by_lang[i].push_back({num_fns, ms[i]})), ...); }(all_langs);
    }

    array<chart::series, size_t(Lang::Count)> series{};
    [&]<int... i>(index_sequence<i...>) {
        ((series[i] = chart::series{lang_name(Lang(i)), gh_color(Lang(i)), span<const chart::point>{pts_by_lang[i]}}),
         ...);
    }(all_langs);

    const auto md_path = "results.md";
    ofstream{"results.svg"} << chart::svg_lines(series, "compiler_benchmark — compile time vs functions");
    ofstream{md_path} << format("![](results.svg)\n\n{}\n\n{}",
                                tools_versions_md(versions, all_langs),
                                chart::md_pivot(series, "### Results", "ms"));

    println("Done. Results are written to {}.", md_path);
    return 0;
}
