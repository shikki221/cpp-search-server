#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id,
                   const std::string_view document,
                   DocumentStatus status,
                   const std::vector<int>& ratings) {
    if ((document_id < 0) ||
        (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id");
    }

    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for (const auto word : words) {
        auto it = words_.insert(std::string(word));
        std::string_view sv_word = *(it.first);
        word_to_document_freqs_[sv_word][document_id]
            += inv_word_count;
        documents_words_freqs_[document_id][sv_word]
            += inv_word_count;
    }

    documents_.emplace(document_id,
                       DocumentData
                       {ComputeAverageRating(ratings), status});

    document_ids_.emplace(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

// MatchDocument
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    if ((document_id < 0) || (documents_.count(document_id) == 0)) {
        throw std::out_of_range("document_id out of range"s);
    }

    const auto& word_freqs = documents_words_freqs_.at(document_id);
    const auto& query = ParseQuery(std::execution::seq, raw_query);

    for (const std::string_view word : query.minus_words) {
        if (word_freqs.count(word) > 0) {
            return { empty_vector, documents_.at(document_id).status };
        }
    }

    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.plus_words) {
        if (word_freqs.count(word) > 0) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

// MatchDocument sequenced_policy
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument( const std::execution::sequenced_policy& policy, const std::string_view raw_query, int document_id) const {
    if ((document_id < 0) || (documents_.count(document_id) == 0)) {
        throw std::out_of_range("document_id out of range"s);
    }

    const auto& word_freqs = documents_words_freqs_.at(document_id);
    const auto& query = ParseQuery(policy, raw_query);

    if (std::any_of(policy,
                    query.minus_words.begin(),
                    query.minus_words.end(),
                    [&word_freqs](const std::string_view word) {
                        return word_freqs.count(word) > 0;
                    })) {
        return { empty_vector, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    std::copy_if(policy,
                 query.plus_words.begin(),
                 query.plus_words.end(),
                 std::back_inserter(matched_words),
                 [&word_freqs](const std::string_view word) {
                     return word_freqs.count(word) > 0;
                 });

    return { matched_words, documents_.at(document_id).status };
}

// MatchDocument parallel_policy
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument( const std::execution::parallel_policy& policy, const std::string_view raw_query, int document_id) const {
    if ((document_id < 0) || (documents_.count(document_id) == 0)) {
        throw std::out_of_range("document_id out of range"s);
    }

    const auto& word_freqs = documents_words_freqs_.at(document_id);
    const auto& query = ParseQuery(policy, raw_query, false);

    if (std::any_of(//policy,
                    query.minus_words.begin(),
                    query.minus_words.end(),
                    [&word_freqs](const std::string_view word) {
                        return word_freqs.count(word) > 0;
                    })) {
        return { empty_vector, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    std::copy_if(//policy,
                 query.plus_words.begin(),
                 query.plus_words.end(),
                 std::back_inserter(matched_words),
                 [&word_freqs](const std::string_view word) {
                     return word_freqs.count(word) > 0;
                 });

    std::sort(policy,
              matched_words.begin(),
              matched_words.end());
    auto it = std::unique(policy,
                          matched_words.begin(),
                          matched_words.end());
    matched_words.erase(it, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

// RemoveDocument
void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + std::string(text) + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

const std::map<string_view, double> &SearchServer::GetWordFrequencies(int document_id) const {
    if(documents_words_freqs_.find(document_id) == documents_words_freqs_.end()) {
        return empty_map;
    }
    return documents_words_freqs_.at(document_id);
}


    