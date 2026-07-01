#include "interpreter.hpp"

#include <algorithm>
#include <fstream>
#include <utility>


namespace wgen {

    std::optional<std::pair<std::string, std::string>> Interpreter::splitNamedArgument(const std::string& token) {
        const auto separator = token.find('=');
        if (separator == std::string::npos) {
            return std::nullopt;
        }

        if (separator == 0 || separator + 1 >= token.size()) {
            throw std::runtime_error("Named argument expects name=value: " + token);
        }

        return std::pair{
            token.substr(0, separator),
            token.substr(separator + 1)
        };
    }

    std::vector<std::string> Interpreter::constructorArgumentOrder(const std::string& typeName) {
        if (typeName == "float" || typeName == "int" || typeName == "bool") {
            return {"value"};
        }

        if (typeName == "heightmap") {
            return {"width", "height", "default"};
        }

        if (typeName == "worley") {
            return {"width", "height", "dots", "seed", "p", "num_points"};
        }

        if (typeName == "wavelet") {
            return {"width", "height", "seed", "frequency"};
        }

        if (typeName == "value_noise") {
            return {"seed"};
        }

        if (typeName == "layered_sin") {
            return {"seed"};
        }

        if (typeName == "simplex") {
            return {"width", "height", "dots", "seed"};
        }

        if (typeName == "perlin") {
            return {"width", "height", "dots", "seed"};
        }

        return {};
    }

    std::string Interpreter::normalizeArgumentName(const std::string& typeName, std::string name) {
        if (typeName == "heightmap" && name == "value") {
            return "default";
        }

        if (typeName == "worley" && (name == "dots_per_cell" || name == "dotsPerCell")) {
            return "dots";
        }

        return name;
    }


    Interpreter::Interpreter() : currentLine{0} {
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
        constructors["heightmap"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() != 2 && args.size() != 3) {
                throw std::runtime_error("heightmap expects width, height, and optional default value");
            }

            const auto width = static_cast<std::size_t>(as<int>(args[0]));
            const auto height = static_cast<std::size_t>(as<int>(args[1]));

            if (args.size() == 2) {
                return HeightMap<float>{width, height};
            }

            return HeightMap<float>{width, height, as<float>(args[2])};
        };

        // Constructors for generators
        constructors["worley"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) {
                throw std::runtime_error("Worley must have at least three arguments: width, height and dots per cell");
            }

            const auto width = static_cast<std::size_t>(as<int>(args[0]));
            const auto height = static_cast<std::size_t>(as<int>(args[1]));
            const auto dots = static_cast<std::size_t>(as<int>(args[2]));

            if (args.size() == 3) {
                return std::make_unique<wgen::WorleyNoise2d>(width, height, dots);
            }

            const auto seed = static_cast<std::uint32_t>(as<int>(args[3]));

            if (args.size() == 4) {
                return std::make_unique<wgen::WorleyNoise2d>(width, height, dots, seed);
            }

            const auto p = static_cast<float>(as<float>(args[4]));

            if (args.size() == 5) {
                return std::make_unique<wgen::WorleyNoise2d>(width, height, dots, seed, p);
            }

