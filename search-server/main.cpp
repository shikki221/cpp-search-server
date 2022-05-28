#include <algorithm> 
#include <cassert> 
#include <cmath> 
#include <map> 
#include <numeric> 
#include <set> 
#include <string> 
#include <utility> 
#include <vector> 
#include <iostream> 
 
using namespace std; 
 
const double EPSILON = 1e-6;
  
const int MAX_RESULT_DOCUMENT_COUNT = 5;  
  
std::vector<int> TakeEvens(const std::vector<int>& numbers) { 
    std::vector<int> evens; 
    for (int x : numbers) { 
        if (x % 2 == 0) { 
            evens.push_back(x); 
        } 
    } 
    return evens; 
} 
  
std::string ReadLine() { 
    std::string s; 
    getline(std::cin, s); 
    return s; 
} 
 
int ReadLineWithNumber() { 
    int result; 
    std::cin >> result; 
    ReadLine(); 
    return result; 
} 
 
std::vector<std::string> SplitIntoWords(const std::string& text) { 
    std::vector<std::string> words; 
    std::string word; 
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
    void SetStopWords(const std::string& text) { 
        for (const std::string& word : SplitIntoWords(text)) { 
            stop_words_.insert(word); 
        } 
    }     
     
    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) { 
        const std::vector<std::string> words = SplitIntoWordsNoStop(document); 
        const double inv_word_count = 1.0 / words.size(); 
        for (const std::string& word : words) { 
            word_to_document_freqs_[word][document_id] += inv_word_count; 
        } 
        documents_.emplace(document_id,  
            DocumentData{ 
                ComputeAverageRating(ratings),  
                status 
            }); 
    } 
 
    template <typename DocumentPredicate> 
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {             
        const Query query = ParseQuery(raw_query); 
        auto matched_documents = FindAllDocuments(query, document_predicate); 
         
        sort(matched_documents.begin(), matched_documents.end(), 
             [](const Document& lhs, const Document& rhs) { 
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) { 
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
 
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {             
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; }); 
    } 
 
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const {             
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL); 
    } 
 
    int GetDocumentCount() const { 
        return documents_.size(); 
    } 
     
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const { 
        const Query query = ParseQuery(raw_query); 
        std::vector<std::string> matched_words; 
        for (const std::string& word : query.plus_words) { 
            if (word_to_document_freqs_.count(word) == 0) { 
                continue; 
            } 
            if (word_to_document_freqs_.at(word).count(document_id)) { 
                matched_words.push_back(word); 
            } 
        } 
        for (const std::string& word : query.minus_words) { 
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
 
    std::set<std::string> stop_words_; 
    std::map<std::string, std::map<int, double>> word_to_document_freqs_; 
    std::map<int, DocumentData> documents_; 
     
    bool IsStopWord(const std::string& word) const { 
        return stop_words_.count(word) > 0; 
    } 
     
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const { 
        std::vector<std::string> words; 
        for (const std::string& word : SplitIntoWords(text)) { 
            if (!IsStopWord(word)) { 
                words.push_back(word); 
            } 
        } 
        return words; 
    } 
     
    static int ComputeAverageRating(const std::vector<int>& ratings) { 
        if (ratings.empty()) { 
            return 0; 
        } 
        return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size()); 
    } 
     
    struct QueryWord { 
        std::string data; 
        bool is_minus; 
        bool is_stop; 
    }; 
     
    QueryWord ParseQueryWord(std::string text) const { 
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
        std::set<std::string> plus_words; 
        std::set<std::string> minus_words; 
    }; 
     
    Query ParseQuery(const std::string& text) const { 
        Query query; 
        for (const std::string& word : SplitIntoWords(text)) { 
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
     
    double ComputeWordInverseDocumentFreq(const std::string& word) const { 
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size()); 
    } 
 
    template <typename DocumentPredicate> 
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const { 
        std::map<int, double> document_to_relevance; 
        for (const std::string& word : query.plus_words) { 
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
         
        for (const std::string& word : query.minus_words) { 
            if (word_to_document_freqs_.count(word) == 0) { 
                continue; 
            } 
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) { 
                document_to_relevance.erase(document_id); 
            } 
        } 
 
        std::vector<Document> matched_documents; 
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
 
 
// -------- Начало модульных тестов поисковой системы ---------- 
 
// ----------------------- Фреймворк --------------------------- 
 
template <typename T>  
void AssertImpl(const T& t, const std::string& t_str, const std::string& file, const std::string& func, unsigned line, const std::string& hint) {  
    if (!t) {  
        std::cout << std::boolalpha;  
        std::cout << file << "("s << line << "): "s << func << ": "s;  
        std::cout << "ASSERT("s << t_str << ") failed."s;  
        if (!hint.empty()) {  
            std::cout << " Hint: "s << hint;  
        }  
        std::cout << std::endl;  
        // abort();  
    }  
}  
  
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint)) 
 
#define ASSERT(a) AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, "")  
 
template <typename T, typename U> 
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file, const std::string& func, unsigned line, const std::string& hint) { 
    if (t != u) { 
        std::cout << std::boolalpha; 
        std::cout << file << "("s << line << "): "s << func << ": "s; 
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s; 
        std::cout << t << " != "s << u << "."s; 
        if (!hint.empty()) { 
            std::cout << " Hint: "s << hint; 
        } 
        std::cout << std::endl; 
        // abort(); 
    } 
} 
 
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint)) 
 
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s) 
 
