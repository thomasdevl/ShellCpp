//
// Created by Thomas Devlamminck on 19/01/2026.
//

#ifndef SHELL_STARTER_CPP_TRIE_H
#define SHELL_STARTER_CPP_TRIE_H

#include <vector>

class TrieNode {
public:
    bool endofWord;
    std::unordered_map<char, TrieNode*> children;

    TrieNode() : endofWord(false){}

    ~TrieNode() {
        for (auto& pair : children) {
            delete pair.second;
        }
    }
};


class Trie {
    TrieNode* root;

    // Helper for recursive prefix searching
    static void collectAllWords(TrieNode* node, const std::string& currentPrefix, std::vector<std::string>& results);

public:
    Trie() {root = new TrieNode();}
    ~Trie() { delete root; }

    void insert(const std::string& word) const;

    std::vector<std::string> get_completions(const std::string& prefix);

    std::string getLongestCommonPrefix(std::string prefix) const;
};


#endif //SHELL_STARTER_CPP_TRIE_H