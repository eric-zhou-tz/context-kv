#include "store/kv_store.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace {

using kv::store::KVStore;

TEST(KVStoreCoreTest, PutGetDeleteBasicCorrectness) {
  KVStore store;

  store.Set("alpha", "1");

  ASSERT_TRUE(store.Contains("alpha"));
  ASSERT_TRUE(store.Get("alpha").has_value());
  EXPECT_EQ("1", store.Get("alpha").value());
  EXPECT_EQ(1U, store.Size());

  EXPECT_TRUE(store.Delete("alpha"));
  EXPECT_FALSE(store.Contains("alpha"));
  EXPECT_FALSE(store.Get("alpha").has_value());
  EXPECT_EQ(0U, store.Size());
}

TEST(KVStoreCoreTest, OverwritesExistingKey) {
  KVStore store;

  store.Set("language", "c");
  store.Set("language", "cpp");

  EXPECT_EQ(1U, store.Size());
  ASSERT_TRUE(store.Get("language").has_value());
  EXPECT_EQ("cpp", store.Get("language").value());
}

TEST(KVStoreCoreTest, DeleteMissingKeyReturnsFalseAndLeavesStoreUnchanged) {
  KVStore store;
  store.Set("keep", "value");

  EXPECT_FALSE(store.Delete("missing"));

  EXPECT_EQ(1U, store.Size());
  EXPECT_EQ("value", store.Get("keep").value());
}

TEST(KVStoreCoreTest, GetMissingKeyReturnsNullopt) {
  KVStore store;

  EXPECT_FALSE(store.Get("missing").has_value());
  EXPECT_FALSE(store.Contains("missing"));
}

TEST(KVStoreCoreTest, EmptyKeyAndEmptyValueAreSupported) {
  KVStore store;

  store.Set("", "empty-key");
  store.Set("empty-value", "");

  ASSERT_TRUE(store.Get("").has_value());
  EXPECT_EQ("empty-key", store.Get("").value());
  ASSERT_TRUE(store.Get("empty-value").has_value());
  EXPECT_EQ("", store.Get("empty-value").value());
}

TEST(KVStoreCoreTest, RepeatedOverwriteKeepsOnlyLatestValue) {
  KVStore store;

  for (int i = 0; i < 100; ++i) {
    store.Set("hot", "value-" + std::to_string(i));
  }

  EXPECT_EQ(1U, store.Size());
  EXPECT_EQ("value-99", store.Get("hot").value());
}

TEST(KVStoreCoreTest, ManyKeysCanBeInsertedAndRetrieved) {
  KVStore store;

  for (int i = 0; i < 1000; ++i) {
    store.Set("key-" + std::to_string(i), "value-" + std::to_string(i));
  }

  EXPECT_EQ(1000U, store.Size());
  for (int i = 0; i < 1000; ++i) {
    const std::string key = "key-" + std::to_string(i);
    ASSERT_TRUE(store.Get(key).has_value());
    EXPECT_EQ("value-" + std::to_string(i), store.Get(key).value());
  }
}

TEST(KVStoreCoreTest, SupportsSpacesSpecialCharactersAndLargeValues) {
  KVStore store;
  const std::string key = "key with spaces / symbols !@#$%^&*()";
  const std::string large_value(256 * 1024, 'x');

  store.Set(key, large_value);

  ASSERT_TRUE(store.Get(key).has_value());
  EXPECT_EQ(large_value, store.Get(key).value());
}

TEST(KVStoreCoreTest, ClearRemovesAllInMemoryEntries) {
  KVStore store;
  store.Set("a", "1");
  store.Set("b", "2");

  store.Clear();

  EXPECT_EQ(0U, store.Size());
  EXPECT_FALSE(store.Contains("a"));
  EXPECT_FALSE(store.Contains("b"));
}

}  // namespace
