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

    /**
     * Removes the last filename component from a path
     */
    virtual std::string remove_filename(std::string_view path) = 0;
    
    /**
     * Returns true if the file specified by the path exists, returns false otherwise
     */
    virtual bool file_exists(std::string_view path) = 0;
    
    /**
     * Tries to resolve path to a file, loads that file into memory and returns information about the file.
     * Content pointers are nullptr if the file couldnt be found/loaded.
     */
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

struct memory_file_service_data;

struct memory_file_service : file_service {
    memory_file_service();
    ~memory_file_service();

    bool add_file(std::string_view path, std::string_view content);

    std::string remove_filename(std::string_view path) override;
    bool file_exists(std::string_view path) override;
    file_content resolve_load(std::string_view cwd, std::string_view path) override;

private:
    std::unique_ptr<memory_file_service_data> _data;
};
