#pragma once

enum Lang { Cpp, Zig, CSharp, Lua, Rust, JavaScript, Perl, Python, Odin, Count };

template <Lang>
struct LangSpec;

template <>
struct LangSpec<Lang::Cpp> {
    static constexpr char MainStart[]  = "#include <stdio.h>\nint main() {\nint sum = 0;";
    static constexpr char Function[]   = "int f{}() {{ return {}; }}";
    static constexpr char SumStmt[]    = "sum += f{}();";
    static constexpr char MainEnd[]    = "printf(\"%d\", sum);\n}";
    static constexpr char Ext[]        = ".cpp";
    static constexpr char Cmd[]        = "cl /nologo /std:c++20 {}";
    static constexpr char VersionCmd[] = "cl";
};

template <>
struct LangSpec<Lang::Zig> {
    static constexpr char Prolog[]     = "const std = @import(\"std\");\n";
    static constexpr char MainStart[]  = R"d(
pub fn main() void {
    var sum: u32 = 0;
)d";
    static constexpr char Function[]   = R"d(
fn f_{}() u32 {{
    return {};
}})d";
    static constexpr char SumStmt[]    = "sum += f_{}();";
    static constexpr char MainEnd[]    = R"d(
    std.debug.print("{d}\n", .{sum});
})d";
    static constexpr char Ext[]        = ".zig";
    static constexpr char Cmd[]        = "zig build-exe -ODebug {}";
    static constexpr char VersionCmd[] = "zig version";
};

template <>
struct LangSpec<Lang::CSharp> {
    static constexpr char Prolog[]     = "public static class Program {";
    static constexpr char MainStart[]  = R"d(
  public static void Main() {
    int sum = 0;)d";
    static constexpr char Function[]   = "  static int f{}() {{ return {}; }}";
    static constexpr char SumStmt[]    = "    sum += f{}();";
    static constexpr char MainEnd[]    = "    System.Console.WriteLine(sum);\n  }\n}";
    static constexpr char Ext[]        = ".cs";
    static constexpr char Cmd[]        = "csc -optimize- -nologo {}";
    static constexpr char VersionCmd[] = "csc -version";
};

template <>
struct LangSpec<Lang::Lua> {
    static constexpr char MainStart[]  = "\nlocal sum = 0";
    static constexpr char Function[]   = "function f{}() return {} end";
    static constexpr char SumStmt[]    = "sum = sum + f{}()";
    static constexpr char MainEnd[]    = "print(sum)";
    static constexpr char Ext[]        = ".lua";
    static constexpr char Cmd[]        = "luajit {}";
    static constexpr char VersionCmd[] = "luajit -v";
};

template <>
struct LangSpec<Lang::Rust> {
    static constexpr char MainStart[]  = R"d(
fn main() {
  let mut sum: i32 = 0;
)d";
    static constexpr char Function[]   = "fn f{}() -> i32 {{ {} }}";
    static constexpr char SumStmt[]    = "  sum += f{}();";
    static constexpr char MainEnd[]    = R"d(
  println!("{}", sum);
}
)d";
    static constexpr char Ext[]        = ".rs";
    static constexpr char Cmd[]        = "rustc --edition=2024 -C opt-level=0 {}";
    static constexpr char VersionCmd[] = "rustc -V";
};

template <>
struct LangSpec<Lang::JavaScript> {
    static constexpr char MainStart[]  = R"d(
function printCompat(x) {
  if (typeof globalThis.print === "function") globalThis.print(x);
  else if (typeof console !== "undefined" && typeof console.log === "function") console.log(x);
}

let sum = 0;)d";
    static constexpr char Function[]   = "function f{}() {{ return {}; }}";
    static constexpr char SumStmt[]    = "sum += f{}();";
    static constexpr char MainEnd[]    = "printCompat(sum);";
    static constexpr char Ext[]        = ".js";
    static constexpr char Cmd[]        = "deno {}";
    static constexpr char VersionCmd[] = "deno --version";
};

template <>
struct LangSpec<Lang::Perl> {
    static constexpr char MainStart[]  = "my $sum = 0;\n";
    static constexpr char Function[]   = "sub f{} {{ {} }}";
    static constexpr char SumStmt[]    = "$sum += f{}();";
    static constexpr char MainEnd[]    = "print \"$sum\n\";";
    static constexpr char Ext[]        = ".pl";
    static constexpr char Cmd[]        = "perl {}";
    static constexpr char VersionCmd[] = "perl -v";
};

template <>
struct LangSpec<Lang::Python> {
    static constexpr char MainStart[]  = "sum = 0";
    static constexpr char Function[]   = "def f{}():\n  return {}";
    static constexpr char SumStmt[]    = "sum += f{}()";
    static constexpr char MainEnd[]    = "print(sum)";
    static constexpr char Ext[]        = ".py";
    static constexpr char Cmd[]        = "python {}";
    static constexpr char VersionCmd[] = "python --version";
};

template <>
struct LangSpec<Lang::Odin> {
    static constexpr char Prolog[]     = "package main\n\nimport \"core:fmt\"\n";
    static constexpr char MainStart[]  = R"d(
main :: proc() {
    sum : i32 = 0)d";
    static constexpr char Function[]   = "f{} :: proc() -> i32 {{ return {} }}";
    static constexpr char SumStmt[]    = "    sum += f{}()";
    static constexpr char MainEnd[]    = R"d(
    fmt.println(sum)
})d";
    static constexpr char Ext[]        = ".odin";
    static constexpr char Cmd[]        = "odin build {} -file";
    static constexpr char VersionCmd[] = "odin version";
};

constexpr const char * gh_color(Lang l) {
    // GitHub Linguist colors
    switch(l) {
    case Lang::Cpp:
        return "#f34b7d";
    case Lang::Zig:
        return "#ec915c";
    case Lang::CSharp:
        return "#178600";
    case Lang::Lua:
        return "#000080";
    case Lang::Rust:
        return "#dea584";
    case Lang::JavaScript:
        return "#f1e05a";
    case Lang::Perl:
        return "#0298c3";
    case Lang::Python:
        return "#3572a5";
    case Lang::Odin:
        return "#60affe";
    default:
        return "#9ca3af"; // gray-400
    }
}
