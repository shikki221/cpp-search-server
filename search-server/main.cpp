#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

template <typename T>
void AssertImpl(const T& t, const string& t_str, const string& file, const string& func, unsigned line, const string& hint) {
    if (!t) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << t_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(a) AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, ""s)

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}
 
struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }    
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, 
            DocumentData{
                ComputeAverageRating(ratings), 
                status
            });
    }
    
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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
 
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { document_id += rating ;
                return document_status == status;
            });
    }
 
    vector<Document> FindTopDocuments(const string& raw_query) const {            
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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
    
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
    
    template <typename LambdaType>
    vector<Document> FindAllDocuments(const Query& query, LambdaType lambda_function) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (lambda_function(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
                
            }
        }
        
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestDocumentsAdding() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dog near red town"s);
        ASSERT(found_docs.empty());
    }
}


void TestStopWordsSupporting() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const string stop_words = "in the"s;
    const vector<int> ratings = {1, 2, 3};
    
    {
        SearchServer server;
        server.SetStopWords(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in the town"s).empty());
    }
    
    {
        SearchServer server;
        server.SetStopWords(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in the city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

}

void TestMinusWordsSupporting() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in the -city"s).empty());
    }
    
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in the city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

}


void TestMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [v, ds] = server.MatchDocument("dog in the big city"s, doc_id);
        ASSERT(v.size() == 3);
    }
    
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [v, ds] = server.MatchDocument("dog in the big -city"s, doc_id);
        ASSERT(v.empty());
    }
}

void TestRelevanceSorting() {

    {
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 43;
        const string content_2 = "fox fox the city city city"s;
        const vector<int> ratings_2 = {4, 5, 6};

        const int doc_id_3 = 44;
        const string content_3 = "dog dog dog in the city"s;
        const vector<int> ratings_3 = {2, 4, 2};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s);
        ASSERT(found_docs.size() == 3);
        ASSERT(found_docs[0].id == doc_id_3);
        ASSERT(found_docs[1].id == doc_id_2);
        ASSERT(found_docs[2].id == doc_id_1);
    }
    
    {
        const int doc_id_1 = 52;
        const string content_1 = "cat fox dog in the city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 53;
        const string content_2 = "fox dog the city city city"s;
        const vector<int> ratings_2 = {4, 5, 6};

        const int doc_id_3 = 54;
        const string content_3 = "dog in the city city city"s;
        const vector<int> ratings_3 = {2, 4, 2};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s);
        ASSERT(found_docs.size() == 3);
        ASSERT(found_docs[0].id == doc_id_1);
        ASSERT(found_docs[1].id == doc_id_2);
        ASSERT(found_docs[2].id == doc_id_3);
    }

    
}

void TestRating() {

    {
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s);
        ASSERT(!found_docs.empty());
        ASSERT(found_docs[0].rating == round((1+2+3+0.0)/3));
    }
}

void TestLambdaFilter() {

    const int doc_id = 42;
    const string content = "cat in the city city city"s;
    const vector<int> ratings = {1, 2, 3};
    const DocumentStatus document_status = DocumentStatus::ACTUAL;
    
    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s, DocumentStatus::ACTUAL);
        ASSERT(!found_docs.empty());
        ASSERT(found_docs[0].id == doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs.empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s, [] (int document_id, DocumentStatus status, int rating) {
            document_id +=1;
            if (status == DocumentStatus::ACTUAL) {document_id +=1;};
            return rating > 1;
        });
        ASSERT(!found_docs.empty());
        ASSERT(found_docs[0].id == doc_id);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s, [] (int document_id, DocumentStatus status, int rating) {
                     document_id +=1;
                     if (status == DocumentStatus::ACTUAL) {document_id +=1;};
                     return rating > 3;
        });
        ASSERT(found_docs.empty());
    }

}

void TestStatus() {

    const int doc_id = 42;
    const string content = "cat in the city city city"s;
    const vector<int> ratings = {1, 2, 3};
    const DocumentStatus document_status = DocumentStatus::BANNED;
    
    {
        SearchServer server;
        server.AddDocument(doc_id, content, document_status, ratings);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs.empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s, DocumentStatus::BANNED);
        ASSERT(!found_docs.empty());
        ASSERT(found_docs[0].id == doc_id);
    }


}

void TestRelevanceAccuracy() {
    
    {
        const int doc_id_1 = 42;
        const string content_1 = "cat in the city city city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 43;
        const string content_2 = "fox fox the city city city"s;
        const vector<int> ratings_2 = {4, 5, 6};

        const int doc_id_3 = 44;
        const string content_3 = "dog dog dog in the city"s;
        const vector<int> ratings_3 = {2, 4, 2};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s);
        ASSERT(found_docs.size() == 3);
        ASSERT(found_docs[0].id == doc_id_3);
        ASSERT(abs(found_docs[0].relevance - log(3)/2) < 1e-6);
        ASSERT(found_docs[1].id == doc_id_2);
        ASSERT(abs(found_docs[1].relevance - log(3)/3) < 1e-6);
        ASSERT(found_docs[2].id == doc_id_1);
        ASSERT(abs(found_docs[2].relevance - log(3)/6) < 1e-6);
    }
    
    {
        const int doc_id_1 = 52;
        const string content_1 = "cat fox dog in the city"s;
        const vector<int> ratings_1 = {1, 2, 3};

        const int doc_id_2 = 53;
        const string content_2 = "fox dog the city city city"s;
        const vector<int> ratings_2 = {4, 5, 6};

        const int doc_id_3 = 54;
        const string content_3 = "dog in the city city city"s;
        const vector<int> ratings_3 = {2, 4, 2};

        SearchServer server;
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);
        const auto found_docs = server.FindTopDocuments("cat fox dog"s);
        ASSERT(found_docs.size() == 3);
        ASSERT(found_docs[0].id == doc_id_1);
        ASSERT(abs(found_docs[0].relevance - 0.25068) < 1e-6);
        ASSERT(found_docs[1].id == doc_id_2);
        ASSERT(abs(found_docs[1].relevance - 0.0675775) < 1e-6);
        ASSERT(found_docs[2].id == doc_id_3);
        ASSERT(abs(found_docs[2].relevance - 0) < 1e-6);
    }
}

void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestDocumentsAdding();
    TestStopWordsSupporting();
    TestMinusWordsSupporting();
    TestMatching();
    TestRelevanceSorting();
    TestRating();
    TestLambdaFilter();
    TestStatus();
    TestRelevanceAccuracy();
}

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}
