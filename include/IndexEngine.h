#pragma once
#include <vector>
#include <string>

using namespace std;

struct PrimaryIndexEntry {
    string key;
    int offset;
};

struct SecondaryIndexEntry {
    string secondaryKey;
    int headOffset;
};

class IndexEngine {
public:
    vector<PrimaryIndexEntry> docPrimary;
    vector<PrimaryIndexEntry> appPrimary;
    vector<SecondaryIndexEntry> docNameSec;
    vector<SecondaryIndexEntry> docSpecSec;
    vector<SecondaryIndexEntry> appDocIdSec;

    const string DOC_NAME_INV_FILE = "data/doc_name_inv.bin";
    const string DOC_SPEC_INV_FILE = "data/doc_spec_inv.bin";
    const string APP_DOCID_INV_FILE = "data/app_docid_inv.bin";

    // Primary Index Methods
    int binarySearchPrimary(const vector<PrimaryIndexEntry>& index, const string& key);
    void insertPrimary(vector<PrimaryIndexEntry>& index, const string& key, int offset);
    void deletePrimary(vector<PrimaryIndexEntry>& index, const string& key);
    void updatePrimaryKey(vector<PrimaryIndexEntry>& index, const string& oldKey, const string& newKey, int offset);

    // Secondary Index Methods
    int binarySearchSecondary(const vector<SecondaryIndexEntry>& index, const string& secKey);
    void addSecondary(vector<SecondaryIndexEntry>& index, const string& invertedFileName, const string& secKey, const string& primaryKey);
    vector<string> getFromSecondary(const vector<SecondaryIndexEntry>& index, const string& invertedFileName, const string& secKey);
    void deleteFromSecondary(vector<SecondaryIndexEntry>& index, const string& invertedFileName, const string& secKey, const string& primaryKey);
    
    // File Init & Rebuild
    void initInvertedFiles();
    void rebuild();
};