// -------------------------- Тесты ---------------------------- 
 
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов 
/*
void TestExcludeStopWordsFromAddedDocumentContent() 
{ 
    const int doc_id = 42; 
    const string content = "cat in the city"s; 
    const vector<int> ratings = { 1, 2, 3 }; 
 
    { 
        SearchServer server; 
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings); 
        const auto found_docs = server.FindTopDocuments("in"s); 
        ASSERT_EQUAL(found_docs.size(), 1); 
        const Document& doc0 = found_docs[0]; 
        ASSERT_EQUAL(doc0.id, doc_id); 
    } 
 
    { 
        SearchServer server; 
        server.SetStopWords("in the"s); 
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings); 
        ASSERT(server.FindTopDocuments("in"s).empty()); 
    } 
} 
*/
 
void TestSearchAddDocuments()
{
    {
        const int doc_id_1 = 0;
        const int doc_id_2 = 1;
        const int doc_id_3 = 2;

        const std::vector<int> ratings = { 1, 2, 3 };

        const std::string document_1 = "white cat and fancy collar"s;

        const std::string document_2 = "dog near red town"s;
        const std::string document_3 = ""s;

        SearchServer server;

        ASSERT_EQUAL(server.GetDocumentCount(), 0);

        server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, ratings);

        ASSERT_EQUAL(server.GetDocumentCount(), 1);

        server.AddDocument(doc_id_3, document_3, DocumentStatus::ACTUAL, ratings);

        const std::string content = "dog"s;

        ASSERT(server.FindTopDocuments(content).empty());

        server.AddDocument(doc_id_2, document_2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments(content);

        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];

        ASSERT_EQUAL(doc0.id, doc_id_2);
    }
}
 
void TestNoStopWords() 
{ 
    const int doc_id = 0; 
    const std::vector<int> ratings = { 1, 2, 3 }; 
    const std::string stop_words = "and cat"s; 
    const std::string document = "white cat and fancy collar"s; 
    const std::string content = "cat"s; 
 
    { 
        SearchServer server; 
        server.AddDocument(doc_id, document, DocumentStatus::ACTUAL, ratings); 
 
        const auto found_docs = server.FindTopDocuments(content); 
        ASSERT_EQUAL(found_docs.size(), 1); 
        ASSERT_EQUAL(found_docs[0].id, doc_id); 
    } 
 
    { 
        SearchServer server; 
        server.SetStopWords(stop_words); 
        server.AddDocument(doc_id, document, DocumentStatus::ACTUAL, ratings); 
 
        const auto found_docs = server.FindTopDocuments(content); 
        ASSERT(found_docs.empty()); 
    } 
} 
 
