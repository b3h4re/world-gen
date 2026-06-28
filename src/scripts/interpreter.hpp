#pragma once

#include <vector>
#include <string>
#include <variant>
#include <stdexcept>
#include <unordered_map>
#include <optional>
#include <functional>
#include <fstream>

#include "files/file_path.hpp"


namespace wgen {
    struct UndefinedVariable{};
    using Value = std::variant<
        UndefinedVariable,
        float,
        int,
        bool
    >;
    using Constructor = std::function<Value(const std::vector<Value>&)>;
    using MutatingOperator = std::function<void(Value&, const std::vector<Value>&)>;
    using BinaryOperator = std::function<Value(const Value&, const Value&)>;

    template <typename T>
    T& as(Value& value) {
        if (auto* ptr = std::get_if<T>(&value)) {
            return *ptr;
        }

        throw std::runtime_error("Type error");
    }

    template <typename T>
    const T& as(const Value& value) {
        if (const auto* ptr = std::get_if<T>(&value)) {
            return *ptr;
        }

        throw std::runtime_error("Type error");
    }

    template <typename... Ts>
    struct Overloaded : Ts... {
        using Ts::operator()...;
    };

    template <typename... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    struct EmptyCommand {};

    struct DeclCommand {
        std::string typeName;
        std::string variableName;
        std::vector<std::string> argumentTokens;
    };

    struct AddCommand {
        std::string lhs;
        std::string rhs;
        std::string result;
    };

    struct CopyCommand {
        std::string source;
        std::string destination;
    };

    using CommandPayload = std::variant<
        DeclCommand,
        AddCommand,
        CopyCommand,
        EmptyCommand
    >;

    struct Command {
        std::size_t lineNumber{};
        std::string sourceLine;
        CommandPayload payload;
    };

    constexpr Command EMPTY_COMMAND{0, "", EmptyCommand{}};


    /*
    Each line in the script starts with a command describing what to do
        decl - decalre a new variable
            decl {type} {variable} {constructor arguments...}

        add - perform addition between var1 and var2 and assign result to var3 (var3 = var1 + var2;)
            add {var1} to {var2} = {var3}

        copy - assigns var1 to var2 (var2 = var1;)
            copy {var1} to {var2}

    Available types: float, int, bool
    */
    class Interpreter {
        public:
            Interpreter();

            constexpr static UndefinedVariable UNDEFINDED{};

            void clear();
            void loadScript(const std::filesystem::path& scriptPath);
            void executeScript();
            void executeCurrentLine();
            typename std::vector<Command>::const_reference getCurrentCommand() const;

            const Value& getVariableValue(const std::string& varName);

        protected:
            std::size_t currentLine;
            std::unordered_map<std::string, Value> variables{};
            std::unordered_map<std::string, Constructor> constructors{};
            std::unordered_map<std::string, MutatingOperator> mutatingOperators{};
            std::unordered_map<std::string, BinaryOperator> binaryOperators{};
            std::vector<Command> program{};

            void parseScriptFile(const std::filesystem::path& scriptPath);
            void registerConstructors();
            void registerMutatingOperators();
            void registerBinaryOperators();


            std::vector<std::string> tokenizeLine(const std::string& line);

            Command parseCommand(const std::vector<std::string>& tokens, std::size_t lineNumber, std::string sourceLine);

            void executeCommand(const Command& command);

            std::optional<float> tryParseFloat(const std::string& text);
            Value resolveArgument(const std::string& token);

            void executeDeclaration(const DeclCommand& command);
            void executeAdd(const AddCommand& command);
            void executeCopy(const CopyCommand& command);

            std::string typeNameOf(const Value& value);
    };
}
