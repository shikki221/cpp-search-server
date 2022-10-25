#include "remove_duplicates.h"
#include <set>

using namespace std;

void RemoveDuplicates(SearchServer& search_server){
    std::vector<int> list_del;
    std::set<std::set<std::string>> cont;
    for(int id : search_server){
        std::set<std::string> temp;
        for(const auto& [word, _] : search_server.GetWordFrequencies(id)){ // TODO как не получать warning unused variable _
            temp.insert(word);
        }
        const auto& [_, flag] = cont.insert(temp);
        if(flag == false){ // если вставка не удалась запоминаем id
            list_del.push_back(id);
        }
    }
    for(int id : list_del){
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id " << id << std::endl;
    }
}
