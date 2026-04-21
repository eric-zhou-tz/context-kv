#include "helpers/temp_dir.h"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace kv {
namespace tests {

TempDir::TempDir() {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path base = std::filesystem::temp_directory_path();

  for (int attempt = 0; attempt < 100; ++attempt) {
    const std::filesystem::path candidate =
        base / ("kv_store_tests_" + std::to_string(stamp) + "_" +
                std::to_string(attempt));
    std::error_code error;
    if (std::filesystem::create_directory(candidate, error)) {
      path_ = candidate.string();
      return;
    }
  }

  throw std::runtime_error("failed to create temporary test directory");
}

TempDir::~TempDir() {
  std::error_code error;
  std::filesystem::remove_all(path_, error);
}

const std::string& TempDir::path() const {
  return path_;
}

std::string TempDir::FilePath(const std::string& filename) const {
  return (std::filesystem::path(path_) / filename).string();
}

}  // namespace tests
}  // namespace kv
