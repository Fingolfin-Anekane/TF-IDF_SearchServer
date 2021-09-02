#pragma once

#include <vector>
#include <deque>
#include "search_server.h"

//Request queue using std::deque
//Declaration
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
            : search_server_(search_server)
    {}

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        //Maybe more fields in future.
        //Left the structure instead of a bool variable because of a code chunk in task
        bool is_empty_query;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int no_result_request_count_ = 0;
};

//Implementation
template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (requests_.size() == min_in_day_) {
        if (requests_.front().is_empty_query)
            --no_result_request_count_;
        requests_.pop_front();
    }
    if (result.empty())
        ++no_result_request_count_;
    requests_.push_back({result.empty()});
    return result;
}