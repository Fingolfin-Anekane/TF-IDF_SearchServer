#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <execution>
#include <string_view>


#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using std::string_literals::operator""s;

//Search server logic
//Declaration
const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    // STRING_VIEW
    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    // PARALLEL
    template<typename ExecutingPolicy>
    std::vector<Document> FindTopDocuments(const ExecutingPolicy& policy, std::string_view raw_query) const;

    template<typename ExecutingPolicy>
    std::vector<Document> FindTopDocuments(const ExecutingPolicy& policy, std::string_view raw_query, DocumentStatus status) const;

    template <typename DocumentPredicate, typename ExecutingPolicy>
    std::vector<Document> FindTopDocuments(const ExecutingPolicy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    template<typename ExecutingPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutingPolicy&& policy, std::string_view raw_query, int document_id) const;

    std::set<int>::iterator begin() const;

    std::set<int>::iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template<typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);


private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string, double>> documents_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy, const Query& query, DocumentPredicate document_predicate) const;
};

//Implementation
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template<typename ExecutingPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutingPolicy& policy, std::string_view raw_query) const
{
    if constexpr (std::is_same_v<ExecutingPolicy, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }
    return FindTopDocuments(std::execution::par, raw_query, DocumentStatus::ACTUAL);
}

template<typename ExecutingPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutingPolicy& policy, std::string_view raw_query, DocumentStatus status) const
{
    if constexpr (std::is_same_v<ExecutingPolicy, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
    }
    return FindTopDocuments(std::execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

template<typename DocumentPredicate, typename ExecutingPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutingPolicy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const
{
    if constexpr (std::is_same_v<ExecutingPolicy, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query, document_predicate);
    }

    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const auto& word_v : query.plus_words) {
        std::string word(word_v.begin(), word_v.end());
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const auto& word_v : query.minus_words) {
        std::string word(word_v.begin(), word_v.end());
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }
    return matched_documents;
}


// Parallel FindAllDocuments
template<typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy, const Query& query, DocumentPredicate document_predicate) const
{
    if constexpr (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {
        return FindAllDocuments(query, document_predicate);
    }

    size_t thread_count = 10;
    if (query.plus_words.size() < thread_count) thread_count = query.plus_words.size();
    ConcurrentMap<int, double> document_to_relevance(thread_count);

    auto begin = query.plus_words.begin(), end = query.plus_words.begin();

    // Run "thread_count" async lambdas on PLUS words
    {
        std::vector<std::future<void>> futures_plus(thread_count);
        while (begin != query.plus_words.end()) {
            std::advance(end, query.plus_words.size() / thread_count);
            // lambda
            futures_plus.push_back(std::async([begin, end, &document_to_relevance, this, document_predicate] {
                for (auto it = begin; it != end; ++it) {
                    std::string word((*it).begin(), (*it).end());
                    if (!this->word_to_document_freqs_.count(word)) continue;
                    const double inverse_document_freq = this->ComputeWordInverseDocumentFreq(word);
                    for (const auto [document_id, term_freq] : this->word_to_document_freqs_.at(word)) {
                        const auto& document_data = this->documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }
                }
                }));
            begin = end;
        }
    }

    begin = query.minus_words.begin();
    end = query.minus_words.begin();
    std::vector<std::future<void>> futures_minus(thread_count);
    while (begin != query.minus_words.end()) {
        std::advance(end, query.minus_words.size() / thread_count);
        futures_minus.push_back(std::async([begin, end, &document_to_relevance, this] {
            for (auto it = begin; it != end; ++it) {
                std::string word((*it).begin(), (*it).end());
                if (this->word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                for (const auto [document_id, _] : this->word_to_document_freqs_.at(word)) {
                    document_to_relevance.Erase(document_id);
                }
            }
            }));
        begin = end;
    }
    std::vector<Document> matched_documents;
    std::map<int, double> document_to_relevance_whole = document_to_relevance.BuildOrdinaryMap();

    for (const auto [document_id, relevance] : document_to_relevance_whole) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }
    return matched_documents;
}


template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    if (!documents_to_word_freqs_.count(document_id)) return;
    for (const auto& [word, _] : documents_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    documents_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
    documents_.erase(document_id);
}


template<typename ExecutingPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutingPolicy&& policy, std::string_view raw_query, int document_id) const {
    if (!documents_.count(document_id)) throw std::out_of_range("");
    static const auto query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words(query.plus_words.size());
    int matched_counter = 0;
    std::copy_if(policy,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [this, document_id, &matched_counter](const std::string& word) {
            if (!this->word_to_document_freqs_.count(word)) return false;
            if (this->word_to_document_freqs_.at(word).count(document_id)) {
                ++matched_counter;
                return true;
            }
            return false;
        });
    matched_words.resize(matched_counter);
    for (const auto& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return { matched_words, documents_.at(document_id).status };
}
