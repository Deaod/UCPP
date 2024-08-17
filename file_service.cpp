
#include "file_service.h"

#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

constexpr const auto lf = "\n";

struct filesystem_service_data {
    std::vector<fs::path> _include_dirs;
    std::vector<std::vector<char>> _file_contents;
    std::vector<file_content> _file_cache;
};

filesystem_service::filesystem_service(std::vector<std::string> include_dirs) : 
    _data(std::make_unique<filesystem_service_data>())
{
    for (auto&& dir : include_dirs) {
        fs::path p(dir);

        if (fs::is_directory(p) == false) {
            std::cerr << "\"" << dir << "\" is not a directory" << lf;
            continue;
        }

        _data->_include_dirs.push_back(p);
    }
}

filesystem_service::~filesystem_service() = default;

std::string filesystem_service::remove_filename(std::string_view path) {
    return fs::path(path).remove_filename().string();
}

static bool file_exists_path(fs::path p) {
    std::error_code ec;
    auto s = fs::status(p, ec);
    return s.type() == fs::file_type::regular;
}

bool filesystem_service::file_exists(std::string_view path) {
    return file_exists_path(fs::path{path});
}

file_content filesystem_service::resolve_load(std::string_view cwd, std::string_view path) {
    fs::path p{path};
    if (p.is_absolute() && file_exists_path(p)) {
        goto load;
    }

    if (cwd != "") {
        p = fs::path(cwd);
        if (fs::is_directory(p) == false)
            p = p.parent_path();
        p /= path;
        if (file_exists_path(p))
            goto load;
    }

    for (auto&& dir : _data->_include_dirs) {
        p = fs::path(dir) / path;
        if (file_exists_path(p))
            goto load;
    }

    return {"", nullptr, nullptr};

load:
    std::string abs_p = fs::absolute(p).string();
    auto cached = std::find_if(_data->_file_cache.begin(), _data->_file_cache.end(), [abs_p](const file_content& fc) {
        return fc.file == abs_p;
    });

    if (cached != _data->_file_cache.end())
        return *cached;

    auto&& c = _data->_file_contents.emplace_back(size_t(fs::file_size(p)));
    std::fstream f{abs_p.c_str(), std::ios::in | std::ios::binary};
    f.read(c.data(), c.size());

    return _data->_file_cache.emplace_back(std::move(abs_p), &*c.begin(), &*c.begin() + c.size());
}

