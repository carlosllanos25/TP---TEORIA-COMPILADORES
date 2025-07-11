#include "antlr4-runtime.h"
#include "EasyRustBaseVisitor.h"
#include "EasyRustLexer.h"
#include "EasyRustParser.h"

#include <cmath>
#include <map>
#include "llvm/ADT/APInt.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

using namespace antlr4;
using namespace llvm;

using namespace std;

class EasyRustDriver : public EasyRustVisitor
{
private:
    struct SymbolInfo
    {
        llvm::Type *type; // Tipo de la variable (por ejemplo, int, float, etc.)
        std::string logicalType;
        llvm::Value *llvmValue; // Referencia a la posición en memoria (AllocaInst, etc.)
    };
    LLVMContext context;
    std::unique_ptr<Module> module;
    std::unique_ptr<IRBuilder<>> builder;
    std::unordered_map<std::string, SymbolInfo> symbolTable;
    FunctionCallee printfFunc;
    FunctionCallee expFunc;
    std::string irString;

public:
    EasyRustDriver()
    {
        module = std::make_unique<Module>("EasyRustModule", context);
        builder = std::make_unique<IRBuilder<>>(context);

        std::vector<Type *> printfArgs;
        printfArgs.push_back(PointerType::getUnqual(Type::getInt8Ty(context)));

        FunctionType *printfType = FunctionType::get(
            Type::getInt32Ty(context), printfArgs, true);
        printfFunc = module->getOrInsertFunction("printf", printfType);

        FunctionType *mathFuncType = FunctionType::get(
            Type::getDoubleTy(context), {Type::getDoubleTy(context)}, false);

        expFunc = module->getOrInsertFunction("exp", mathFuncType);
    }
    std::string getIR() const
    {
        return irString;
    }

    llvm::Type *getLLVMTypeFromLogicalType(const std::string &logicalType, llvm::LLVMContext &context)
    {
        if (logicalType == "int")
        {
            return llvm::Type::getInt32Ty(context);
        }
        else if (logicalType == "float")
        {
            return llvm::Type::getDoubleTy(context);
        }
        else if (logicalType == "string")
        {
            return llvm::PointerType::getUnqual(context); // Puntero a char para cadenas
        }
        else if (logicalType == "bool")
        {
            return llvm::Type::getInt1Ty(context);
        }
        else if (logicalType == "void")
        {
            return llvm::Type::getVoidTy(context); // Manejo del tipo void
        }
        else
        {
            std::cerr << "Error: Tipo no soportado '" << logicalType << "'\n";
            return nullptr;
        }
    }

    std::any visitProgram(EasyRustParser::ProgramContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitProgram\n";

        // Crear la función main
        FunctionType *mainType = FunctionType::get(Type::getInt32Ty(context), false);
        Function *mainFunc = Function::Create(mainType, Function::ExternalLinkage, "main", module.get());

        BasicBlock *entry = BasicBlock::Create(context, "entry", mainFunc);
        builder->SetInsertPoint(entry);

        for (auto stmt : ctx->statement())
        {
            if (stmt->functionDecl())
            {
                // Visitar la declaración de la función
                visit(stmt);
                // Restaurar el punto de inserción al bloque de entrada de main
                builder->SetInsertPoint(entry);
            }
            else
            {
                // Visitar otras declaraciones que se insertan en main
                visit(stmt);
            }
        }

        if (!builder->GetInsertBlock()->getTerminator())
        {
            llvm::errs() << "Debug: Agregando retorno final al main\n";
            builder->CreateRet(ConstantInt::get(Type::getInt32Ty(context), 0));
        }

        // Verificar función y módulo
        if (verifyFunction(*mainFunc, &errs()))
        {
            errs() << "Error: La función main contiene errores\n";
        }
        if (verifyModule(*module, &errs()))
        {
            errs() << "Error: El módulo contiene errores\n";
        }

        // Imprimir el módulo
        // Serializar el módulo a una cadena
        llvm::raw_string_ostream rso(irString);
        module->print(rso, nullptr);
        rso.flush();

        llvm::errs() << "Debug: IR almacenado internamente en EasyRustDriver\n";

        return nullptr;
    }

