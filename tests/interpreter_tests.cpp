#include "scripts/interpreter.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

static_assert(!std::is_copy_constructible_v<wgen::Interpreter>);
static_assert(!std::is_copy_assignable_v<wgen::Interpreter>);
static_assert(std::is_move_constructible_v<wgen::Interpreter>);
static_assert(std::is_move_assignable_v<wgen::Interpreter>);
static_assert(!std::is_copy_constructible_v<wgen::Value>);
static_assert(!std::is_copy_assignable_v<wgen::Value>);
static_assert(std::is_move_constructible_v<wgen::Value>);
static_assert(std::is_move_assignable_v<wgen::Value>);

class TestInterpreter : public wgen::Interpreter {
public:
    void setVariable(std::string name, wgen::Value value) {
        variables[std::move(name)] = std::move(value);
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
	        "decl int first 4\n"
	        "decl int second 9\n"
	        "add first to second = sum\n"
	    );

	    TestInterpreter interpreter;
	    interpreter.loadScript(scriptPath);
	    interpreter.setVariable("sourceFlag", true);
	    interpreter.executeScript();

	    expectValue(interpreter.getVariableValue("copiedFlag"), true, "bool copy has wrong value");
	    expectValue(interpreter.getVariableValue("first"), 4, "int declaration from literal has wrong value");
	    expectValue(interpreter.getVariableValue("second"), 9, "int declaration from literal has wrong value");
	    expectValue(interpreter.getVariableValue("sum"), 13, "int addition has wrong value");
	}

void testHeightMapDeclarationArguments() {
    const auto scriptPath = makeScript(
        "heightmap_constructor_args.txt",
        "decl heightmap positional 3 2\n"
        "decl heightmap named width=4 height=5\n"
        "decl heightmap filled width=2 height=2 default=7.5\n"
        "decl heightmap mixed 6 height=7\n"
    );

    wgen::Interpreter interpreter;
    interpreter.loadScript(scriptPath);
    interpreter.executeScript();

    const auto& positional = wgen::as<wgen::HeightMap<float>>(interpreter.getVariableValue("positional"));
    require(positional.width() == 3, "positional heightmap width is wrong");
    require(positional.height() == 2, "positional heightmap height is wrong");

    const auto& named = wgen::as<wgen::HeightMap<float>>(interpreter.getVariableValue("named"));
    require(named.width() == 4, "named heightmap width is wrong");
    require(named.height() == 5, "named heightmap height is wrong");

    const auto& filled = wgen::as<wgen::HeightMap<float>>(interpreter.getVariableValue("filled"));
    require(filled.width() == 2, "default-filled heightmap width is wrong");
    require(filled.height() == 2, "default-filled heightmap height is wrong");
    expectNear(filled.at(0, 0), 7.5F, 0.00001F, "default-filled heightmap value is wrong");
    expectNear(filled.at(1, 1), 7.5F, 0.00001F, "default-filled heightmap value is wrong");

    const auto& mixed = wgen::as<wgen::HeightMap<float>>(interpreter.getVariableValue("mixed"));
    require(mixed.width() == 6, "mixed heightmap width is wrong");
    require(mixed.height() == 7, "mixed heightmap height is wrong");
}

void testAddAtCommand() {
    const auto scriptPath = makeScript(
        "addat.txt",
        "decl heightmap base width=4 height=3 default=1.0\n"
        "decl heightmap patch width=2 height=2 default=2.0\n"
        "addat 1 1 base to patch = result\n"
    );

    wgen::Interpreter interpreter;
    interpreter.loadScript(scriptPath);
    interpreter.executeScript();

    const auto& base = wgen::as<wgen::HeightMap<float>>(interpreter.getVariableValue("base"));
    for (std::size_t y = 0; y < base.height(); ++y) {
        for (std::size_t x = 0; x < base.width(); ++x) {
            expectNear(base.at(x, y), 1.0F, 0.00001F, "addat should not mutate the source heightmap");
        }
    }

    const auto& result = wgen::as<wgen::HeightMap<float>>(interpreter.getVariableValue("result"));
    require(result.width() == 4, "addat result width is wrong");
    require(result.height() == 3, "addat result height is wrong");

    const std::vector<float> expected{
        1.0F, 1.0F, 1.0F, 1.0F,
        1.0F, 3.0F, 3.0F, 1.0F,
        1.0F, 3.0F, 3.0F, 1.0F,
    };

    for (std::size_t y = 0; y < result.height(); ++y) {
        for (std::size_t x = 0; x < result.width(); ++x) {
            expectNear(
                result.at(x, y),
                expected[y * result.width() + x],
                0.00001F,
                "addat result value is wrong"
            );
        }
    }
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
        [&] { interpreter.loadScript(makeScript("bad_addat.txt", "addat 1 base to patch = result\n")); },
        "malformed addat should throw during load");
    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(makeScript("bad_addat_x.txt", "addat left 1 base to patch = result\n")); },
        "non-integer addat x coordinate should throw during load");
    requireThrows<std::runtime_error>(
        [&] { interpreter.loadScript(makeScript("bad_addat_y.txt", "addat 1 -1 base to patch = result\n")); },
        "negative addat y coordinate should throw during load");
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

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript(
                "copy_generator.txt",
                "decl worley generator width=3 height=3 dots=2\n"
                "copy generator to copied\n"
            ));
            interpreter.executeScript();
        },
        "copying a generator should throw during execution");

    requireThrows<std::runtime_error>(
        [] {
            wgen::Interpreter interpreter;
            interpreter.loadScript(makeScript(
                "bad_addat_type.txt",
                "decl float value 1.0\n"
                "decl heightmap patch 2 2\n"
                "addat 0 0 value to patch = result\n"
            ));
            interpreter.executeScript();
        },
        "addat with non-heightmap lhs should throw during execution");
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
        runTest("heightmap declaration arguments", testHeightMapDeclarationArguments);
        runTest("addat command", testAddAtCommand);
        runTest("clear resets program and variables", testClearResetsProgramAndVariables);
        runTest("load errors", testLoadErrors);
        runTest("runtime errors", testRuntimeErrors);
    } catch (const std::exception &exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
