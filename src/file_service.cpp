
#include "file_service.h"

#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include <parallel_hashmap/phmap.h>

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

    if (c.size() == 0) {
        c.push_back(' ');
    }
    return _data->_file_cache.emplace_back(std::move(abs_p), &*c.begin(), &*c.begin() + c.size());
}

struct memory_file_service_data {
    phmap::flat_hash_map<std::string, std::string> file_store;
};

memory_file_service::memory_file_service() : _data{std::make_unique<memory_file_service_data>()} {}

memory_file_service::~memory_file_service() {}

bool memory_file_service::add_file(std::string_view path, std::string_view content) {
    std::string strpath{path};
    auto it = _data->file_store.find(strpath);
    if (it != _data->file_store.end())
        return false;

    _data->file_store.emplace(std::move(strpath), content);
    return true;
}

std::string memory_file_service::remove_filename(std::string_view path) {
    auto off = path.find_last_of("\\/");
    if (off >= 0)
        return std::string{path.substr(0, off + 1)};

    return std::string{path};
}

bool memory_file_service::file_exists(std::string_view path) {
    return _data->file_store.find(path) != _data->file_store.end();
}

file_content memory_file_service::resolve_load(std::string_view, std::string_view path) {
    std::string strpath{path};
    auto it = _data->file_store.find(strpath);
    if (it == _data->file_store.end())
        return {"", nullptr, nullptr};

    return {std::move(strpath), &*it->second.begin(), &*it->second.begin() + it->second.size()};
}
