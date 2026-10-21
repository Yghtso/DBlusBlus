#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace dblusblus::test {

// A fresh directory under the system temp path, removed with its contents on destruction.
// Valid() is false when the directory could not be created.
class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::error_code error;
        const auto base = std::filesystem::temp_directory_path(error);
        if (error) {
            return;
        }
        std::string path_template = (base / "dblusblus-test-XXXXXX").string();
        if (const char* created = ::mkdtemp(path_template.data()); created != nullptr) {
            path_ = created;
        }
    }

    ~TemporaryDirectory() {
        if (!path_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] bool Valid() const noexcept {
        return !path_.empty();
    }

    [[nodiscard]] std::filesystem::path File(std::string_view name) const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

} // namespace dblusblus::test
