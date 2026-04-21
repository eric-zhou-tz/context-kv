#ifndef KV_STORE_TESTS_HELPERS_FILE_UTILS_H_
#define KV_STORE_TESTS_HELPERS_FILE_UTILS_H_

#include <cstdint>
#include <string>

namespace kv {
namespace tests {

bool FileExists(const std::string& path);
std::uintmax_t FileSize(const std::string& path);
void RemoveIfExists(const std::string& path);
void WriteBinaryFile(const std::string& path, const std::string& bytes);
void AppendBinaryFile(const std::string& path, const std::string& bytes);
std::string ReadBinaryFile(const std::string& path);

template <typename T>
void AppendPrimitive(std::string& bytes, T value) {
  const char* raw = reinterpret_cast<const char*>(&value);
  bytes.append(raw, sizeof(T));
}

}  // namespace tests
}  // namespace kv

#endif  // KV_STORE_TESTS_HELPERS_FILE_UTILS_H_
