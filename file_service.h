#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct file_content {
    std::string file;
    char* begin;
    char* end;
};

struct file_service {
    virtual ~file_service() {}

    virtual std::string remove_filename(std::string_view path) = 0;
    virtual bool file_exists(std::string_view path) = 0;
    virtual file_content resolve_load(std::string_view cwd, std::string_view path) = 0;
};

struct filesystem_service_data;

struct filesystem_service : file_service {
    filesystem_service(std::vector<std::string> include_dirs);
    ~filesystem_service();

    std::string remove_filename(std::string_view path) override;
    bool file_exists(std::string_view path) override;
    file_content resolve_load(std::string_view cwd, std::string_view path) override;

private:
    std::unique_ptr<filesystem_service_data> _data;
};
