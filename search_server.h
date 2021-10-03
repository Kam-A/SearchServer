#pragma once
#include <string>
#include "string_processing.h"
#include <vector>
#include "document.h"
#include <set>
#include <map>
#include <algorithm>
#include <cmath>
#include <utility>
#include "log_duration.h"
#include <execution>
#include <iostream>
#include "concurrent_map.h"
const int MAX_RESULT_DOCUMENT_COUNT = 5;
class SearchServer {
public:
    // Defines an invalid document id
    // You can refer this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
            : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    }

    // Invoke delegating constructor from string container
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);
    //void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
        //LOG_DURATION_STREAM("Operation time", std::cout);
        Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);
        sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentPredicate document_predicate) const {
        return FindTopDocuments(raw_query,document_predicate);
    }

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;
    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string,std::less<>> stop_words_;
    std::set<std::string,std::less<>> vocab_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int,std::map<std::string_view, double>> doc_to_word_freq;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(const std::string_view word) const;
    static bool IsValidWord(const std::string_view word);
    // now here can pass as string as string_view
    template <typename StringContainer>
    static std::set<std::string,std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
        using namespace std;
        set<string,less<>> non_empty_strings;
        for (const string_view str: strings) {
            if (!IsValidWord(str))
                throw invalid_argument("Not valid symbols in stop-words"s);
            if (!str.empty()) {
                non_empty_strings.insert(string(str));
            }
        }
        return non_empty_strings;
    }
    std::vector<std::string> SplitIntoWordsNoStop(const std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    QueryWord ParseQueryWord(std::string_view text) const;
    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };
    Query ParseQuery(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        for (const std::string_view word : query.plus_words) {
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

        for (const std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int,double> document_to_relevance(10);
        std::for_each(
                std::execution::par,
                query.plus_words.begin(),
                query.plus_words.end(),
                [&](const std::string_view word) {
                    if (word_to_document_freqs_.count(word) != 0) {
                        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                            const auto& document_data = documents_.at(document_id);
                            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                                document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                            }
                        }
                    }
                });
        std::for_each(
                std::execution::par,
                query.minus_words.begin(),
                query.minus_words.end(),
                [&](const std::string_view word) {
                    if (word_to_document_freqs_.count(word) != 0) {
                        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                            document_to_relevance.erase(document_id);
                        }
                    }
                });
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

