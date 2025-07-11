#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

#include "EasyRustLexer.h"
#include "EasyRustParser.h"
#include "EasyRustDriver.h"

using namespace antlr4;
using namespace std;

int main(int argc, const char *argv[]) {

    ifstream ifile;

    if (argc > 1) {
        ifile.open(argv[1]);
        if (!ifile.is_open()) {
            cerr << "Error: No se pudo abrir el archivo " << argv[1] << endl;
            return EXIT_FAILURE;
        }
    }

    istream &stream = argc > 1 ? ifile : cin;
    
    ANTLRInputStream input(stream);
    EasyRustLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    EasyRustParser parser(&tokens);

    tree::ParseTree *tree = parser.program();
    EasyRustDriver *driver = new EasyRustDriver();
    driver->visit(tree);

    // Obtener el código IR generado
    string ir_code = driver->getIR();

    // Definir nombres de archivos
    string input_filename = argc > 1 ? string(argv[1]) : "stdin";
    size_t last_dot = input_filename.find_last_of('.');
    string base_name = (last_dot == string::npos) ? input_filename : input_filename.substr(0, last_dot);
    string ir_filename = base_name + ".ll";
    string optimized_ir = base_name + "_opt.ll";
    string asm_filename = base_name + ".s";
    string exec_filename = base_name + ".out";

    // Guardar el IR en un archivo
    ofstream ir_file(ir_filename);
    if (!ir_file.is_open()) {
        cerr << "Error: No se pudo crear el archivo IR " << ir_filename << endl;
        return EXIT_FAILURE;
    }
    ir_file << ir_code;
    ir_file.close();
    cout << "IR guardado en " << ir_filename << endl;

    // Optimizar el IR (Nivel 1)
    string cmd_opt = "opt -S -O1 " + ir_filename + " -o " + optimized_ir;
    cout << "Ejecutando optimización: " << cmd_opt << endl;
    if (system(cmd_opt.c_str()) != 0) {
        cerr << "Error: Optimización fallida." << endl;
        return EXIT_FAILURE;
    }
    cout << "IR optimizado guardado en " << optimized_ir << endl;

    //  Compilar el IR optimizado a assembler
    string cmd_llc = "llc " + optimized_ir + " -o " + asm_filename;
    cout << "Compilando a assembler: " << cmd_llc << endl;
    if (system(cmd_llc.c_str()) != 0) {
        cerr << "Error: Compilación a assembler fallida." << endl;
        return EXIT_FAILURE;
    }
    cout << "Assembler guardado en " << asm_filename << endl;

    //Generar el ejecutable final usando clang
    string cmd_clang = "clang " + asm_filename + " -o " + exec_filename + " -no-pie";
    cout << "Generando ejecutable: " << cmd_clang << endl;
    if (system(cmd_clang.c_str()) != 0) {
        cerr << "Error: Generación del ejecutable fallida." << endl;
        return EXIT_FAILURE;
    }
    cout << "Ejecutable generado: " << exec_filename << endl;

    return EXIT_SUCCESS;
}
