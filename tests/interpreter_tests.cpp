#include "scripts/interpreter.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

class TestInterpreter : public wgen::Interpreter {
public:
    void setVariable(std::string name, wgen::Value value) {
        variables[std::move(name)] = value;
    }
};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error{std::string{message}};
    }
}

template<typename Exception, typename Function>
void requireThrows(Function function, std::string_view message) {
    bool threwExpected = false;
    try {
        function();
    } catch (const Exception &) {
        threwExpected = true;
    }

    require(threwExpected, message);
}

void expectNear(float actual, float expected, float epsilon, std::string_view message) {
    if (std::abs(actual - expected) > epsilon) {
        throw std::runtime_error{std::string{message}};
    }
}

template<typename T>
void expectValue(const wgen::Value &value, const T &expected, std::string_view message) {
    const auto *actual = std::get_if<T>(&value);
    require(actual != nullptr, message);
    require(*actual == expected, message);
}

void expectFloatValue(const wgen::Value &value, float expected, std::string_view message) {
    const auto *actual = std::get_if<float>(&value);
    require(actual != nullptr, message);
    expectNear(*actual, expected, 0.00001F, message);
}

std::filesystem::path makeScript(std::string_view name, std::string_view contents) {
    const auto path = std::filesystem::temp_directory_path() / ("world_gen_interpreter_" + std::string{name});
    std::ofstream file{path};
    if (!file) {
        throw std::runtime_error{"failed to create temporary script file"};
    }

    file << contents;
    return path;
}

void testLoadAndStepThroughScript() {
    const auto scriptPath = makeScript(
        "step.txt",
        "// ignored comment\n"
        "\n"
        "decl float height 1.25 // trailing comment\n"
        "copy height to copied\n"
    );

    wgen::Interpreter interpreter;
    interpreter.loadScript(scriptPath);

    const auto &firstCommand = interpreter.getCurrentCommand();
    require(firstCommand.lineNumber == 3, "first parsed command should keep original source line number");
    require(std::holds_alternative<wgen::DeclCommand>(firstCommand.payload), "first command should be declaration");

    interpreter.executeCurrentLine();
    expectFloatValue(interpreter.getVariableValue("height"), 1.25F, "float declaration produced wrong value");

    const auto &secondCommand = interpreter.getCurrentCommand();
    require(secondCommand.lineNumber == 4, "second parsed command should keep original source line number");
    require(std::holds_alternative<wgen::CopyCommand>(secondCommand.payload), "second command should be copy");

    interpreter.executeCurrentLine();
    expectFloatValue(interpreter.getVariableValue("copied"), 1.25F, "copy command produced wrong value");

    const auto &emptyCommand = interpreter.getCurrentCommand();
    require(emptyCommand.lineNumber == 0, "current command after script end should be empty command");
    require(std::holds_alternative<wgen::EmptyCommand>(emptyCommand.payload), "current command after script end should be empty");

    interpreter.executeCurrentLine();
    expectFloatValue(interpreter.getVariableValue("copied"), 1.25F, "executing past script end should be a no-op");
}

void testExecuteWholeFloatScript() {
    const auto scriptPath = makeScript(
        "float_program.txt",
        "decl float left 1.5\n"
        "decl float right 2.25\n"
        "add left to right = sum\n"
        "copy sum to copied\n"
    );

    wgen::Interpreter interpreter;
    interpreter.loadScript(scriptPath);
    interpreter.executeScript();

    expectFloatValue(interpreter.getVariableValue("left"), 1.5F, "left float has wrong value");
    expectFloatValue(interpreter.getVariableValue("right"), 2.25F, "right float has wrong value");
    expectFloatValue(interpreter.getVariableValue("sum"), 3.75F, "float addition has wrong value");
    expectFloatValue(interpreter.getVariableValue("copied"), 3.75F, "copied sum has wrong value");
}