void TestSupportMinusWords() 
{ 
    const int doc_id_1 = 0; 
    const int doc_id_2 = 1; 
    const std::vector<int> ratings = { 1, 2, 3 }; 
 
    SearchServer server; 
 
    const std::string document_1 = "nice cat and fancy collar"s; 
    const std::string document_2 = "white cat and fancy collar"s; 
    const std::string content = "-nice cat and fancy collar"s; 
 
    server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, ratings); 
    ASSERT(server.FindTopDocuments(content).empty()); 
 
    server.AddDocument(doc_id_2, document_2, DocumentStatus::ACTUAL, ratings); 
 
    const auto found_docs = server.FindTopDocuments(content); 
    ASSERT_EQUAL(found_docs.size(), 1); 
 
    const Document& doc = found_docs[0]; 
    ASSERT_EQUAL(doc.id, doc_id_2); 
} 
 
void TestDocumentMatching()
{
    const int doc_id_1 = 0;
    const int doc_id_2 = 1;
    const int doc_id_3 = 2;
    const int doc_id_4 = 3;
    const int doc_id_5 = 4;
    const std::vector<int> ratings = { 1, 2, 3, 4, 5 };

    SearchServer server;

    const std::string document_1 = ""s;
    const std::string document_2 = "white cat and fancy collar"s;
    const std::string document_3 = "nice cat and fancy collar"s;
    const std::string document_4 = "nice cat and fancy collar and something else"s;
    const std::string document_5 = "nice cat and fancy collar, like really"s;
    const std::string content = "-white fancy cat"s;

    server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, ratings);
    {
        auto [words, status] = server.MatchDocument(content, doc_id_1);
        ASSERT(words.empty());
    }

    server.AddDocument(doc_id_2, document_2, DocumentStatus::ACTUAL, ratings);
    {
        auto [words, status] = server.MatchDocument(content, doc_id_2);
        ASSERT(words.empty());
    }

    server.AddDocument(doc_id_3, document_3, DocumentStatus::ACTUAL, ratings);
    {
        auto [words, status] = server.MatchDocument(content, doc_id_3);
        ASSERT_EQUAL(words.size(), 2);
        ASSERT_EQUAL(words[0], "cat"s);
        ASSERT_EQUAL(words[1], "fancy"s);

    }
    server.AddDocument(doc_id_4, document_4, DocumentStatus::ACTUAL, ratings);
    {
        server.SetStopWords("something"s);

        auto [words, status] = server.MatchDocument(content, doc_id_4);

    }
    server.AddDocument(doc_id_5, document_5, DocumentStatus::ACTUAL, ratings);
    {
        server.SetStopWords("cat"s);

        auto [words, status] = server.MatchDocument(content, doc_id_5);
    }
}
 
void TestSortByRelevance() 
{ 
 
    const int doc_id_1 = 0; 
    const int doc_id_2 = 1; 
    const int doc_id_3 = 2; 
    const int doc_id_4 = 3; 
 
    SearchServer server; 
 
    const std::string document_1 = ""s; 
    const std::string document_2 = "white cat and fancy collar"s; 
    const std::string document_3 = "nice cat and fancy collar"s; 
    const std::string document_4 = "cat fancy tail"s; 
 
    const std::string content = "white fancy cat"s; 
 
    server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, { 8, -3 }); 
    server.AddDocument(doc_id_2, document_2, DocumentStatus::ACTUAL, { 7, 2, 7 }); 
    server.AddDocument(doc_id_3, document_3, DocumentStatus::ACTUAL, { 5, -12, 2, 1 }); 
    server.AddDocument(doc_id_4, document_4, DocumentStatus::ACTUAL, { 5, -12, 10, 1 }); 
 
    auto results = server.FindTopDocuments(content); 
 
    ASSERT(is_sorted(results.begin(), results.end(), [](const Document& lhs, const Document& rhs) { return lhs.relevance > rhs.relevance; })); 
 
    ASSERT_EQUAL(results.size(), 3); 
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
        ASSERT(found_docs[0].rating == round((1 + 2 + 3 + 0.0) / 3)); 
    }
}
 
