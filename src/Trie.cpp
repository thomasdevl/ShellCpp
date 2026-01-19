//
// Created by Thomas Devlamminck on 19/01/2026.
//

#include <string>
#include <unordered_map>
#include "Trie.hpp"
#include <vector>

// Trie used for autocompletion

void Trie::collectAllWords(TrieNode* node, const std::string& currentPrefix, std::vector<std::string> & results) {
    if (node->endofWord) {
        results.push_back(currentPrefix);
    }
    for (auto const& [ch, childNode] : node->children) {
        collectAllWords(childNode, currentPrefix + ch, results);
    }
}

void Trie::insert(const std::string& word) const {
    TrieNode* node = root;
    for (char c : word) {
        if (node->children.find(c) == node->children.end()) {
            node->children[c] = new TrieNode();
        }
        node = node->children[c];
    }
    node->endofWord = true;
}

std::vector<std::string> Trie::get_completions(const std::string& prefix) {
    TrieNode* node = root;
    for (char c : prefix) {
        if (node->children.find(c) == node->children.end()) {
            return {}; // No matches
        }
        node = node->children[c];
    }
    std::vector<std::string> results;
    collectAllWords(node, prefix, results);
    return results;
}

std::string Trie::getLongestCommonPrefix(std::string prefix) const {
    TrieNode* node = root;

    // go to the end of prefix
    for (char c : prefix) {
        if (node->children.find(c) == node->children.end()) {
            return prefix; // No matches at all
        }
        node = node->children[c];
    }

    // keep going if there is only one child + not end of word
    std::string lcp = prefix;
    while (node->children.size() == 1 && !node->endofWord) {
        auto it = node->children.begin();
        lcp += it->first;
        node = it->second;
    }

    return lcp;
}