    std::any visitStatement(EasyRustParser::StatementContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitStatement\n";
        if (ctx->functionDecl())
        {
            return visit(ctx->functionDecl());
        }
        else if (ctx->printStmt())
        {
            return visit(ctx->printStmt());
        }
        else if (ctx->forLoop())
        {
            return visit(ctx->forLoop());
        }
        else if (ctx->whileLoop())
        {
            return visit(ctx->whileLoop());
        }
        else if (ctx->variableDecl())
        {
            return visit(ctx->variableDecl());
        }
        else if (ctx->exprStmt())
        {
            return visit(ctx->exprStmt());
        }
        else if (ctx->ifStmt())
        {
            return visit(ctx->ifStmt());
        }
        else if (ctx->returnStmt())
        {
            return visit(ctx->returnStmt());
        }
        else if (ctx->assignmentStmt())
        {
            return visit(ctx->assignmentStmt());
        }

        llvm::errs() << "Error: Tipo de statement no reconocido\n";
        return nullptr;
    }

    std::any visitVariableDecl(EasyRustParser::VariableDeclContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitVariableDecl\n";
        std::string varName = ctx->IDENTIFIER()->getText();
        llvm::errs() << "Debug: Variable identificada: " << varName << "\n";
        std::string logicalType = ctx->type()->getText(); // Obtiene el tipo lógico directamente de la gramática
        llvm::errs() << "Debug: Tipo lógico: " << logicalType << "\n";
        Value *exprValue = nullptr;
        try
        {
            exprValue = std::any_cast<Value *>(visit(ctx->expr()));
        }
        catch (const std::bad_any_cast &e)
        {
            llvm::errs() << "Error: std::bad_any_cast en visitVariableDecl al procesar expr: "
                         << ctx->expr()->getText() << "\n";
            llvm::errs() << "Excepción: " << e.what() << "\n";
            return nullptr;
        }

        if (!exprValue)
        {
            llvm::errs() << "Error: visit(ctx->expr()) retornó nullptr para la expresión: "
                         << ctx->expr()->getText() << "\n";
            return nullptr;
        }
        llvm::Type *llvmType = getLLVMTypeFromLogicalType(logicalType, context);
        if (!llvmType)
        {
            std::cerr << "Error: Tipo no soportado para la variable '" << varName << "'\n";
            return nullptr;
        }

        // Validar que el tipo del valor coincide con el tipo lógico
        if (logicalType == "int" && exprValue->getType()->isDoubleTy())
        {
            llvm::errs() << "Debug: Convertir double a int\n";
            exprValue = builder->CreateFPToSI(exprValue, llvm::Type::getInt32Ty(context), "double_to_int");
        }
        else if (logicalType == "float" && exprValue->getType()->isIntegerTy(32))
        {
            llvm::errs() << "Debug: Convertir int a float\n";
            exprValue = builder->CreateSIToFP(exprValue, llvm::Type::getDoubleTy(context), "int_to_double");
        }
        else if ((logicalType == "int" && !exprValue->getType()->isIntegerTy(32)) ||
                 (logicalType == "float" && !exprValue->getType()->isDoubleTy()))
        {
            std::cerr << "Error: Tipo incompatible para la variable '" << varName << "'\n";
            return nullptr;
        }

        AllocaInst *alloc = builder->CreateAlloca(llvmType, 0, varName.c_str());
        builder->CreateStore(exprValue, alloc);

        symbolTable[varName] = {llvmType, logicalType, alloc};
        return exprValue;
    }

