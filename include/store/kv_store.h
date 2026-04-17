#ifndef KV_STORE_STORE_KV_STORE_H_
#define KV_STORE_STORE_KV_STORE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace kv {
namespace store {

/**
 * @brief In-memory key-value store backed by an unordered map.
 */
class KVStore {
 public:
  /**
   * @brief Constructs an empty key-value store.
   */
  KVStore() = default;

  /**
   * @brief Inserts or updates a value for a key.
   *
   * @param key Key to insert or update.
   * @param value Value to associate with the key.
   */
  void Set(const std::string& key, const std::string& value);

  /**
   * @brief Retrieves the value stored for a key.
   *
   * @param key Key to look up.
   * @return Stored value when the key exists, otherwise `std::nullopt`.
   */
  std::optional<std::string> Get(const std::string& key) const;

  /**
   * @brief Removes a key from the store.
   *
   * @param key Key to remove.
   * @return `true` when an entry was removed, otherwise `false`.
   */
  bool Delete(const std::string& key);

  /**
   * @brief Checks whether a key exists in the store.
   *
   * @param key Key to search for.
   * @return `true` when the key exists, otherwise `false`.
   */
  bool Contains(const std::string& key) const;

  /**
   * @brief Returns the number of stored key-value pairs.
   *
   * @return Current entry count.
   */
  std::size_t Size() const;

  /**
   * @brief Removes all entries from the store.
   */
  void Clear();

 private:
  /** @brief Internal storage for key-value pairs. */
  std::unordered_map<std::string, std::string> data_;
};

}  // namespace store
}  // namespace kv

#endif  // KV_STORE_STORE_KV_STORE_H_