            throw std::runtime_error("Worley expects width, height, dots, optional seed, and optional p");
        };

        constructors["wavelet"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                throw std::runtime_error("Wavelet must have at least three arguments: width, height");
            }

            const auto width = static_cast<std::size_t>(as<int>(args[0]));
            const auto height = static_cast<std::size_t>(as<int>(args[1]));

            if (args.size() == 2) {
                return std::make_unique<wgen::WaveletNoise2d>(width, height);
            }

            const auto seed = static_cast<std::uint32_t>(as<int>(args[3]));

            if (args.size() == 3) {
                return std::make_unique<wgen::WaveletNoise2d>(width, height, seed);
            }

            const auto frequency = static_cast<float>(as<float>(args[4]));

            if (args.size() == 4) {
                return std::make_unique<wgen::WaveletNoise2d>(
                    width,
                    height,
                    seed,
                    wgen::defaultReconstructionKernel,
                    frequency
                );
            }

            throw std::runtime_error("Wavelet expects width, height, optional seed, and optional frequency");
        };

        constructors["value_noise"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() == 0) {
                return std::make_unique<wgen::ValueNoiseGenerator>();
            }

            const auto seed = static_cast<std::uint32_t>(as<int>(args[0]));

            if (args.size() == 1) {
                return std::make_unique<wgen::ValueNoiseGenerator>(seed);
            }
            throw std::runtime_error("Value noise expects only optional seed argument");
        };
        constructors["layered_sin"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() == 0) {
                return std::make_unique<wgen::LayeredSinNoiseGenerator>();
            }

            const auto seed = static_cast<std::uint32_t>(as<int>(args[0]));

            if (args.size() == 1) {
                return std::make_unique<wgen::LayeredSinNoiseGenerator>(seed);
            }
            throw std::runtime_error("Layered sin noise expects only optional seed argument");
        };

        constructors["simplex"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) {
                throw std::runtime_error("Simplex must have at least three arguments: width, height and dots per cell");
            }

            const auto width = static_cast<std::size_t>(as<int>(args[0]));
            const auto height = static_cast<std::size_t>(as<int>(args[1]));
            const auto dots = static_cast<std::size_t>(as<int>(args[2]));

            if (args.size() == 3) {
                return std::make_unique<wgen::SimplexNoise2d>(width, height, dots);
            }

            const auto seed = static_cast<std::uint32_t>(as<int>(args[3]));

            if (args.size() == 4) {
                return std::make_unique<wgen::SimplexNoise2d>(width, height, dots, seed);
            }

            throw std::runtime_error("Worley expects width, height, dots, optional seed");
        };

        constructors["perlin"] = [](const std::vector<Value>& args) -> Value {
            if (args.size() < 3) {
                throw std::runtime_error("Perlin must have at least three arguments: width, height and dots per cell");
            }

            const auto width = static_cast<std::size_t>(as<int>(args[0]));
            const auto height = static_cast<std::size_t>(as<int>(args[1]));
            const auto dots = static_cast<std::size_t>(as<int>(args[2]));

            if (args.size() == 3) {
                return std::make_unique<wgen::PerlinNoise2d>(width, height, dots);
            }

            const auto seed = static_cast<std::uint32_t>(as<int>(args[3]));

            if (args.size() == 4) {
                return std::make_unique<wgen::PerlinNoise2d>(width, height, dots, seed);
            }

            throw std::runtime_error("Perlin expects width, height, dots, optional seed");
        };
    }
    void Interpreter::registerMutatingOperators() {

    }
    void Interpreter::registerBinaryOperators() {
        binaryOperators["float + float"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                return std::get<float>(lhs) + std::get<float>(rhs);
            };

        binaryOperators["heightmap + float"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                return std::get<HeightMap<float>>(lhs) + std::get<float>(rhs);
            };

        binaryOperators["float + heightmap"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                return std::get<float>(lhs) + std::get<HeightMap<float>>(rhs);
            };

        binaryOperators["heightmap + heightmap"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                return std::get<HeightMap<float>>(lhs) + std::get<HeightMap<float>>(rhs);
            };

        binaryOperators["heightmap + generator"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                const auto h = std::get<HeightMap<float>>(lhs);
                return h + std::get<std::unique_ptr<wgen::Generator>>(rhs)->generateHeightMap(h.width(), h.height());
            };

        binaryOperators["int + int"] =
            [](const Value& lhs, const Value& rhs) -> Value {
                return std::get<int>(lhs) + std::get<int>(rhs);
            };
    }


    void Interpreter::clear() {
        currentLine = 0;
        variables.clear();
        program.clear();
    }

    void Interpreter::loadScript(const std::filesystem::path &scriptPath) {
        clear();
        parseScriptFile(scriptPath);
    }


    void Interpreter::loadScript(const std::vector<std::string>& script) {
        clear();

        for (std::size_t i = 0; i < script.size(); ++i) {
            auto tokens = tokenizeLine(script[i]);

            if (!tokens.empty()) {
                program.push_back(parseCommand(tokens, i+1, script[i]));
            }
        }
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

        std::vector<std::string> lines{};

        program.clear();

        std::string line;

        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
        loadScript(lines);
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

        if (commandName == "addat") {
            // addat {x} {y} {var1} to {var2} = {var3}
            if (tokens.size() != 8 || tokens[4] != "to" || tokens[6] != "=") {
                throw std::runtime_error(
                    "Line " + std::to_string(lineNumber) +
                    ": addat expects: addat {x} {y} {var1} to {var2} = {var3}"
                );
            }

            AddAtCommand command;
            const auto x = tryParseInt(tokens[1]);
            const auto y = tryParseInt(tokens[2]);
            if (!x || !y || *x < 0 || *y < 0) {
                throw std::runtime_error(
                    "Line " + std::to_string(lineNumber) +
                    ": addat coordinates must be non-negative integers"
                );
            }

            command.x = static_cast<std::size_t>(*x);
            command.y = static_cast<std::size_t>(*y);
            command.lhs = tokens[3];
            command.rhs = tokens[5];
            command.result = tokens[7];

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
        while (currentLine < program.size()) {
            executeCurrentLine();
        }
    }

    void Interpreter::executeCurrentLine() {
        if (currentLine >= program.size()) {
            return;
        }
        const Command& cmd = program[currentLine];
        try {
            executeCommand(cmd);
            ++currentLine;
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "Runtime error on line " +
                std::to_string(cmd.lineNumber) +
                ":\n    " + cmd.sourceLine +
                "\n" + e.what()
            );
        }
    }

    typename std::vector<Command>::const_reference Interpreter::getCurrentCommand() const {
        if (currentLine >= program.size()) {
            return EMPTY_COMMAND;
        }
        return program.at(currentLine);
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

        if (std::holds_alternative<HeightMap<float>>(value)) {
            return "heightmap";
        }

        if (std::holds_alternative<std::unique_ptr<Generator>>(value)) {
            return "generator";
        }

        return "<unknown>";
    }

    Value Interpreter::copyLanguageValue(const Value& value) {
        return std::visit(
            Overloaded{
                [](const UndefinedVariable& undefined) -> Value {
                    return undefined;
                },
                [](float number) -> Value {
                    return number;
                },
                [](int number) -> Value {
                    return number;
                },
                [](bool boolean) -> Value {
                    return boolean;
                },
                [](const HeightMap<float>& heightMap) -> Value {
                    return heightMap;
                },
                [](const std::unique_ptr<Generator>&) -> Value {
                    throw std::runtime_error("Generator values are not copyable");
                }
            },
            value
        );
    }

	    Value Interpreter::resolveArgument(const std::string& token) {
	        if (auto integer = tryParseInt(token)) {
	            return *integer;
	        }

	        if (auto number = tryParseFloat(token)) {
	            return *number;
	        }

        auto it = variables.find(token);
        if (it != variables.end()) {
            return copyLanguageValue(it->second);
        }

        throw std::runtime_error("Unknown argument: " + token);
	    }

    std::vector<Value> Interpreter::resolveConstructorArguments(const DeclCommand& command) {
        const bool hasNamedArgument = std::any_of(
            command.argumentTokens.begin(),
            command.argumentTokens.end(),
            [](const std::string& token) {
                return token.find('=') != std::string::npos;
            }
        );

        if (!hasNamedArgument) {
            std::vector<Value> args;
            args.reserve(command.argumentTokens.size());
            for (const std::string& argToken : command.argumentTokens) {
                args.push_back(resolveArgument(argToken));
            }

            return args;
        }

        const auto order = constructorArgumentOrder(command.typeName);
        if (order.empty()) {
            throw std::runtime_error("Named constructor arguments are not supported for type: " + command.typeName);
        }

        std::vector<std::optional<Value>> orderedArgs(order.size());
        std::size_t positionalIndex = 0;

        for (const std::string& token : command.argumentTokens) {
            if (auto namedArg = splitNamedArgument(token)) {
                const std::string name = normalizeArgumentName(command.typeName, namedArg->first);
                const auto nameIt = std::find(order.begin(), order.end(), name);
                if (nameIt == order.end()) {
                    throw std::runtime_error(
                        "Unknown constructor argument '" + namedArg->first + "' for type: " + command.typeName
                    );
                }

                const auto index = static_cast<std::size_t>(std::distance(order.begin(), nameIt));
                if (orderedArgs[index]) {
                    throw std::runtime_error("Duplicate constructor argument: " + namedArg->first);
                }

                orderedArgs[index] = resolveArgument(namedArg->second);
                continue;
            }

            while (positionalIndex < orderedArgs.size() && orderedArgs[positionalIndex]) {
                ++positionalIndex;
            }

            if (positionalIndex >= orderedArgs.size()) {
                throw std::runtime_error("Too many constructor arguments for type: " + command.typeName);
            }

            orderedArgs[positionalIndex] = resolveArgument(token);
            ++positionalIndex;
        }

        std::size_t lastArgument = 0;
        bool foundArgument = false;
        for (std::size_t i = 0; i < orderedArgs.size(); ++i) {
            if (orderedArgs[i]) {
                lastArgument = i;
                foundArgument = true;
            }
        }

        if (!foundArgument) {
            return {};
        }

        std::vector<Value> args;
        args.reserve(lastArgument + 1);
        for (std::size_t i = 0; i <= lastArgument; ++i) {
            if (!orderedArgs[i]) {
                throw std::runtime_error("Missing constructor argument: " + order[i]);
            }

            args.push_back(std::move(*orderedArgs[i]));
        }

        return args;
    }

	    std::optional<int> Interpreter::tryParseInt(const std::string& text) {
	        char* end = nullptr;
	        long value = std::strtol(text.c_str(), &end, 10);

	        if (end != text.c_str() && *end == '\0') {
	            return static_cast<int>(value);
	        }

	        return std::nullopt;
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
                [this](const AddAtCommand& cmd) {
                    executeAddAt(cmd);
                },
                [this](const CopyCommand& cmd) {
                    executeCopy(cmd);
                },
                [this](const EmptyCommand& cmd) {}
            },
            command.payload
        );
    }

    const Value& Interpreter::getVariableValue(const std::string& varName) {
        if (!variables.contains(varName)) {
            return Interpreter::UNDEFINDED;
        }
        return variables.at(varName);
    }

    void Interpreter::executeDeclaration(const DeclCommand& command) {
        auto constructorIt = constructors.find(command.typeName);

        if (constructorIt == constructors.end()) {
            throw std::runtime_error("Unknown type: " + command.typeName);
        }

        std::vector<Value> args = resolveConstructorArguments(command);

        variables[command.variableName] = constructorIt->second(args);
    }

    void Interpreter::executeCopy(const CopyCommand& command) {
        auto sourceIt = variables.find(command.source);

        if (sourceIt == variables.end()) {
            throw std::runtime_error("Unknown variable: " + command.source);
        }

        variables[command.destination] = copyLanguageValue(sourceIt->second);
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

    void Interpreter::executeAddAt(const AddAtCommand& command) {
        auto lhsIt = variables.find(command.lhs);
        if (lhsIt == variables.end()) {
            throw std::runtime_error("Unknown variable: " + command.lhs);
        }

        auto rhsIt = variables.find(command.rhs);
        if (rhsIt == variables.end()) {
            throw std::runtime_error("Unknown variable: " + command.rhs);
        }

        const auto* lhs = std::get_if<HeightMap<float>>(&lhsIt->second);
        if (lhs == nullptr) {
            throw std::runtime_error("addat lhs must be heightmap, got: " + typeNameOf(lhsIt->second));
        }

        const auto* rhs = std::get_if<HeightMap<float>>(&rhsIt->second);
        if (rhs == nullptr) {
            throw std::runtime_error("addat rhs must be heightmap, got: " + typeNameOf(rhsIt->second));
        }

        HeightMap<float> result = *lhs;
        result.add_at(*rhs, command.x, command.y);
        variables[command.result] = std::move(result);
    }



}
