#include <fstream>
#include <iostream>
#include <memory>

#include <filesystem>
namespace fs = std::filesystem;

#include <boost/program_options.hpp>
namespace opt = boost::program_options;

#include "preprocessor.h"

constexpr const auto lf = "\n";

std::unique_ptr<std::ofstream> output_file;
std::unique_ptr<std::ifstream> input_file;

int main(int argc, char* argv[]) {
    opt::options_description options{ "Command-Line Options" };
    options.add_options()
        ("help,h", "show this message")
        ("output,o", opt::value<std::string>(), "file to write result to")
        ("input,i", opt::value<std::string>(), "file to preprocess")
        ("include-dir,I", opt::value<std::vector<std::string>>(), "include directories")
        ("define,D", opt::value<std::vector<std::string>>(), "defined symbols");

    opt::variables_map vm;
    opt::command_line_parser parser{ argc, argv };

    parser.options(options);
    opt::store(parser.run(), vm);
    opt::notify(vm);

    if (vm.find("help") != vm.end()) {
        std::cout << options << lf;
        return EXIT_SUCCESS;
    }

    auto&& out = [&]() -> std::ostream& {
        auto o = vm.find("output");
        if (o == vm.end()) {
            return std::cout;
        } else {
            output_file.reset(new std::ofstream(o->second.as<std::string>().c_str(), std::ios::binary | std::ios::out));
            return *output_file;
        }
    }();

    auto&& in = [&]() -> std::istream& {
        auto i = vm.find("input");
        if (i == vm.end()) {
            return std::cin;
        } else {
            input_file.reset(new std::ifstream(i->second.as<std::string>().c_str(), std::ios::binary | std::ios::in));
            return *input_file;
        }
    }();

    std::vector<fs::path> include_dirs;
    auto dirs = vm.find("include-dir");
    if (dirs != vm.end()) {
        for (auto&& dir : dirs->second.as<std::vector<std::string>>()) {
            fs::path p(dir);

            if (fs::is_directory(p) == false) {
                std::cerr << "\"" << dir << "\" is not a directory" << lf;
                continue;
            }

            include_dirs.push_back(p);
        }
    }

    std::vector<preprocessor::define> defines;
    auto defs = vm.find("define");
    if (defs != vm.end()) {
        for (auto&& def : defs->second.as<std::vector<std::string>>()) {
            auto eq_pos = def.find_first_of('=');
            if (eq_pos >= 0) {
                defines.emplace_back(def.substr(0, eq_pos), def.substr(eq_pos + 1));
            } else {
                defines.emplace_back(def);
            }
        }
    }

    preprocessor pp{ out, include_dirs, defines };
    if (pp.preprocess_file(in)) {
        return EXIT_FAILURE;
    }
}
