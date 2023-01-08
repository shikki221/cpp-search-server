#pragma once

#include <vector>
#include <string>
#include <set>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(StringContainer& container) {
    std::set<std::string, std::less<>> non_empty_strings;
    if constexpr(std::is_convertible_v<StringContainer, std::string_view>) {
        for (auto const& word : SplitIntoWords(container)) {
            non_empty_strings.insert(std::string(word));
        }
    }
    else {
        for (auto const& word : container) {
            non_empty_strings.insert(std::string(word));
        }
    }
    return non_empty_strings;
}
