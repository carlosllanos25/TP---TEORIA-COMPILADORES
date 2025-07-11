* El archivo comand-menu.sh ejecuta un script con un menu para los comandos.

# Comandos de ejecución

##  Generar los archivos *.CPP y *.h 
antlr4 -no-listener -visitor -Dlanguage=Cpp -o src EasyRust.g4

## Preparar los archivos de construccion
cmake -S src -B build

## Compilar el proyecto en el directorio build
cmake --build build

## Ejecutar el programa
build/prog      or      build/prog test.hrust > hrust.ll

## Compilar el archivo llvm generado
lli hrust.ll

## Optimizar

### Nivel 0
opt -S -O0 hrust.ll -o hrust0.ll

### Nivel 1
opt -S -O1 hrust.ll -o hrust1.ll

## Compilar en assembler
llc hrust.ll

## Generar el ejecutable
clang hrust.s -o hrust.out -no-pie

## Ejecutar el compilador
./hrust.out