    std::any visitFunctionDecl(EasyRustParser::FunctionDeclContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitFunctionDecl\n";

        std::string funcName = ctx->IDENTIFIER()->getText();
        std::string returnTypeStr = ctx->type()->getText();
        llvm::Type *returnType = getLLVMTypeFromLogicalType(returnTypeStr, context);

        if (!returnType)
        {
            llvm::errs() << "Error: Tipo de retorno no soportado para la función " << funcName << "\n";
            return nullptr;
        }

        // Crear el tipo de función
        std::vector<llvm::Type *> paramTypes;
        if (ctx->parameters())
        {
            for (auto &paramCtx : ctx->parameters()->parameter())
            {
                std::string paramTypeStr = paramCtx->type()->getText();
                llvm::Type *paramType = getLLVMTypeFromLogicalType(paramTypeStr, context);
                if (!paramType)
                {
                    llvm::errs() << "Error: Tipo de parámetro no soportado en la función " << funcName << "\n";
                    return nullptr;
                }
                paramTypes.push_back(paramType);
            }
        }

        llvm::FunctionType *funcType = llvm::FunctionType::get(returnType, paramTypes, false);
        llvm::Function *function = llvm::Function::Create(
            funcType, llvm::Function::ExternalLinkage, funcName, module.get());

        // Crear el bloque de entrada
        llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", function);
        builder->SetInsertPoint(entryBlock);

        // Registrar parámetros en la tabla de símbolos
        auto paramIt = function->arg_begin();
        if (ctx->parameters())
        {
            for (auto &paramCtx : ctx->parameters()->parameter())
            {
                std::string paramName = paramCtx->IDENTIFIER()->getText();
                paramIt->setName(paramName);

                // Reservar espacio para el parámetro en la pila
                llvm::AllocaInst *alloc = builder->CreateAlloca(paramIt->getType(), nullptr, paramName.c_str());
                builder->CreateStore(&(*paramIt), alloc);
                symbolTable[paramName] = {paramIt->getType(), paramCtx->type()->getText(), alloc};
                paramIt++;
            }
        }
        llvm::errs() << "Debug: Parámetros registrados para la función " << funcName << "\n";

        // Visitar las instrucciones en el cuerpo de la función
        for (auto &stmtCtx : ctx->statement())
        {
            visit(stmtCtx);
        }

        // Si la función es void, agrega un retorno explícito
        if (returnType->isVoidTy() && !builder->GetInsertBlock()->getTerminator())
        {
            builder->CreateRetVoid();
        }

        llvm::verifyFunction(*function);
        return nullptr;
    }

    std::any visitReturnStmt(EasyRustParser::ReturnStmtContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitReturnStmt\n";

        llvm::Value *returnValue = nullptr;
        if (ctx->expr())
        {
            returnValue = std::any_cast<llvm::Value *>(visit(ctx->expr()));
            if (!returnValue)
            {
                llvm::errs() << "Error: Valor de retorno inválido\n";
                return nullptr;
            }
        }

        // Verificar el tipo de retorno de la función actual
        Function *currentFunction = builder->GetInsertBlock()->getParent();
        Type *returnType = currentFunction->getReturnType();

        if (returnType->isVoidTy())
        {
            llvm::errs() << "Error: Función con tipo de retorno void no puede retornar un valor\n";
            return nullptr;
        }

        if (returnValue->getType() != returnType)
        {
            // Convertir el tipo de retorno si es necesario
            if (returnType->isDoubleTy() && returnValue->getType()->isIntegerTy())
            {
                llvm::errs() << "Debug: Convertir int a double para el retorno\n";
                returnValue = builder->CreateSIToFP(returnValue, Type::getDoubleTy(context), "int_to_double_ret");
            }
            else if (returnType->isIntegerTy() && returnValue->getType()->isDoubleTy())
            {
                llvm::errs() << "Debug: Convertir double a int para el retorno\n";
                returnValue = builder->CreateFPToSI(returnValue, Type::getInt32Ty(context), "double_to_int_ret");
            }
            else
            {
                llvm::errs() << "Error: Tipos de retorno incompatibles\n";
                return nullptr;
            }
        }

        // Emitir la instrucción de retorno
        builder->CreateRet(returnValue);
        return nullptr;
    }

    std::any visitParameters(EasyRustParser::ParametersContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitParameters\n";
        return visitChildren(ctx);
    }

    std::any visitParameter(EasyRustParser::ParameterContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitParameter\n";
        return visitChildren(ctx);
    }

