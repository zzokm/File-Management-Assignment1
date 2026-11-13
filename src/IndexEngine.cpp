#include "IndexEngine.h"
#include "StorageEngine.h"
#include <fstream>
#include <cstring>
#include <filesystem>

using namespace std;

static void ensureInvertedFile(const string& path) {
    filesystem::create_directories("data");
    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        ofstream out(path, ios::binary);
    }
}

void IndexEngine::initInvertedFiles() {
    filesystem::create_directories("data");
    ensureInvertedFile(DOC_NAME_INV_FILE);
    ensureInvertedFile(DOC_SPEC_INV_FILE);
    ensureInvertedFile(APP_DOCID_INV_FILE);
}

int IndexEngine::binarySearchPrimary(const vector<PrimaryIndexEntry>& index, const string& key) const {
    int l = 0, r = (int)index.size() - 1;
    while (l <= r) {
        int mid = (l + r) / 2;
        if (index[mid].key == key) return index[mid].offset;
        if (index[mid].key < key) l = mid + 1;
        else r = mid - 1;
    }
    return -1;
}

void IndexEngine::insertPrimary(vector<PrimaryIndexEntry>& index, const string& key, int offset) {
    int l = 0, r = (int)index.size() - 1;
    int pos = index.size();
    while (l <= r) {
        int mid = (l + r) / 2;
        if (index[mid].key >= key) {
            pos = mid;
            r = mid - 1;
        } else {
            l = mid + 1;
        }
    }
    index.insert(index.begin() + pos, {key, offset});
}

void IndexEngine::deletePrimary(vector<PrimaryIndexEntry>& index, const string& key) {
    int l = 0, r = (int)index.size() - 1;
    while (l <= r) {
        int mid = (l + r) / 2;
        if (index[mid].key == key) {
            index.erase(index.begin() + mid);
            return;
        }
        if (index[mid].key < key) l = mid + 1;
        else r = mid - 1;
    }
}

void IndexEngine::updatePrimaryKey(vector<PrimaryIndexEntry>& index, const string& oldKey, const string& newKey, int offset) {
    deletePrimary(index, oldKey);
    insertPrimary(index, newKey, offset);
}

int IndexEngine::binarySearchSecondary(const vector<SecondaryIndexEntry>& index, const string& secKey) const {
    int l = 0, r = (int)index.size() - 1;
    while (l <= r) {
        int mid = (l + r) / 2;
        if (index[mid].secondaryKey == secKey) return mid;
        if (index[mid].secondaryKey < secKey) l = mid + 1;
        else r = mid - 1;
    }
    return -1;
}

void IndexEngine::addSecondary(vector<SecondaryIndexEntry>& index, const string& file, const string& secKey, const string& primaryKey) {
    filesystem::create_directories("data");
    ensureInvertedFile(file);

    InvertedNode node;
    memset(&node, ' ', sizeof(InvertedNode));
    memcpy(node.primaryKey, primaryKey.data(), min(primaryKey.size(), sizeof(node.primaryKey)));

    int pos = binarySearchSecondary(index, secKey);
    if (pos != -1) {
        node.nextOffset = index[pos].headOffset;
    } else {
        node.nextOffset = -1;
        int l = 0, r = (int)index.size() - 1;
        pos = index.size();
        while (l <= r) {
            int mid = (l + r) / 2;
            if (index[mid].secondaryKey >= secKey) {
                pos = mid;
                r = mid - 1;
            } else {
                l = mid + 1;
            }
        }
        index.insert(index.begin() + pos, {secKey, -1});
    }

    fstream fs(file, ios::binary | ios::in | ios::out);
    fs.seekp(0, ios::end);
    int offset = fs.tellp();
    fs.write((char*)&node, sizeof(InvertedNode));

    index[pos].headOffset = offset;
}

vector<string> IndexEngine::getFromSecondary(const vector<SecondaryIndexEntry>& index, const string& file, const string& secKey) {
    vector<string> results;
    int pos = binarySearchSecondary(index, secKey);
    if (pos == -1) return results;

    ifstream in(file, ios::binary);
    int curr = index[pos].headOffset;
    while (curr != -1) {
        in.seekg(curr);
        InvertedNode node;
        in.read((char*)&node, sizeof(InvertedNode));
        results.push_back(trim(node.primaryKey, sizeof(node.primaryKey)));
        curr = node.nextOffset;
    }
    return results;
}

void IndexEngine::deleteFromSecondary(vector<SecondaryIndexEntry>& index, const string& file, const string& secKey, const string& primaryKey) {
    int pos = binarySearchSecondary(index, secKey);
    if (pos == -1) return;

    fstream fs(file, ios::binary | ios::in | ios::out);
    int curr = index[pos].headOffset;
    int prev = -1;

    while (curr != -1) {
        fs.seekg(curr);
        InvertedNode node;
        fs.read((char*)&node, sizeof(InvertedNode));

        if (trim(node.primaryKey, sizeof(node.primaryKey)) == primaryKey) {
            if (prev == -1) {
                index[pos].headOffset = node.nextOffset;
            } else {
                fs.seekg(prev);
                InvertedNode prevNode;
                fs.read((char*)&prevNode, sizeof(InvertedNode));
                prevNode.nextOffset = node.nextOffset;
                fs.seekp(prev);
                fs.write((char*)&prevNode, sizeof(InvertedNode));
            }
            break;
        }
        prev = curr;
        curr = node.nextOffset;
    }

    if (index[pos].headOffset == -1) {
        index.erase(index.begin() + pos);
    }
}

void IndexEngine::rebuild() {
    filesystem::create_directories("data");
    docPrimary.clear();
    appPrimary.clear();
    docNameSec.clear();
    docSpecSec.clear();
    appDocIdSec.clear();

    initInvertedFiles();
    ofstream(DOC_NAME_INV_FILE, ios::binary | ios::trunc);
    ofstream(DOC_SPEC_INV_FILE, ios::binary | ios::trunc);
    ofstream(APP_DOCID_INV_FILE, ios::binary | ios::trunc);

    ifstream docF(StorageEngine::DOCTORS_FILE, ios::binary);
    if (docF.is_open()) {
        docF.seekg(0, ios::end);
        int end = docF.tellg(), offset = 4;
        while (offset + sizeof(DoctorRecord) <= end) {
            DoctorRecord d;
            if (StorageEngine::readDoctor(offset, d)) {
                string id = trim(d.doctorID, sizeof(d.doctorID));
                insertPrimary(docPrimary, id, offset);
                addSecondary(docNameSec, DOC_NAME_INV_FILE, trim(d.doctorName, sizeof(d.doctorName)), id);
                addSecondary(docSpecSec, DOC_SPEC_INV_FILE, trim(d.specialty, sizeof(d.specialty)), id);
            }
            offset += sizeof(DoctorRecord);
        }
    }

    ifstream appF(StorageEngine::APPOINTMENTS_FILE, ios::binary);
    if (appF.is_open()) {
        appF.seekg(0, ios::end);
        int end = appF.tellg(), offset = 4;
        while (offset + sizeof(AppointmentRecord) <= end) {
            AppointmentRecord a;
            if (StorageEngine::readAppointment(offset, a)) {
                string id = trim(a.appointmentID, sizeof(a.appointmentID));
                insertPrimary(appPrimary, id, offset);
                addSecondary(appDocIdSec, APP_DOCID_INV_FILE, trim(a.doctorID, sizeof(a.doctorID)), id);
            }
            offset += sizeof(AppointmentRecord);
        }
    }
}
