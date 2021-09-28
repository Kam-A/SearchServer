//
// Created by user on 27.09.2021.
//

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> res(queries.size());
    std::transform(std::execution::par,
                   queries.begin(), queries.end(),
                   res.begin(),
                   [&](const std::string& el){
                       return search_server.FindTopDocuments(el);
                   });
    return res;
}

std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    auto res = ProcessQueries(search_server,queries);
    std::vector<Document> res_of_res;
    return std::reduce(std::execution::par, res.begin(),res.end(),res_of_res,
                [&](auto a, auto b) {
                    a.insert(a.end(),b.begin(),b.end());
                    return a;});
}