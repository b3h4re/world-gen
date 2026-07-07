#pragma once

#include <filesystem>
#include <string>
#include <string_view>


namespace wgen::files {

    class Path {
    public:
        Path() = default;
        Path(const char* path);
        Path(const std::string& path);
        Path(std::filesystem::path path);

        const std::filesystem::path& get() const { return path_; }
        std::string string() const { return path_.string(); }

        operator const std::filesystem::path&() const { return path_; }

        static Path homeConfigPath(std::string_view appName, std::string_view fileName);
        static std::filesystem::path expandUser(std::filesystem::path path);
        static std::filesystem::path normalize(std::filesystem::path path);

        static std::filesystem::path homeDirectory();
        static std::filesystem::path tmpDirectory();

    private:
        std::filesystem::path path_;
    };

}
