#include "chart.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>
#include <vector>

using namespace std;

namespace chart {
static constexpr int W = 1024, H = 600;
static constexpr int PL = 80, PR = 24, PT = 44, PB = 56;
static constexpr int LEG_W = 240;

static constexpr string_view FONT = "ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Arial";
static constexpr string_view BG   = "#ffffff";
static constexpr string_view FG   = "#111827"; // slate-900
static constexpr string_view SUB  = "#6b7280"; // gray-500
static constexpr string_view GRID = "#e5e7eb"; // gray-200

static string esc(string_view s) {
    string out;
    out.reserve(s.size());
    for(char c : s) {
        switch(c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

static vector<int> all_xs(span<const series> ss) {
    vector<int> xs;
    for(auto & s : ss)
        for(auto & p : s.pts)
            xs.push_back(p.x);
    ranges::sort(xs);
    xs.erase(unique(begin(xs), end(xs)), end(xs));
    return xs;
}

static double max_y(span<const series> ss) {
    double m = 0.0;
    for(auto & s : ss)
        for(auto & p : s.pts)
            if(p.y)
                m = max(m, *p.y);
    return m;
}

static optional<double> y_at(span<const point> pts, int x) {
    for(auto & p : pts)
        if(p.x == x)
            return p.y;
    return nullopt;
}

string svg_lines(span<const series> ss, string_view title) {
    const auto xs = all_xs(ss);
    const auto my = max_y(ss);
    const int  PW = W - PL - PR, PH = H - PT - PB;

    const int xmin = xs.empty() ? 0 : xs.front();
    const int xmax = xs.empty() ? 1 : xs.back();

    auto x2px = [&](int x) -> double {
        if(xmax == xmin)
            return PL + PW * 0.5;
        return PL + (double(x - xmin) / double(xmax - xmin)) * PW;
    };
    auto y2py = [&](double y) -> double {
        if(my <= 0.0)
            return PT + PH;
        return PT + (1.0 - (y / my)) * PH;
    };

    string s;
    s.reserve(size_t(H) * 24);

    const auto t = esc(title);

    s += format(
      R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="{}" height="{}" viewBox="0 0 {} {}" role="img" aria-label="{}">
)svg",
      W,
      H,
      W,
      H,
      t);
    s += format(R"svg(<rect x="0" y="0" width="{}" height="{}" fill="{}"/>
)svg",
                W,
                H,
                BG);

    s += format(R"svg(<style>
:root{{font-family:{};}}
.t{{fill:{};font-size:16px;font-weight:650;}}
.a{{fill:{};font-size:12px;}}
.g{{stroke:{};stroke-width:1;shape-rendering:crispEdges;}}
.ax{{stroke:{};stroke-width:1.2;shape-rendering:crispEdges;}}
.p{{fill:{};stroke-width:2;fill-opacity:.12;}}
.l{{fill:{};font-size:12px;}}
</style>
)svg",
                FONT,
                FG,
                SUB,
                GRID,
                FG,
                FG,
                FG);

    s += format(R"svg(<text class="t" x="{}" y="{}">{}</text>
)svg",
                PL,
                28,
                t);

    // axes
    s += format(R"svg(<line class="ax" x1="{}" y1="{}" x2="{}" y2="{}"/>
<line class="ax" x1="{}" y1="{}" x2="{}" y2="{}"/>
)svg",
                PL,
                PT + PH,
                PL + PW,
                PT + PH,
                PL,
                PT,
                PL,
                PT + PH);

    // y grid ticks (0/25/50/75/100%)
    for(int k : {0, 1, 2, 3, 4}) {
        const double y  = my * (double(k) / 4.0);
        const double py = y2py(y);
        s += format(R"svg(<line class="g" x1="{}" y1="{}" x2="{}" y2="{}"/>
<text class="a" x="{}" y="{}" text-anchor="end">{:.0f}</text>
)svg",
                    PL,
                    py,
                    PL + PW,
                    py,
                    PL - 10,
                    py + 4,
                    y);
    }

    // x ticks: each measured num_fns
    for(int x : xs) {
        const double px = x2px(x);
        s += format(R"svg(<line class="g" x1="{}" y1="{}" x2="{}" y2="{}"/>
<text class="a" x="{}" y="{}" text-anchor="middle">{}</text>
)svg",
                    px,
                    PT,
                    px,
                    PT + PH,
                    px,
                    PT + PH + 22,
                    x);
    }

    // lines + points
    for(auto & se : ss) {
        string path;
        bool   pen = false;
        for(int x : xs) {
            const auto y = y_at(se.pts, x);
            if(!y) {
                pen = false;
                continue;
            }
            const auto px = x2px(x);
            const auto py = y2py(*y);
            path += format("{}{:.2f},{:.2f} ", pen ? "L" : "M", px, py);
            pen = true;
        }

        if(!path.empty()) {
            s += format(
              R"svg(<path d="{}" fill="none" stroke="{}" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/>
)svg",
              path,
              se.color);
        }

        for(int x : xs) {
            const auto y = y_at(se.pts, x);
            if(!y)
                continue;
            const auto px = x2px(x);
            const auto py = y2py(*y);
            s += format(R"svg(<circle cx="{:.2f}" cy="{:.2f}" r="3.4" fill="{}" />
)svg",
                        px,
                        py,
                        se.color);
        }
    }

    // legend (top-right)
    const int lx0 = PL + PW - LEG_W;
    int       ly  = PT - 6;
    for(auto & se : ss) {
        s += format(R"svg(<rect x="{}" y="{}" width="10" height="10" rx="2" fill="{}"/>
<text class="l" x="{}" y="{}">{}</text>
)svg",
                    lx0,
                    ly,
                    se.color,
                    lx0 + 16,
                    ly + 10,
                    esc(se.label));
        ly += 16;
    }

    // axis labels
    s += format(R"svg(<text class="a" x="{}" y="{}" text-anchor="end">ms</text>
<text class="a" x="{}" y="{}" text-anchor="end">functions</text>
)svg",
                PL + 20,
                PT - 10,
                PL + PW,
                PT + PH + 44);

    s += "</svg>\n";
    return s;
}

string md_pivot(span<const series> ss, string_view caption, string_view unit) {
    const auto xs = all_xs(ss);
    string     s;
    if(!caption.empty())
        s += format("{}\n\n", caption);
    if(!unit.empty())
        s += format("_Time in {} (lower is better)_\n\n", unit);

    s += "| Language |";
    for(int x : xs)
        s += format(" {} |", x);
    s += "\n|---|";
    for(size_t i = 0; i < xs.size(); ++i)
        s += "---:|";
    s += "\n";

    for(auto & se : ss) {
        s += format("| {} |", se.label);
        for(int x : xs) {
            const auto y = y_at(se.pts, x);
            if(y)
                s += format(" {:.3f} |", *y);
            else
                s += " N/A |";
        }
        s += "\n";
    }
    return s;
}

} // namespace chart
