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

std::list<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    auto vec_of_vecs = ProcessQueries(search_server,queries);
    std::list<Document> list_of_docs;
    for(const auto& vec_of_docs : vec_of_vecs){
        list_of_docs.insert(list_of_docs.end(),
                            std::make_move_iterator(vec_of_docs.begin()),
                            std::make_move_iterator(vec_of_docs.end()));
    }

    return list_of_docs;
}