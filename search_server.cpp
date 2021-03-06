#include "search_server.h"
using namespace std;
SearchServer::SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
}
SearchServer::SearchServer(const string_view stop_words_text)
        : SearchServer(SplitIntoWordsView(stop_words_text)) {
}
void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("id for adding doc isn't correct"s);
    }
    vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        auto it = vocab_.insert(word);
        word_to_document_freqs_[*it.first][document_id] += inv_word_count;
        doc_to_word_freq[document_id][*it.first] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.emplace(document_id);
}
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query,
                                          [status](int document_id, DocumentStatus document_status, int rating) {
                                              return document_status == status;});
}
vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, const string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query,status);
}
vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, const string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(execution::par, raw_query,
                                          [status](int document_id, DocumentStatus document_status, int rating) {
                                              return document_status == status;});
}
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, const string_view raw_query) const {
    return SearchServer::FindTopDocuments(raw_query);
}
vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, const string_view raw_query) const {
    return SearchServer::FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}
int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    //LOG_DURATION_STREAM("Operation time", std::cout);
    Query query = ParseQuery(raw_query);
    vector<string_view> matched_words;
    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    return tuple{matched_words, documents_.at(document_id).status};
}
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&,
                                                                       const string_view raw_query,
                                                                       int document_id) const {
    return MatchDocument(raw_query,document_id);
}
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&,
                                                                       const string_view raw_query,
                                                                       int document_id) const {
    return MatchDocument(raw_query,document_id);
}
bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}
bool SearchServer::IsValidWord(const string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
vector<string> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string> words;
    for (const string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("there's spec symbs in words"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(string(word));
        }
    }
    return words;
}
int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}
SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("after minus there're no words"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("after minus there're no words"s);
    }

    return QueryWord{text, is_minus, IsStopWord(text)};
}
SearchServer::Query SearchServer::ParseQuery(const string_view text) const {
    Query query;
    for (const string_view word : SplitIntoWordsView(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}
double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}
std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}
// map's count is logariphmic in size
// 'at' access is log too
// in the end : log(n)
const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string_view, double> res;
    if (documents_.count(document_id)!=0){
        return doc_to_word_freq.at(document_id);
    }
    else {
        return res;
    }
}
// count for map log(n)
// at for doc_to_word_freq log(n)
// loop for word in doc for delete - W
// on each iteration erase and at's access both log(n) => log(n)
// in the end : W * log(n), where W - num of word in deleted docs,
//                                n - num of docs on server
void SearchServer::RemoveDocument(int document_id){
    if (doc_to_word_freq.count(document_id) != 0){
        for (const auto& [word, word_freq]: doc_to_word_freq.at(document_id)){
            word_to_document_freqs_.at(word).erase(document_id);
        }
        doc_to_word_freq.erase(document_id);
        documents_.erase(document_id);
        document_ids_.erase(document_id);
    }
}
void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    RemoveDocument(document_id);
}
void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    if (doc_to_word_freq.count(document_id) != 0){
        vector<string_view> words_;
        for (const auto& [word,seq] : doc_to_word_freq.at(document_id)){
            words_.push_back(word);
        }
        // no conflict in parallel - there're no the same word in words_
        for_each(execution::par,
                 words_.begin(),
                 words_.end(),
                 [&](const auto& w_){
                     word_to_document_freqs_.at(w_).erase(document_id);
                 });
        doc_to_word_freq.erase(document_id);
        documents_.erase(document_id);
        document_ids_.erase(document_id);
    }
}