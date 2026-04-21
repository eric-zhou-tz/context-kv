#include "helpers/file_utils.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>

namespace kv {
namespace tests {

bool FileExists(const std::string& path) {
  return std::filesystem::exists(path);
}

std::uintmax_t FileSize(const std::string& path) {
  return std::filesystem::file_size(path);
}

void RemoveIfExists(const std::string& path) {
  std::error_code error;
  std::filesystem::remove(path, error);
}

void WriteBinaryFile(const std::string& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    throw std::runtime_error("failed to open test file for write: " + path);
  }
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!output) {
    throw std::runtime_error("failed to write test file: " + path);
  }
}

void AppendBinaryFile(const std::string& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::app);
  if (!output.is_open()) {
    throw std::runtime_error("failed to open test file for append: " + path);
  }
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!output) {
    throw std::runtime_error("failed to append test file: " + path);
  }
}

std::string ReadBinaryFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open test file for read: " + path);
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

}  // namespace tests
}  // namespace kv