    std::any visitPrintStmt(EasyRustParser::PrintStmtContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitPrintStmt\n";

        // Evalúa la expresión
        llvm::Value *exprValue = std::any_cast<llvm::Value *>(visit(ctx->expr()));
        if (!exprValue)
        {
            auto token = ctx->getStart();
            std::cerr << "Error: exprValue es nullptr en visitPrintStmt en línea "
                      << token->getLine() << ", columna " << token->getCharPositionInLine() << "\n";
            return std::any();
        }

        // Determina el formato basado en el tipo lógico
        llvm::Value *formatStr = nullptr;

        // Identifica el tipo de la expresión
        llvm::Type *exprType = exprValue->getType();
        if (exprType->isIntegerTy())
        {
            // Si es un entero
            llvm::errs() << "Debug: Expresión es un entero\n";
            exprValue = builder->CreateSIToFP(exprValue, llvm::Type::getDoubleTy(context), "int_to_double");
            formatStr = builder->CreateGlobalString("%lf\n", "fmt");
        }
        else if (exprType->isDoubleTy())
        {
            // Si es un flotante
            llvm::errs() << "Debug: Expresión es un flotante\n";
            formatStr = builder->CreateGlobalString("%lf\n", "fmt");
        }
        else if (exprType->isPointerTy())
        {
            // Si es una cadena
            llvm::errs() << "Debug: Expresión es una cadena\n";
            formatStr = builder->CreateGlobalString("%s\n", "fmt");
        }
        else
        {
            llvm::errs() << "Error: Tipo no soportado para impresión\n";
            return std::any();
        }

        // Crea la llamada a printf
        std::vector<llvm::Value *> printfArgs = {formatStr, exprValue};
        builder->CreateCall(printfFunc, printfArgs, "printf_call");

        return std::any();
    }

    std::any visitForLoop(EasyRustParser::ForLoopContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitForLoop\n";
        return visitChildren(ctx);
    }

    std::any visitWhileLoop(EasyRustParser::WhileLoopContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitWhileLoop\n";

        // Crear los bloques básicos necesarios
        llvm::Function *currentFunction = builder->GetInsertBlock()->getParent();

        // Bloque de condición
        llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(context, "while.cond", currentFunction);
        // Bloque del cuerpo del bucle
        llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(context, "while.body", currentFunction);
        // Bloque de salida
        llvm::BasicBlock *exitBlock = llvm::BasicBlock::Create(context, "while.exit", currentFunction);

        // Crear salto incondicional al bloque de condición
        builder->CreateBr(condBlock);

        // Insertar en el bloque de condición
        builder->SetInsertPoint(condBlock);

        // Evaluar la condición
        llvm::Value *condValue = std::any_cast<llvm::Value *>(visit(ctx->condition()));
        if (!condValue)
        {
            llvm::errs() << "Error: Condición no válida en while\n";
            return nullptr;
        }

        // Crear salto condicional basado en la condición
        builder->CreateCondBr(condValue, bodyBlock, exitBlock);

        // Insertar en el bloque del cuerpo
        builder->SetInsertPoint(bodyBlock);

        // Visitar las declaraciones dentro del cuerpo del bucle
        for (auto stmt : ctx->statement())
        {
            visit(stmt);
        }

        // Salto de regreso al bloque de condición
        builder->CreateBr(condBlock);

        // Insertar en el bloque de salida
        builder->SetInsertPoint(exitBlock);

        return nullptr;
    }
    std::any visitAssignmentStmt(EasyRustParser::AssignmentStmtContext *ctx)
    {
        llvm::errs() << "Debug: Entrando a visitAssignmentStmt\n";

        // Obtener el nombre de la variable
        std::string varName = ctx->IDENTIFIER()->getText();
        llvm::errs() << "Debug: Asignando a variable: " << varName << "\n";

        // Verificar que la variable esté definida en la tabla de símbolos
        if (symbolTable.find(varName) == symbolTable.end())
        {
            std::cerr << "Error: Variable '" << varName << "' no está definida\n";
            return nullptr;
        }

        // Obtener la información de la variable desde la tabla de símbolos
        auto &symbolInfo = symbolTable[varName];
        llvm::Type *varType = symbolInfo.type;
        const std::string &logicalType = symbolInfo.logicalType;

        // Evaluar la expresión del lado derecho
        llvm::Value *exprValue = std::any_cast<llvm::Value *>(visit(ctx->expr()));
        if (!exprValue)
        {
            llvm::errs() << "Error: Valor inválido en la asignación a '" << varName << "'\n";
            return nullptr;
        }

        // Validar que el tipo del valor coincide con el tipo de la variable
        if (logicalType == "int" && exprValue->getType()->isDoubleTy())
        {
            llvm::errs() << "Debug: Convertir double a int para asignación\n";
            exprValue = builder->CreateFPToSI(exprValue, llvm::Type::getInt32Ty(context), "double_to_int");
        }
        else if (logicalType == "float" && exprValue->getType()->isIntegerTy(32))
        {
            llvm::errs() << "Debug: Convertir int a float para asignación\n";
            exprValue = builder->CreateSIToFP(exprValue, llvm::Type::getDoubleTy(context), "int_to_double");
        }
        else if ((logicalType == "int" && !exprValue->getType()->isIntegerTy(32)) ||
                 (logicalType == "float" && !exprValue->getType()->isDoubleTy()))
        {
            std::cerr << "Error: Tipo incompatible en la asignación a '" << varName << "'\n";
            return nullptr;
        }

        // Actualizar el valor de la variable
        builder->CreateStore(exprValue, symbolInfo.llvmValue);

        llvm::errs() << "Debug: Asignación completada para variable: " << varName << "\n";
        return nullptr;
    }

