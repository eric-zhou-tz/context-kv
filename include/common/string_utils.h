#ifndef KV_STORE_COMMON_STRING_UTILS_H_
#define KV_STORE_COMMON_STRING_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

namespace kv {
namespace common {

/**
 * @brief Removes leading and trailing whitespace from a string view.
 *
 * @param input Text to trim.
 * @return Trimmed string as an owning `std::string`.
 */
std::string Trim(std::string_view input);

/**
 * @brief Splits text into whitespace-delimited token views.
 *
 * @param input Text to split.
 * @return Views into the original input for each token.
 */
std::vector<std::string_view> SplitWhitespaceView(std::string_view input);

/**
 * @brief Splits text into whitespace-delimited owning strings.
 *
 * @param input Text to split.
 * @return Vector containing copied token strings.
 */
std::vector<std::string> SplitWhitespace(std::string_view input);

/**
 * @brief Converts text to uppercase.
 *
 * @param input Text to convert.
 * @return Uppercase copy of the input.
 */
std::string ToUpper(std::string_view input);

}  // namespace common
}  // namespace kv

#endif  // KV_STORE_COMMON_STRING_UTILS_H_
