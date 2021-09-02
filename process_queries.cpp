#include "process_queries.h"
#include <algorithm>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result(queries.size());
    std::transform(
        std::execution::par,
        queries.begin(), queries.end(),
        result.begin(),
        [&search_server](const std::string& query) {
            return search_server.FindTopDocuments(query);
        }
    );
    return result;
}


std::forward_list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    const std::vector<std::vector<Document>> raw_data = ProcessQueries(search_server, queries);
    std::forward_list<Document> result;
    for (const auto& vec : raw_data) {
        for (const auto& doc : vec) {
            result.push_front(doc);
        }
    }
    result.reverse();
    return result;

}