    std::any visitExprStmt(EasyRustParser::ExprStmtContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitExprStmt\n";
        return visitChildren(ctx);
    }

    std::any visitIfStmt(EasyRustParser::IfStmtContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitIfStmt\n";

        // Generar la condición
        Value *condValue = std::any_cast<Value *>(visit(ctx->condition()));
        if (!condValue)
        {
            llvm::errs() << "Error: Condición no válida en if\n";
            return nullptr;
        }

        // Crear los bloques básicos para "then", "else" y "merge"
        Function *currentFunction = builder->GetInsertBlock()->getParent();

        BasicBlock *thenBlock = BasicBlock::Create(context, "then", currentFunction);
        BasicBlock *elseBlock = nullptr;
        BasicBlock *mergeBlock = BasicBlock::Create(context, "merge", currentFunction);

        if (ctx->statement(1)) // Si existe un bloque "else"
        {
            elseBlock = BasicBlock::Create(context, "else", currentFunction);
        }

        // Instrucción de salto condicional
        if (elseBlock)
        {
            builder->CreateCondBr(condValue, thenBlock, elseBlock);
        }
        else
        {
            builder->CreateCondBr(condValue, thenBlock, mergeBlock);
        }

        // Emitir código para el bloque "then"
        builder->SetInsertPoint(thenBlock);
        visit(ctx->statement(0)); // Visitar las declaraciones del bloque "then"
        builder->CreateBr(mergeBlock);

        // Emitir código para el bloque "else" (si existe)
        if (elseBlock)
        {
            builder->SetInsertPoint(elseBlock);
            visit(ctx->statement(1)); // Visitar las declaraciones del bloque "else"
            builder->CreateBr(mergeBlock);
        }

        // Continuar en el bloque "merge"
        builder->SetInsertPoint(mergeBlock);

        return nullptr;
    }

