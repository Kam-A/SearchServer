#include "remove_duplicates.h"
using namespace std;
void RemoveDuplicates(SearchServer& search_server){
    set<vector<string>> words_per_docs;
    vector<int> doc_to_remove;
    for (const int doc_id : search_server){
        vector<string> curr_doc_voc;
        //log(n)
        map<string,double> word_to_freq = search_server.GetWordFrequencies(doc_id);
        // W
        for (const auto& word: word_to_freq){
            curr_doc_voc.push_back(word.first);
        }
        // get size - const
        // push_back - amort/const
        // emplace - log(W)
        int set_size = static_cast<int>(words_per_docs.size());
        words_per_docs.emplace(curr_doc_voc);
        if ( set_size == static_cast<int>(words_per_docs.size()))
            doc_to_remove.push_back(doc_id);
    }
    for (const auto& doc_id :doc_to_remove){
        cout << "Found duplicate document id "s << doc_id << endl;
        search_server.RemoveDocument(doc_id);
    }
}