void testSeededIntAndBoolValues() {
    const auto scriptPath = makeScript(
        "seeded_values.txt",
        "copy sourceFlag to copiedFlag\n"
        "decl int first sourceFirst\n"
        "decl int second sourceSecond\n"
        "add first to second = sum\n"
    );

    TestInterpreter interpreter;
    interpreter.loadScript(scriptPath);
    interpreter.setVariable("sourceFlag", true);
    interpreter.setVariable("sourceFirst", 4);
    interpreter.setVariable("sourceSecond", 9);
    interpreter.executeScript();

    expectValue(interpreter.getVariableValue("copiedFlag"), true, "bool copy has wrong value");
    expectValue(interpreter.getVariableValue("first"), 4, "int declaration from variable has wrong value");
    expectValue(interpreter.getVariableValue("second"), 9, "int declaration from variable has wrong value");
    expectValue(interpreter.getVariableValue("sum"), 13, "int addition has wrong value");
}

void testClearResetsProgramAndVariables() {
    const auto scriptPath = makeScript("clear.txt", "decl float value 3.0\n");

    wgen::Interpreter interpreter;
    interpreter.loadScript(scriptPath);
    interpreter.executeScript();
    expectFloatValue(interpreter.getVariableValue("value"), 3.0F, "test setup declaration failed");

    interpreter.clear();

    require(
        std::holds_alternative<wgen::EmptyCommand>(interpreter.getCurrentCommand().payload),
        "clear should remove loaded program");
}

void testLoadErrors() {
    wgen::Interpreter interpreter;

    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(std::filesystem::temp_directory_path() / "world_gen_missing_script.txt"); },
        "missing script path should throw");
    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(std::filesystem::temp_directory_path()); },
        "directory script path should throw");
    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(makeScript("unknown_command.txt", "multiply a by b\n")); },
        "unknown command should throw during load");
    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(makeScript("bad_decl.txt", "decl float\n")); },
        "malformed decl should throw during load");
    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(makeScript("bad_add.txt", "add a b = c\n")); },
        "malformed add should throw during load");
    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(makeScript("bad_copy.txt", "copy a b\n")); },
        "malformed copy should throw during load");
}

void testRuntimeErrors() {
    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript("unknown_type.txt", "decl vector value 1.0\n"));
            interpreter.executeScript();
        },
        "unknown declared type should throw during execution");

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript("unknown_arg.txt", "decl float value missing\n"));
            interpreter.executeScript();
        },
        "unknown declaration argument should throw during execution");

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript("wrong_arg_count.txt", "decl float value\n"));
            interpreter.executeScript();
        },
        "wrong constructor argument count should throw during execution");

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript("int_literal_type_error.txt", "decl int value 1\n"));
            interpreter.executeScript();
        },
        "int declaration from numeric literal currently resolves to float and should throw");

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript("bad_copy_source.txt", "copy missing to destination\n"));
            interpreter.executeScript();
        },
        "copy from unknown variable should throw during execution");

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript("bad_add_left.txt", "decl float value 1.0\nadd missing to value = sum\n"));
            interpreter.executeScript();
        },
        "add with unknown lhs should throw during execution");

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript("bad_add_right.txt", "decl float value 1.0\nadd value to missing = sum\n"));
            interpreter.executeScript();
        },
        "add with unknown rhs should throw during execution");

    requireThrows<std::runtime_error>(
        [] {
            const auto scriptPath = makeScript(
                "unsupported_add.txt",
                "decl float floatValue 1.0\n"
                "add floatValue to intValue = sum\n"
            );

            TestInterpreter interpreter;
            interpreter.loadScript(scriptPath);
            interpreter.setVariable("intValue", 2);
            interpreter.executeScript();
        },
        "mixed float and int addition should throw during execution");
}

using TestFunction = void (*)();

void runTest(std::string_view name, TestFunction test) {
    try {
        test();
    } catch (const std::exception &exception) {
        throw std::runtime_error{std::string{name} + ": " + exception.what()};
    }
}

} // namespace

int main() {
    try {
        runTest("load and step through script", testLoadAndStepThroughScript);
        runTest("execute whole float script", testExecuteWholeFloatScript);
        runTest("seeded int and bool values", testSeededIntAndBoolValues);
        runTest("clear resets program and variables", testClearResetsProgramAndVariables);
        runTest("load errors", testLoadErrors);
        runTest("runtime errors", testRuntimeErrors);
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