void TestFilteringUsingPredicatFunction() 
{ 
    const int doc_id_1 = 0; 
    const int doc_id_2 = 1; 
    const int doc_id_3 = 2; 
    const int doc_id_4 = 3; 
    const int doc_id_5 = 4; 
 
    SearchServer server; 
 
    const std::string document_1 = "fluffy cat white tail"s; 
    const std::string document_2 = "white dog beautiful eyes"s; 
    const std::string document_3 = "white cat and fancy collar"s; 
    const std::string document_4 = "fancy cat white eyes"s; 
    const std::string document_5 = "black cat and fancy tail"s; 
 
    const std::string content = "white tail eyes"s; 
 
    server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, { -8 }); 
    server.AddDocument(doc_id_2, document_2, DocumentStatus::BANNED, { 10 }); 
    server.AddDocument(doc_id_3, document_3, DocumentStatus::ACTUAL, { 5 }); 
    server.AddDocument(doc_id_4, document_4, DocumentStatus::IRRELEVANT, { 7 }); 
    server.AddDocument(doc_id_5, document_5, DocumentStatus::REMOVED, { -2 }); 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return (rating > 0); 
            }); 
 
        ASSERT_EQUAL(results.size(), 3); 
        ASSERT_EQUAL(results[0].id, doc_id_2); 
        ASSERT_EQUAL(results[1].id, doc_id_4); 
        ASSERT_EQUAL(results[2].id, doc_id_3); 
    } 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return (status == DocumentStatus::BANNED); 
            }); 
 
        ASSERT_EQUAL(results.size(), 1); 
        ASSERT_EQUAL(results[0].id, doc_id_2); 
    } 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return (document_id % 2); 
            }); 
 
        ASSERT_EQUAL(results.size(), 2); 
        ASSERT_EQUAL(results[0].id, doc_id_2); 
        ASSERT_EQUAL(results[1].id, doc_id_4); 
    } 
} 
 
void TestSearchDocumentsByStatus() 
{ 
    const int doc_id_1 = 0; 
    const int doc_id_2 = 1; 
    const int doc_id_3 = 2; 
    const int doc_id_4 = 3; 
    const int doc_id_5 = 4; 
 
    SearchServer server; 
 
    const std::string document_1 = "fluffy cat white tail"s; 
    const std::string document_2 = "white dog beautiful eyes"s; 
    const std::string document_3 = "white cat and fancy collar"s; 
    const std::string document_4 = "fancy cat white eyes"s; 
    const std::string document_5 = "black cat and fancy tail"s; 
 
    const std::string content = "white tail eyes"s; 
 
    server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, { -8 }); 
    server.AddDocument(doc_id_2, document_2, DocumentStatus::BANNED, { 10 }); 
    server.AddDocument(doc_id_3, document_3, DocumentStatus::ACTUAL, { 5 }); 
    server.AddDocument(doc_id_4, document_4, DocumentStatus::IRRELEVANT, { 7 }); 

    {
        auto results = server.FindTopDocuments(content,
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating)
            {
                return (status == DocumentStatus::REMOVED);
            });

        ASSERT_EQUAL(results.size(), 0);
    }

    server.AddDocument(doc_id_5, document_5, DocumentStatus::REMOVED, { -2 }); 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return (status == DocumentStatus::ACTUAL); 
            }); 
 
        ASSERT_EQUAL(results.size(), 2); 
        ASSERT_EQUAL(results[0].id, doc_id_1); 
        ASSERT_EQUAL(results[1].id, doc_id_3); 
    } 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return (status == DocumentStatus::BANNED); 
            }); 
 
        ASSERT_EQUAL(results.size(), 1); 
        ASSERT_EQUAL(results[0].id, doc_id_2); 
    } 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return (status == DocumentStatus::IRRELEVANT); 
            }); 
 
        ASSERT_EQUAL(results.size(), 1); 
        ASSERT_EQUAL(results[0].id, doc_id_4); 
    } 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return (status == DocumentStatus::REMOVED); 
            }); 
 
        ASSERT_EQUAL(results.size(), 1); 
        ASSERT_EQUAL(results[0].id, doc_id_5); 
    } 
 
    { 
        auto results = server.FindTopDocuments(content, 
            []([[maybe_unused]] int document_id, DocumentStatus status, [[maybe_unused]] int rating) 
            { 
                return ((status == DocumentStatus::ACTUAL) | 
                    (status == DocumentStatus::BANNED) | 
                    (status == DocumentStatus::IRRELEVANT) | 
                    (status == DocumentStatus::REMOVED) 
                    ); 
            }); 

        ASSERT_EQUAL(results.size(), 5); 
    } 
} 
 
