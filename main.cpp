#include "thz/lexer.hpp"
#include "thz/parser.hpp"
#include "thz/irbuilder.hpp"
#include "thz/codegen.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "thzc: não foi possível abrir '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    std::string input, output = "out.cpp";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) { output = argv[++i]; continue; }
        if (a == "-h" || a == "--help") {
            std::printf("Uso: thzc [arquivo.js] [-o saida.cpp]\n");
            return 0;
        }
        if (!a.empty() && a[0] != '-') input = a;
    }
    if (input.empty()) {
        std::cerr << "thzc: nenhum arquivo de entrada\n";
        return 1;
    }

    try {
        std::string src = readFile(input);

        thz::Lexer lexer(src);
        auto tokens = lexer.tokenize();

        thz::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();

        thz::IRBuilder builder(std::move(prog));
        auto module = builder.build();

        thz::Codegen cg(std::move(module));
        std::string cpp = cg.generate();

        std::ofstream out(output);
        if (!out) { std::cerr << "thzc: não foi possível escrever '" << output << "'\n"; return 1; }
        out << cpp;
        out.close();

        std::printf("OK: %s -> %s (%zu bytes)\n", input.c_str(), output.c_str(), cpp.size());
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "thzc: erro: " << e.what() << "\n";
        return 1;
    }
}