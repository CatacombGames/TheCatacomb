// DOS platform layer - DOS file names on the host file system.
//
// DOS is case-insensitive and uses backslashes. Reads look in the write
// directory (saved games, settings, high scores) first and then in the data
// directory (the game files); writes always go to the write directory so the
// game data can live in a read-only location.

#include "dos/machine.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <system_error>

namespace dos::fs {

namespace {

std::filesystem::path g_data_dir = ".";
std::filesystem::path g_write_dir = ".";

bool iequals(std::string_view a, std::string_view b)
{
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

std::vector<std::string> split(std::string_view name)
{
    std::vector<std::string> parts;
    std::string cur;
    for (char c : name) {
        if (c == '\\' || c == '/') {
            if (!cur.empty())
                parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty())
        parts.push_back(cur);
    return parts;
}

// Walk the components of a DOS path matching each one case-insensitively.
std::optional<std::filesystem::path> find_ci(const std::filesystem::path& dir, std::string_view name)
{
    std::error_code ec;
    std::filesystem::path cur = dir;
    for (const auto& part : split(name)) {
        if (part == ".")
            continue;
        if (part == "..") {
            cur /= "..";
            continue;
        }
        std::filesystem::path exact = cur / part;
        if (std::filesystem::exists(exact, ec)) {
            cur = exact;
            continue;
        }
        bool found = false;
        for (std::filesystem::directory_iterator it(cur, ec), end; !ec && it != end; it.increment(ec)) {
            if (iequals(it->path().filename().string(), part)) {
                cur = it->path();
                found = true;
                break;
            }
        }
        if (!found)
            return std::nullopt;
    }
    return cur;
}

bool is_absolute_dos(std::string_view name)
{
    return (name.size() > 1 && name[1] == ':') || (!name.empty() && (name[0] == '\\' || name[0] == '/'));
}

}  // namespace

void set_data_dir(const std::filesystem::path& p) { g_data_dir = p; }
void set_write_dir(const std::filesystem::path& p) { g_write_dir = p; }
const std::filesystem::path& data_dir() { return g_data_dir; }
const std::filesystem::path& write_dir() { return g_write_dir; }

std::filesystem::path resolve_read(std::string_view dos_name)
{
    if (is_absolute_dos(dos_name))
        return std::filesystem::path(std::string(dos_name));
    if (auto p = find_ci(g_write_dir, dos_name))
        return *p;
    if (auto p = find_ci(g_data_dir, dos_name))
        return *p;
    std::filesystem::path p = g_data_dir;
    for (const auto& part : split(dos_name))
        p /= part;
    return p;
}

std::filesystem::path resolve_write(std::string_view dos_name)
{
    if (is_absolute_dos(dos_name))
        return std::filesystem::path(std::string(dos_name));
    if (auto p = find_ci(g_write_dir, dos_name))
        return *p;
    std::filesystem::path p = g_write_dir;
    const auto parts = split(dos_name);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i + 1 < parts.size()) {
            if (auto sub = find_ci(p, parts[i]))
                p = *sub;
            else
                p /= parts[i];
        } else {
            p /= parts[i];
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    return p;
}

bool exists(std::string_view dos_name)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(resolve_read(dos_name), ec);
}

}  // namespace dos::fs
