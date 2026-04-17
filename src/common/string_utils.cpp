#include "common/string_utils.h"

#include <algorithm>
#include <cctype>

namespace kv {
namespace common {

std::string Trim(std::string_view input) {
  std::size_t begin = 0;

  while (begin < input.size() &&
         std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
    ++begin;
  }

  std::size_t end = input.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }

  return std::string(input.substr(begin, end - begin));
}

std::vector<std::string_view> SplitWhitespaceView(std::string_view input) {
  std::vector<std::string_view> tokens;
  std::size_t index = 0;

  while (index < input.size()) {
    while (index < input.size() &&
           std::isspace(static_cast<unsigned char>(input[index])) != 0) {
      ++index;
    }

    const std::size_t begin = index;

    while (index < input.size() &&
           std::isspace(static_cast<unsigned char>(input[index])) == 0) {
      ++index;
    }

    if (begin < index) {
      tokens.emplace_back(input.substr(begin, index - begin));
    }
  }

  return tokens;
}

std::string ToUpper(std::string_view input) {
  std::string upper(input);
  std::transform(upper.begin(), upper.end(), upper.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::toupper(character));
                 });
  return upper;
}

}  // namespace common
}  // namespace kv
