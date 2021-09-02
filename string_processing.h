#pragma once

#include <vector>
#include <string>
#include <set>
#include <execution>
#include <algorithm>

// String Processing -( ͡° ͜ʖ ͡° )-
//Declarations
std::vector<std::string> SplitIntoWords(const std::string& text);

//STRING_VIEW
std::vector<std::string> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings);

//Implementations
template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;
    for (const std::string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}


