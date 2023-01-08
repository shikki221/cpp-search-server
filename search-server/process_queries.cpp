#include "document.h"
#include "search_server.h"
#include "process_queries.h"

using namespace std;

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par,
                   queries.begin(), queries.end(), result.begin(),
                   [&search_server](std::string const qry) {
                       return search_server.FindTopDocuments(qry);
                   });
    return result;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server,
                     const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> documents(queries.size());
    std::transform(std::execution::par,
                   queries.begin(), queries.end(), documents.begin(),
                   [&search_server](std::string const qry) {
                       return search_server.FindTopDocuments(qry);
                   });
    std::vector<Document> result;
    std::vector<Document>::iterator it;
    for (const std::vector<Document>& v : documents) {
        it = result.insert(result.end(), v.begin(), v.end());
    }
    return result;
}