    std::any visitMulDiv(EasyRustParser::MulDivContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitMulDiv\n";
        // Visitar las expresiones izquierda y derecha
        llvm::Value *left = std::any_cast<llvm::Value *>(visit(ctx->expr(0)));
        llvm::Value *right = std::any_cast<llvm::Value *>(visit(ctx->expr(1)));

        // Obtener el operador
        std::string op = ctx->op->getText();

        // Determinar el tipo de los operandos
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
        {
            if (op == "*")
            {
                llvm::errs() << "Debug: Realizando Mul (Entero)\n";
                return builder->CreateMul(left, right, "multmp");
            }
            else if (op == "/")
            {
                llvm::errs() << "Debug: Realizando SDiv (Entero)\n";
                return builder->CreateSDiv(left, right, "divtmp");
            }
        }
        else if (left->getType()->isDoubleTy() && right->getType()->isDoubleTy())
        {
            if (op == "*")
            {
                llvm::errs() << "Debug: Realizando FMul (Flotante)\n";
                return builder->CreateFMul(left, right, "fmultmp");
            }
            else if (op == "/")
            {
                llvm::errs() << "Debug: Realizando FDiv (Flotante)\n";
                return builder->CreateFDiv(left, right, "fdivtmp");
            }
        }
        else
        {
            llvm::errs() << "Error: Tipos incompatibles para MulDiv\n";
            return nullptr;
        }

        llvm::errs() << "Error: Operador no soportado en MulDiv: " << op << "\n";
        return nullptr;
    }
    llvm::Value *concatenateStrings(llvm::Value *left, llvm::Value *right)
    {
        llvm::errs() << "Debug: Entrando a concatenateStrings\n";

        // Asume que `left` y `right` son punteros a cadenas válidos
        llvm::Value *leftLen = builder->CreateCall(
            module->getOrInsertFunction("strlen", llvm::FunctionType::get(
                                                      llvm::Type::getInt64Ty(context),
                                                      {llvm::PointerType::getUnqual(context)},
                                                      false)),
            {left},
            "strlen_left");

        llvm::Value *rightLen = builder->CreateCall(
            module->getOrInsertFunction("strlen", llvm::FunctionType::get(
                                                      llvm::Type::getInt64Ty(context),
                                                      {llvm::PointerType::getUnqual(context)},
                                                      false)),
            {right},
            "strlen_right");

        // Calcular la longitud total (strlen(left) + strlen(right) + 1 para el terminador)
        llvm::Value *totalLen = builder->CreateAdd(
            leftLen,
            builder->CreateAdd(rightLen, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)),
            "total_len");

