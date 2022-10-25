#include "remove_duplicates.h"
#include <set>

using namespace std;

void RemoveDuplicates(SearchServer& search_server){
    std::vector<int> documents_to_remove;
    std::set<std::set<std::string>> cont;
    for(int id : search_server){
        std::set<std::string> temp;
        for(const auto& [word, _] : search_server.GetWordFrequencies(id)){ 
            temp.insert(word);
        }
        const auto& [_, flag] = cont.insert(temp);
        if(flag == false){
            documents_to_remove.push_back(id);
        }
    }
    for(int id : documents_to_remove){
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id " << id << std::endl;
    }
}
