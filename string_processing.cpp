#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

//STRING_VIEW
std::vector<std::string> SplitIntoWords(std::string_view text) {
    std::vector<std::string> result;
    if (text.empty()) {
        return result;
    }
    auto ch = [](const char c) {return c != ' '; };
    auto sp = [](const char c) {return c == ' '; };
    auto word_begin = std::find_if(text.begin(), text.end(), ch);
    auto word_end = word_begin;
    while (word_begin != text.end()) {
        word_end = std::find_if(word_begin, text.end(), sp);
        result.emplace_back(std::string(word_begin, word_end));
        word_begin = std::find_if(word_end, text.end(), ch);
    }
    return result;
}