        // Allocar espacio para la cadena concatenada
        llvm::Value *resultBuffer = builder->CreateAlloca(
            llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 1024),
            nullptr,
            "result_buffer");

        // Crear llamada a `sprintf` para concatenar las cadenas
        llvm::Value *formatStr = builder->CreateGlobalString("%s%s", "fmt");
        builder->CreateCall(
            module->getOrInsertFunction(
                "sprintf",
                llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(context),
                    {llvm::PointerType::getUnqual(context), llvm::PointerType::getUnqual(context),
                     llvm::PointerType::getUnqual(context), llvm::PointerType::getUnqual(context)},
                    true)),
            {resultBuffer, formatStr, left, right});

        // Retorna el puntero al resultado
        return resultBuffer;
    }

    std::any visitAddSub(EasyRustParser::AddSubContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitAddSub\n";
        // Visitar las expresiones izquierda y derecha
        llvm::Value *left = std::any_cast<llvm::Value *>(visit(ctx->expr(0)));
        llvm::Value *right = std::any_cast<llvm::Value *>(visit(ctx->expr(1)));

        // Obtener el operador
        std::string op = ctx->op->getText();

        // Determinar el tipo de los operandos
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy())
        {
            if (op == "+")
            {
                llvm::errs() << "Debug: Realizando Add (Entero)\n";
                return builder->CreateAdd(left, right, "addtmp");
            }
            else if (op == "-")
            {
                llvm::errs() << "Debug: Realizando Sub (Entero)\n";
                return builder->CreateSub(left, right, "subtmp");
            }
        }
        else if (left->getType()->isDoubleTy() && right->getType()->isDoubleTy())
        {
            if (op == "+")
            {
                llvm::errs() << "Debug: Realizando FAdd (Flotante)\n";
                return builder->CreateFAdd(left, right, "faddtmp");
            }
            else if (op == "-")
            {
                llvm::errs() << "Debug: Realizando FSub (Flotante)\n";
                return builder->CreateFSub(left, right, "fsubtmp");
            }
        }
        else if (left->getType()->isPointerTy() && right->getType()->isPointerTy())
        {
            llvm::PointerType *leftPtrType = llvm::dyn_cast<llvm::PointerType>(left->getType());
            llvm::PointerType *rightPtrType = llvm::dyn_cast<llvm::PointerType>(right->getType());

            if (leftPtrType && rightPtrType && op == "+")
            {
                llvm::errs() << "Debug: Realizando concatenación de cadenas\n";
                return concatenateStrings(left, right);
            }
        }

        llvm::errs() << "Error: Operador no soportado en AddSub: " << op << "\n";
        return nullptr;
    }

    std::any visitParens(EasyRustParser::ParensContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitParens\n";
        return visit(ctx->expr());
    }

    std::any visitString(EasyRustParser::StringContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitString\n";
        // Obtén el texto del literal de cadena (sin las comillas)
        std::string stringValue = ctx->getText();
        if (stringValue.front() == '"' && stringValue.back() == '"')
        {
            stringValue = stringValue.substr(1, stringValue.size() - 2); // Elimina las comillas
        }

        llvm::errs() << "Debug: Literal de cadena procesado: " << stringValue << "\n";

        // Crea un GlobalStringPtr para el literal
        Value *stringPtr = builder->CreateGlobalString(stringValue, "string_literal");

        // Retorna el puntero a la cadena
        return stringPtr;
    }

    std::any visitIdentifier(EasyRustParser::IdentifierContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitIdentifier\n";
        std::string varName = ctx->IDENTIFIER()->getText();
        if (symbolTable.find(varName) == symbolTable.end())
        {
            // Si la variable no está definida, muestra un error
            auto token = ctx->getStart();
            errs() << "Error: Variable no definida: " << varName
                   << " en línea " << token->getLine()
                   << ", columna " << token->getCharPositionInLine() << "\n";
            return nullptr;
        }

        // Obtén la información de la tabla de símbolos
        auto &symbolInfo = symbolTable[varName];
        Type *varType = symbolInfo.type;
        const std::string &logicalType = symbolInfo.logicalType;
        llvm::errs() << "Debug: Variable '" << varName
                     << "' tiene logicalType: " << logicalType << "\n";

        // Carga el valor de la variable desde la memoria
        Value *value = builder->CreateLoad(varType, symbolInfo.llvmValue, varName.c_str());

        // Manejo adicional para booleanos y cadenas
        if (logicalType == "bool")
        {
            // Convertir a booleano si es necesario (i1 ya está representado como booleano en LLVM)
            llvm::errs() << "Debug: Variable es un booleano\n";
        }
        else if (logicalType == "string")
        {
            // Manejar cadenas (char pointers)
            llvm::errs() << "Debug: Variable es una cadena\n";
            // Retorna directamente el puntero a la cadena
            return value;
        }
        else if (logicalType == "int")
        {
            llvm::errs() << "Debug: Variable es un entero\n";
        }
        else if (logicalType == "float")
        {
            llvm::errs() << "Debug: Variable es un flotante\n";
        }
        else
        {
            llvm::errs() << "Error: Tipo no soportado para la variable '" << varName << "'\n";
            return nullptr;
        }

        // Retornar el valor para tipos no string
        return value;
    }

    std::any visitNumber(EasyRustParser::NumberContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitNumber\n";
        std::string numText = ctx->NUMBER()->getText();

        // Determina si el número es entero o flotante
        if (numText.find('.') != std::string::npos)
        {
            // Número con punto decimal => double
            llvm::errs() << "Debug: Procesando número flotante: " << numText << "\n";
            auto numVal = std::stod(numText);
            llvm::Value *val = llvm::ConstantFP::get(context, llvm::APFloat(numVal));
            return std::any(val);
        }
        else
        {
            // Número entero => int
            llvm::errs() << "Debug: Procesando número entero: " << numText << "\n";
            auto numVal = std::stoi(numText);
            llvm::Value *val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), numVal);
            return std::any(val);
        }
    }

    std::any visitBoolean(EasyRustParser::BooleanContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitBoolean\n";
        return visitChildren(ctx);
    }

    std::any visitCallFunction(EasyRustParser::CallFunctionContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitCallFunction\n";

        std::string funcName = ctx->functionCall()->IDENTIFIER()->getText();
        llvm::Function *function = module->getFunction(funcName);

        if (!function)
        {
            llvm::errs() << "Error: Función no definida: " << funcName << "\n";
            return nullptr;
        }

        // Procesar argumentos
        std::vector<llvm::Value *> args;
        if (ctx->functionCall()->arguments())
        {
            for (auto &argCtx : ctx->functionCall()->arguments()->expr())
            {
                llvm::Value *argValue = std::any_cast<llvm::Value *>(visit(argCtx));
                args.push_back(argValue);
            }
        }

        llvm::errs() << "Debug: Creando llamada a función " << funcName << "\n";

        if (function->getReturnType()->isVoidTy())
        {
            llvm::errs() << "Debug: Retornando void\n";
            builder->CreateCall(function, args);
            return nullptr;
        }
        else
        {
            llvm::errs() << "Debug: Retornando otra cosa\n";
            llvm::Value *callValue = builder->CreateCall(function, args, "calltmp");

            if (!callValue)
            {
                llvm::errs() << "Error: CreateCall retornó nullptr para la función: " << funcName << "\n";
                return nullptr;
            }

            llvm::errs() << "Debug: Llamada a función creada exitosamente\n";
            return callValue;
        }
    }

    std::any visitFunctionCall(EasyRustParser::FunctionCallContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitFunctionCall\n";
        return visitChildren(ctx);
    }

    std::any visitArguments(EasyRustParser::ArgumentsContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitArguments\n";
        return visitChildren(ctx);
    }

    std::any visitCondition(EasyRustParser::ConditionContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitCondition\n";

        // Visitar las expresiones izquierda y derecha
        Value *lhs = std::any_cast<Value *>(visit(ctx->expr(0)));
        Value *rhs = std::any_cast<Value *>(visit(ctx->expr(1)));

        if (!lhs || !rhs)
        {
            llvm::errs() << "Error: Operandos inválidos en la condición\n";
            return nullptr;
        }

        // Obtener el operador de comparación
        std::string opText = ctx->comparisonOp()->getText();
        llvm::errs() << "Debug: Operador de comparación detectado: " << opText << "\n";

        // Generar la instrucción LLVM correspondiente
        if (lhs->getType()->isIntegerTy())
        {
            llvm::CmpInst::Predicate pred;
            if (opText == "==")
                pred = llvm::CmpInst::ICMP_EQ;
            else if (opText == "!=")
                pred = llvm::CmpInst::ICMP_NE;
            else if (opText == "<")
                pred = llvm::CmpInst::ICMP_SLT;
            else if (opText == ">")
                pred = llvm::CmpInst::ICMP_SGT;
            else if (opText == "<=")
                pred = llvm::CmpInst::ICMP_SLE;
            else if (opText == ">=")
                pred = llvm::CmpInst::ICMP_SGE;
            else
            {
                llvm::errs() << "Error: Operador de comparación desconocido: " << opText << "\n";
                return nullptr;
            }
            return builder->CreateICmp(pred, lhs, rhs, "cmp");
        }
        else if (lhs->getType()->isFloatingPointTy())
        {
            llvm::CmpInst::Predicate pred;
            if (opText == "==")
                pred = llvm::CmpInst::FCMP_OEQ;
            else if (opText == "!=")
                pred = llvm::CmpInst::FCMP_ONE;
            else if (opText == "<")
                pred = llvm::CmpInst::FCMP_OLT;
            else if (opText == ">")
                pred = llvm::CmpInst::FCMP_OGT;
            else if (opText == "<=")
                pred = llvm::CmpInst::FCMP_OLE;
            else if (opText == ">=")
                pred = llvm::CmpInst::FCMP_OGE;
            else
            {
                llvm::errs() << "Error: Operador de comparación desconocido: " << opText << "\n";
                return nullptr;
            }
            return builder->CreateFCmp(pred, lhs, rhs, "cmp");
        }

        llvm::errs() << "Error: Tipo no soportado para comparación\n";
        return nullptr;
    }

    std::any visitComparisonOp(EasyRustParser::ComparisonOpContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitComparisonOp\n";

        // Devuelve el texto del operador de comparación
        std::string opText = ctx->getText();

        llvm::errs() << "Debug: Operador de comparación: " << opText << "\n";

        // Retorna el operador como texto para que otros visitantes puedan usarlo
        return opText;
    }

    std::any visitType(EasyRustParser::TypeContext *ctx) override
    {
        llvm::errs() << "Debug: Entrando a visitType\n";
        return visitChildren(ctx);
    }
};