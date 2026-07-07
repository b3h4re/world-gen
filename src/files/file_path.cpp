#include "file_path.hpp"

#include <cstdlib>


namespace wgen::files {


    std::filesystem::path Path::homeDirectory() {
        const char* home = std::getenv("HOME");

        if (home == nullptr || std::string_view{home}.empty()) {
            return {};
        }

        return home;
    }

    std::filesystem::path Path::tmpDirectory() {
        for (const char* envName : {"TMPDIR", "TEMP", "TMP"}) {
            const char* tmp = std::getenv(envName);
            if (tmp != nullptr && !std::string_view{tmp}.empty()) {
                return tmp;
            }
        }

        return "/tmp";
    }


    Path::Path(const char* path) : path_{normalize(path)} {}

    Path::Path(const std::string& path) : path_{normalize(path)} {}

    Path::Path(std::filesystem::path path) : path_{normalize(std::move(path))} {}

    Path Path::homeConfigPath(std::string_view appName, std::string_view fileName) {
        std::filesystem::path home = homeDirectory();

        if (home.empty()) {
            return Path{std::filesystem::path{".config"} / appName / fileName};
        }

        return Path{home / ".config" / appName / fileName};
    }

    std::filesystem::path Path::expandUser(std::filesystem::path path) {
        std::string text = path.string();

        if (text == "~" || text.starts_with("~/")) {
            std::filesystem::path home = homeDirectory();

            if (home.empty()) {
                return path;
            }

            if (text == "~") {
                return home;
            }

            return home / text.substr(2);
        }

        return path;
    }

    std::filesystem::path Path::normalize(std::filesystem::path path) {
        path = expandUser(std::move(path));

        if (path.empty() || path.is_absolute()) {
            return path;
        }

        return std::filesystem::absolute(path);
    }

}
