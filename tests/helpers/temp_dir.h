#ifndef KV_STORE_TESTS_HELPERS_TEMP_DIR_H_
#define KV_STORE_TESTS_HELPERS_TEMP_DIR_H_

#include <string>

namespace kv {
namespace tests {

class TempDir {
 public:
  TempDir();
  ~TempDir();

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::string& path() const;
  std::string FilePath(const std::string& filename) const;

 private:
  std::string path_;
};

}  // namespace tests
}  // namespace kv

#endif  // KV_STORE_TESTS_HELPERS_TEMP_DIR_H_
