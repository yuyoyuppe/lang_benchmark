#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace chart {

struct point {
    int                   x = 0;
    std::optional<double> y;
};

struct series {
    std::string_view       label;
    std::string_view       color;
    std::span<const point> pts;
};

std::string svg_lines(std::span<const series> ss, std::string_view title);
std::string md_pivot(std::span<const series> ss, std::string_view caption = {}, std::string_view unit = "ms");

} // namespace chart
