#include "store/kv_store.h"

namespace kv {
namespace store {

void KVStore::Set(const std::string& key, const std::string& value) {
  data_[key] = value;
}

std::optional<std::string> KVStore::Get(const std::string& key) const {
  const auto it = data_.find(key);
  if (it == data_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool KVStore::Delete(const std::string& key) {
  return data_.erase(key) > 0;
}

bool KVStore::Contains(const std::string& key) const {
  return data_.find(key) != data_.end();
}

std::size_t KVStore::Size() const {
  return data_.size();
}

void KVStore::Clear() {
  data_.clear();
}

}  // namespace store
}  // namespace kv
