#pragma once
#include <vector>
#include <string>
#include <deque>
#include "document.h"
#include "search_server.h"
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> res = search_server_.FindTopDocuments(raw_query, document_predicate);
        time_incr_and_check(res);
        return res;
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        bool is_empty;
    };
    void time_incr_and_check(std::vector<Document> resp);
    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;
    const static int sec_in_day_ = 1440;
    int time_;
    int empty_req_num_;
};