void TestCorrectCalcRelevance() 
{ 
    const int doc_id_1 = 0; 
    const int doc_id_2 = 1; 
    const int doc_id_3 = 2; 
 
    SearchServer server; 
 
    const std::string document_1 = "fluffy cat white tail"s; 
    const std::string document_2 = "white dog beautiful eyes"s; 
    const std::string document_3 = "white cat and fancy collar"s; 
 
    const std::string content = "cat white tail"s; 
 
    server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, { 8, -3 }); 
    server.AddDocument(doc_id_2, document_2, DocumentStatus::ACTUAL, { 7, 2, 7 }); 
    server.AddDocument(doc_id_3, document_3, DocumentStatus::ACTUAL, { 5, -12, 2, 1 }); 
 
    double value_1 = abs(log(3) / 2); 
    double value_2 = abs(log(3) / 6); 
    double value_3 = abs(log(3) / 1); 
 
    auto results = server.FindTopDocuments(content, DocumentStatus::ACTUAL); 
 
    ASSERT_EQUAL(results.size(), 3); 
    ASSERT((results[0].relevance - value_1) < EPSILON); 
    ASSERT((results[1].relevance - value_2) < EPSILON); 
    ASSERT((results[2].relevance - value_3) < EPSILON); 
} 

void TestEmptyContent() 

{ 
    const int doc_id = 0; 
    const std::string content = "cat in the city"s; 
    const std::vector<int> ratings = { 1, 2, 3 }; 
    { 
        SearchServer server; 
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings); 
        const auto found_docs = server.FindTopDocuments(""s); 
        ASSERT(found_docs.empty()); 
    } 
} 
 
void TestCornerCase() 
{ 
    const int doc_id_1 = 0; 
    const int doc_id_2 = 1; 
    const std::vector<int> ratings = { 1, 2, 3 }; 
 
    SearchServer server; 
 
    const std::string document_1 = "nice cat and fancy collar"s; 
    const std::string document_2 = "white cat and fancy collar"s; 
    const std::string content = "-cat cat"s; 
 
    server.AddDocument(doc_id_1, document_1, DocumentStatus::ACTUAL, ratings); 
    server.AddDocument(doc_id_2, document_2, DocumentStatus::ACTUAL, ratings); 
 
    const auto found_docs = server.FindTopDocuments(content); 
    ASSERT(found_docs.empty()); 
} 
 
void TestSearchServer() 
{ 
    // TestExcludeStopWordsFromAddedDocumentContent(); 
    TestSearchAddDocuments(); 
    TestNoStopWords(); 
    TestSupportMinusWords(); 
    TestDocumentMatching(); 
    TestSortByRelevance(); 
    TestRating(); 
    TestFilteringUsingPredicatFunction(); 
    TestSearchDocumentsByStatus(); 
    TestCorrectCalcRelevance(); 
    TestEmptyContent(); 
    TestCornerCase(); 
} 
 
int main() 
{ 
    TestSearchServer(); 
    std::cout << "Search server testing finished"s << std::endl; 
    return 0; 
} 