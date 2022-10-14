#pragma once
#include "search_server.h"
#include <deque>
#include <vector>
#include <string>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        uint64_t timestamp = 0;
        size_t results = 0;
    };
    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;
    int no_results_requests_ = 0;
    uint64_t current_time_ = 0;
    const static int sec_in_day_ = 1440;

    void AddRequest(size_t results_num);
};