#include "IndexEngine.h"
#include <fstream>
#include <iostream>
#include <cstring>

using namespace std;

static void ensureInvertedFile(const string& path) {
    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        ofstream out(path, ios::binary);
    }
}

void IndexEngine::initInvertedFiles() {
    ensureInvertedFile(DOC_NAME_INV_FILE);
    ensureInvertedFile(DOC_SPEC_INV_FILE);
    ensureInvertedFile(APP_DOCID_INV_FILE);
}




int IndexEngine::binarySearchPrimary(const vector<PrimaryIndexEntry>& index, const string& key) {
    int left = 0, right = index.size() - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (index[mid].key == key) return index[mid].offset;
        if (index[mid].key < key) left = mid + 1;
        else right = mid - 1;
    }
    return -1; // Not found
}

void IndexEngine::insertPrimary(vector<PrimaryIndexEntry>& index, const string& key, int offset) {
    PrimaryIndexEntry entry = {key, offset};
    if (index.empty()) {
        index.push_back(entry);
        return;
    }

    int pos = 0;
    while (pos < index.size() && index[pos].key < key) {
        pos++;
    }

    // Shifting elements to maintain ascending order
    index.insert(index.begin() + pos, entry);
}

void IndexEngine::deletePrimary(vector<PrimaryIndexEntry>& index, const string& key) {
    int left = 0, right = index.size() - 1;
    int pos = -1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (index[mid].key == key) {
            pos = mid;
            break;
        }
        if (index[mid].key < key) left = mid + 1;
        else right = mid - 1;
    }

    if (pos != -1) {
        // Shifting elements left to delete
        index.erase(index.begin() + pos);
    }
}

void IndexEngine::updatePrimaryKey(vector<PrimaryIndexEntry>& index, const string& oldKey, const string& newKey, int offset) {
    deletePrimary(index, oldKey);
    insertPrimary(index, newKey, offset);
}




int IndexEngine::binarySearchSecondary(const vector<SecondaryIndexEntry>& index, const string& secKey) {
    int left = 0, right = index.size() - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (index[mid].secondaryKey == secKey) return mid;
        if (index[mid].secondaryKey < secKey) left = mid + 1;
        else right = mid - 1;
    }
    return -1; 
}

void IndexEngine::addSecondary(vector<SecondaryIndexEntry>& index, const string& invertedFileName, const string& secKey, const string& primaryKey) {
    InvertedNode newNode;
    memset(&newNode, ' ', sizeof(InvertedNode));
    memcpy(newNode.primaryKey, primaryKey.data(), min(primaryKey.size(), sizeof(newNode.primaryKey)));
    
    int secIndexPos = binarySearchSecondary(index, secKey);
    
    if (secIndexPos != -1) {
        // Secondary key exists, link to the old head
        newNode.nextOffset = index[secIndexPos].headOffset;
    } else {
        // New secondary key
        newNode.nextOffset = -1;
        
        SecondaryIndexEntry entry = {secKey, -1};
        int pos = 0;
        while (pos < index.size() && index[pos].secondaryKey < secKey) {
            pos++;
        }
        index.insert(index.begin() + pos, entry);
        secIndexPos = pos;
    }

    // Write to inverted list file (append to end)
    fstream fs(invertedFileName, ios::binary | ios::in | ios::out);
    fs.seekp(0, ios::end);
    int newOffset = fs.tellp();
    fs.write((char*)&newNode, sizeof(InvertedNode));

    // Update in-memory head
    index[secIndexPos].headOffset = newOffset;
}

vector<string> IndexEngine::getFromSecondary(const vector<SecondaryIndexEntry>& index, const string& invertedFileName, const string& secKey) {
    vector<string> primaryKeys;
    int secIndexPos = binarySearchSecondary(index, secKey);
    
    if (secIndexPos == -1) return primaryKeys;

    int currentOffset = index[secIndexPos].headOffset;
    ifstream in(invertedFileName, ios::binary);

    while (currentOffset != -1) {
        in.seekg(currentOffset);
        InvertedNode node;
        in.read((char*)&node, sizeof(InvertedNode));
        
        primaryKeys.push_back(trim(node.primaryKey, sizeof(node.primaryKey)));
        currentOffset = node.nextOffset;
    }

    return primaryKeys;
}

void IndexEngine::deleteFromSecondary(vector<SecondaryIndexEntry>& index, const string& invertedFileName, const string& secKey, const string& primaryKey) {
    int secIndexPos = binarySearchSecondary(index, secKey);
    if (secIndexPos == -1) return;

    int currentOffset = index[secIndexPos].headOffset;
    int prevOffset = -1;
    fstream fs(invertedFileName, ios::binary | ios::in | ios::out);

    while (currentOffset != -1) {
        fs.seekg(currentOffset);
        InvertedNode node;
        fs.read((char*)&node, sizeof(InvertedNode));

        string currentPK = trim(node.primaryKey, sizeof(node.primaryKey));

        if (currentPK == primaryKey) {
            if (prevOffset == -1) {
                // Deleting the head
                index[secIndexPos].headOffset = node.nextOffset;
            } else {
                // Deleting from the middle
                fs.seekg(prevOffset);
                InvertedNode prevNode;
                fs.read((char*)&prevNode, sizeof(InvertedNode));
                prevNode.nextOffset = node.nextOffset;
                fs.seekp(prevOffset);
                fs.write((char*)&prevNode, sizeof(InvertedNode));
            }
            break;
        }

        prevOffset = currentOffset;
        currentOffset = node.nextOffset;
    }
}