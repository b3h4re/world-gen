#include "interpreter.hpp"


namespace wgen {

    Interpreter::Interpreter() {
        registerConstructors();
        registerMutatingOperators();
        registerBinaryOperators();
    }

    void Interpreter::registerConstructors() {
        constructors["float"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw std::runtime_error("float expects 1 argument");
            }

            return as<float>(args[0]);
        };
        constructors["int"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw std::runtime_error("int expects 1 argument");
            }

            return as<int>(args[0]);
        };
        constructors["bool"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw std::runtime_error("bool expects 1 argument");
            }

            return as<bool>(args[0]);
        };
    }
    void Interpreter::registerMutatingOperators() {

    }
    void Interpreter::registerBinaryOperators() {
        binaryOperators["float + float"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                return std::get<float>(lhs) + std::get<float>(rhs);
            };

        binaryOperators["int + int"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                return std::get<int>(lhs) + std::get<int>(rhs);
            };
    }


    void Interpreter::clear() {
        variables.clear();
        program.clear();
    }

    void Interpreter::loadScript(const std::filesystem::path &scriptPath) {
        clear();
        parseScriptFile(scriptPath);
    }

    void Interpreter::parseScriptFile(const std::filesystem::path& scriptPath) {
        files::Path path{scriptPath};
        if (!std::filesystem::exists(path.get())) {
            throw std::runtime_error("Script file not found at: " + path.string());
        }
        if (!std::filesystem::is_regular_file(path.get())) {
            throw std::runtime_error("Config path is not a regular file: " + path.string());
        }

        std::ifstream file{path.get()};

        if (!file) {
            throw std::runtime_error("Could not open file: " + path.string());
        }

        program.clear();

        std::string line;
        std::size_t lineNumber = 1;

        while (std::getline(file, line)) {
            auto tokens = tokenizeLine(line);

            if (!tokens.empty()) {
                program.push_back(parseCommand(tokens, lineNumber, line));
            }

            ++lineNumber;
        }
        file.close();
    }

    std::vector<std::string> Interpreter::tokenizeLine(const std::string& line) {
        std::string cleaned = line;

        if (auto commentPos = cleaned.find("//"); commentPos != std::string::npos) {
            cleaned = cleaned.substr(0, commentPos);
        }

        std::istringstream stream(cleaned);
        std::vector<std::string> tokens;

        std::string token;
        while (stream >> token) {
            tokens.push_back(token);
        }

        return tokens;
    }

    Command Interpreter::parseCommand(const std::vector<std::string>& tokens, std::size_t lineNumber, std::string sourceLine) {
        const std::string& commandName = tokens[0];

        if (commandName == "decl") {
            if (tokens.size() < 3) {
                throw std::runtime_error(
                    "Line " + std::to_string(lineNumber) +
                    ": decl expects: decl {type} {variable} {args...}"
                );
            }

            DeclCommand command;
            command.typeName = tokens[1];
            command.variableName = tokens[2];

            for (std::size_t i = 3; i < tokens.size(); ++i) {
                command.argumentTokens.push_back(tokens[i]);
            }

            return Command{
                .lineNumber = lineNumber,
                .sourceLine = std::move(sourceLine),
                .payload = std::move(command)
            };
        }

        if (commandName == "add") {
            // add {var1} to {var2} = {var3}
            if (tokens.size() != 6 || tokens[2] != "to" || tokens[4] != "=") {
                throw std::runtime_error(
                    "Line " + std::to_string(lineNumber) +
                    ": add expects: add {var1} to {var2} = {var3}"
                );
            }

            AddCommand command;
            command.lhs = tokens[1];
            command.rhs = tokens[3];
            command.result = tokens[5];

            return Command{
                .lineNumber = lineNumber,
                .sourceLine = std::move(sourceLine),
                .payload = std::move(command)
            };
        }

        if (commandName == "copy") {
            // copy {source} to {destination}
            if (tokens.size() != 4 || tokens[2] != "to") {
                throw std::runtime_error(
                    "Line " + std::to_string(lineNumber) +
                    ": copy expects: copy {source} to {destination}"
                );
            }

            CopyCommand command;
            command.source = tokens[1];
            command.destination = tokens[3];

            return Command{
                .lineNumber = lineNumber,
                .sourceLine = std::move(sourceLine),
                .payload = std::move(command)
            };
        }

        throw std::runtime_error(
            "Line " + std::to_string(lineNumber) +
            ": unknown command '" + commandName + "'"
        );
    }

    void Interpreter::executeScript() {
        for (const Command& command : program) {
            try {
                executeCommand(command);
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    "Runtime error on line " +
                    std::to_string(command.lineNumber) +
                    ":\n    " + command.sourceLine +
                    "\n" + e.what()
                );
            }
        }
    }

    std::string Interpreter::typeNameOf(const Value& value) {
        if (std::holds_alternative<float>(value)) {
            return "float";
        }

        if (std::holds_alternative<int>(value)) {
            return "int";
        }

        if (std::holds_alternative<bool>(value)) {
            return "bool";
        }



        return "<unknown>";
    }

    Value Interpreter::resolveArgument(const std::string& token) {
        if (auto number = tryParseFloat(token)) {
            return *number;
        }

        auto it = variables.find(token);
        if (it != variables.end()) {
            return it->second;
        }

        throw std::runtime_error("Unknown argument: " + token);
    }

    std::optional<float> Interpreter::tryParseFloat(const std::string& text) {
        char* end = nullptr;
        float value = std::strtof(text.c_str(), &end);

        if (end != text.c_str() && *end == '\0') {
            return value;
        }

        return std::nullopt;
    }

    void Interpreter::executeCommand(const Command& command) {
        std::visit(
            Overloaded{
                [this](const DeclCommand& cmd) {
                    executeDeclaration(cmd);
                },
                [this](const AddCommand& cmd) {
                    executeAdd(cmd);
                },
                [this](const CopyCommand& cmd) {
                    executeCopy(cmd);
                }
            },
            command.payload
        );
    }

    void Interpreter::executeDeclaration(const DeclCommand& command) {
        auto constructorIt = constructors.find(command.typeName);

        if (constructorIt == constructors.end()) {
            throw std::runtime_error("Unknown type: " + command.typeName);
        }

        std::vector<Value> args;

        for (const std::string& argToken : command.argumentTokens) {
            args.push_back(resolveArgument(argToken));
        }

        variables[command.variableName] = constructorIt->second(args);
    }

    void Interpreter::executeCopy(const CopyCommand& command) {
        auto sourceIt = variables.find(command.source);

        if (sourceIt == variables.end()) {
            throw std::runtime_error("Unknown variable: " + command.source);
        }

        variables[command.destination] = sourceIt->second;
    }

    void Interpreter::executeAdd(const AddCommand& command) {
        auto lhsIt = variables.find(command.lhs);
        if (lhsIt == variables.end()) {
            throw std::runtime_error("Unknown variable: " + command.lhs);
        }

        auto rhsIt = variables.find(command.rhs);
        if (rhsIt == variables.end()) {
            throw std::runtime_error("Unknown variable: " + command.rhs);
        }

        const Value& lhs = lhsIt->second;
        const Value& rhs = rhsIt->second;

        std::string key = typeNameOf(lhs) + " + " + typeNameOf(rhs);

        auto operatorIt = binaryOperators.find(key);
        if (operatorIt == binaryOperators.end()) {
            throw std::runtime_error("Unsupported addition: " + key);
        }

        variables[command.result] = operatorIt->second(lhs, rhs);
    }



}
