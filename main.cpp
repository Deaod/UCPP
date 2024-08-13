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

    auto in_path = [&]() -> fs::path {
        auto i = vm.find("input");
        if (i == vm.end()) {
            return {};
        } else {
            return { i->second.as<std::string>() };
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

    std::vector<define> defines;
    auto defs = vm.find("define");
    if (defs != vm.end()) {
        for (auto&& def : defs->second.as<std::vector<std::string>>()) {
            auto&& [lex, err] = lexer{"cmdline"}.run(&*def.begin(), &*def.end());

            if (err.size() > 0) {
                std::cerr << "Could not parse define: " << def << lf;
                continue;
            }

            if (lex.rbegin()->type == lexeme_type::LINE_END)
                lex.pop_back_and_dispose(lexeme::disposer{});

            auto eqit = std::find_if(lex.begin(), lex.end(), [](const lexeme& l) {
                return l.type == lexeme_type::EQ;
            });

            auto def_name = std::find_if(lex.begin(), eqit, [](const lexeme& l) {
                return l.type == lexeme_type::IDENTIFIER;
            });

            std::vector<lexeme> def_cont{};
            for (auto it = ++eqit; it != lex.end(); ++it)
                if (it->type != lexeme_type::WHITESPACE && it->type != lexeme_type::COMMENT)
                    def_cont.push_back(*it);

            defines.emplace_back(*def_name, std::move(def_cont));
        }
    }

    preprocessor pp{ out, include_dirs, defines };
    if (pp.preprocess_file(in_path) == false) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
