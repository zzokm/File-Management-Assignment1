#include "QueryEngine.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

QueryEngine::QueryEngine(BusinessLogic& bl) : businessLogic(bl) {}

string QueryEngine::toLower(const string& str) {
    string res = str;
    for (char& c : res) c = (char)tolower((unsigned char)c);
    return res;
}

string QueryEngine::trimString(const string& str) {
    size_t s = 0;
    while (s < str.size() && (isspace((unsigned char)str[s]) || str[s] == '\0')) s++;
    if (s >= str.size()) return "";
    size_t e = str.size() - 1;
    while (e > s && (isspace((unsigned char)str[e]) || str[e] == '\0')) e--;
    return str.substr(s, e - s + 1);
}

string QueryEngine::extractQuotedValue(const string& str) {
    string trimmed = trimString(str);
    if (trimmed.size() >= 2 && ((trimmed.front() == '\'' && trimmed.back() == '\'') ||
                                (trimmed.front() == '"' && trimmed.back() == '"'))) {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

static size_t findKeyword(const string& str, const string& keyword, size_t startPos = 0) {
    string lowerStr = QueryEngine::toLower(str);
    string lowerKw = QueryEngine::toLower(keyword);
    bool inQuote = false;

    for (size_t i = startPos; i < str.size(); i++) {
        if (str[i] == '\'' || str[i] == '"') inQuote = !inQuote;
        else if (!inQuote && i + lowerKw.size() <= str.size()) {
            if (lowerStr.substr(i, lowerKw.size()) == lowerKw) return i;
        }
    }
    return string::npos;
}

ParsedQuery QueryEngine::parseQuery(const string& queryString) {
    ParsedQuery pq;
    string q = trimString(queryString);
    if (!q.empty() && q.back() == ';') q = trimString(q.substr(0, q.size() - 1));

    if (q.empty() || toLower(q.substr(0, 7)) != "select ") {
        pq.errorMessage = "Query must start with SELECT keyword.";
        return pq;
    }

    size_t fromPos = findKeyword(q, " from ", 6);
    if (fromPos == string::npos) {
        pq.errorMessage = "Syntax error: missing FROM clause.";
        return pq;
    }

    pq.selectField = trimString(q.substr(7, fromPos - 7));
    size_t afterFrom = fromPos + 6;
    size_t wherePos = findKeyword(q, " where ", afterFrom);

    string tablePart = (wherePos == string::npos) ? trimString(q.substr(afterFrom))
                                                  : trimString(q.substr(afterFrom, wherePos - afterFrom));
    pq.hasWhere = (wherePos != string::npos);

    string tableLower = toLower(tablePart);
    if (tableLower == "doctors" || tableLower == "doctor") pq.table = "doctors";
    else if (tableLower == "appointments" || tableLower == "appointment") pq.table = "appointments";
    else {
        pq.errorMessage = "Unknown table '" + tablePart + "'. Valid tables: Doctors, Appointments.";
        return pq;
    }

    if (pq.hasWhere) {
        string wherePart = trimString(q.substr(wherePos + 7));
        if (wherePart.empty()) {
            pq.errorMessage = "Syntax error: empty WHERE clause.";
            return pq;
        }

        vector<string> condTokens;
        size_t cur = 0;
        while (cur < wherePart.size()) {
            size_t andPos = findKeyword(wherePart, " and ", cur);
            if (andPos == string::npos) {
                condTokens.push_back(trimString(wherePart.substr(cur)));
                break;
            }
            condTokens.push_back(trimString(wherePart.substr(cur, andPos - cur)));
            cur = andPos + 5;
        }

        for (const string& condStr : condTokens) {
            size_t eqPos = findKeyword(condStr, "=", 0);
            if (eqPos == string::npos) {
                pq.errorMessage = "Syntax error in condition: '" + condStr + "'. Expected Column = 'Value'.";
                return pq;
            }

            string rawCol = trimString(condStr.substr(0, eqPos));
            string rawVal = extractQuotedValue(condStr.substr(eqPos + 1));
            string colLower = toLower(rawCol);
            string normCol;

            if (pq.table == "doctors") {
                if (colLower == "doctor id" || colLower == "doctorid" || colLower == "id") normCol = "Doctor ID";
                else if (colLower == "doctor name" || colLower == "doctorname" || colLower == "name") normCol = "Doctor Name";
                else if (colLower == "doctor specialty" || colLower == "doctorspecialty" || colLower == "specialty") normCol = "Doctor Specialty";
                else if (colLower == "address") normCol = "Address";
                else { pq.errorMessage = "Unknown Doctors column '" + rawCol + "'."; return pq; }
            } else {
                if (colLower == "appointment id" || colLower == "appointmentid" || colLower == "id") normCol = "Appointment ID";
                else if (colLower == "appointment date" || colLower == "appointmentdate" || colLower == "date") normCol = "Appointment Date";
                else if (colLower == "doctor id" || colLower == "doctorid") normCol = "Doctor ID";
                else { pq.errorMessage = "Unknown Appointments column '" + rawCol + "'."; return pq; }
            }
            pq.conditions.push_back({normCol, rawVal});
        }
    }

    pq.isValid = true;
    return pq;
}

bool QueryEngine::executeQuery(const string& queryString) {
    ParsedQuery pq = parseQuery(queryString);
    if (!pq.isValid) {
        cout << "\n[Query Error]: " << pq.errorMessage << "\n";
        return false;
    }
    return (pq.table == "doctors") ? executeDoctorQuery(pq) : executeAppointmentQuery(pq);
}

bool QueryEngine::executeDoctorQuery(const ParsedQuery& pq) {
    IndexEngine& ie = businessLogic.getIndexEngine();

    auto fetchDocs = [&](const vector<string>& pks) {
        vector<DoctorRecord> docs;
        for (const string& pk : pks) {
            int offset = ie.binarySearchPrimary(ie.docPrimary, pk);
            if (offset != -1) {
                DoctorRecord doc;
                if (StorageEngine::readDoctor(offset, doc)) docs.push_back(doc);
            }
        }
        return docs;
    };

    if (!pq.hasWhere) {
        cout << "\n[Query Plan]: Full Table Scan on Doctors\n";
        printDoctorsTable(businessLogic.listAllDoctors(), pq.selectField);
        return true;
    }

    if (pq.conditions.size() == 1) {
        const auto& cond = pq.conditions[0];
        if (cond.column == "Doctor ID") {
            cout << "\n[Query Plan]: Binary Search on Doctor Primary Index (Key = '" << cond.value << "')\n";
            int offset = ie.binarySearchPrimary(ie.docPrimary, cond.value);
            if (offset == -1) {
                cout << "[Result]: Doctor with ID '" << cond.value << "' not found.\n";
                return true;
            }
            DoctorRecord doc;
            if (StorageEngine::readDoctor(offset, doc)) {
                printDoctorsTable({doc}, pq.selectField);
            } else {
                cout << "[Result]: Record at offset " << offset << " is deleted or invalid.\n";
            }
            return true;
        } else if (cond.column == "Doctor Name") {
            cout << "\n[Query Plan]: Secondary Index Lookup via Inverted List for Doctor Name ('" << cond.value << "')\n";
            vector<string> pks = ie.getFromSecondary(ie.docNameSec, ie.DOC_NAME_INV_FILE, cond.value);
            cout << "[Index Trace]: Found " << pks.size() << " primary key reference(s) in inverted list.\n";
            if (pks.empty()) {
                cout << "[Result]: No doctors found with Name = '" << cond.value << "'.\n";
                return true;
            }
            printDoctorsTable(fetchDocs(pks), pq.selectField);
            return true;
        } else if (cond.column == "Doctor Specialty") {
            cout << "\n[Query Plan]: Secondary Index Lookup via Inverted List for Doctor Specialty ('" << cond.value << "')\n";
            vector<string> pks = ie.getFromSecondary(ie.docSpecSec, ie.DOC_SPEC_INV_FILE, cond.value);
            cout << "[Index Trace]: Found " << pks.size() << " primary key reference(s) in inverted list.\n";
            if (pks.empty()) {
                cout << "[Result]: No doctors found with Specialty = '" << cond.value << "'.\n";
                return true;
            }
            printDoctorsTable(fetchDocs(pks), pq.selectField);
            return true;
        } else {
            cout << "\n[Query Plan]: Linear scan on non-indexed field Address\n";
            vector<DoctorRecord> matched;
            for (const auto& d : businessLogic.listAllDoctors()) {
                if (trim(d.address, sizeof(d.address)) == cond.value) matched.push_back(d);
            }
            printDoctorsTable(matched, pq.selectField);
            return true;
        }
    } else if (pq.conditions.size() == 2) {
        string nameVal, specVal;
        bool hasName = false, hasSpec = false;
        for (const auto& c : pq.conditions) {
            if (c.column == "Doctor Name") { nameVal = c.value; hasName = true; }
            else if (c.column == "Doctor Specialty") { specVal = c.value; hasSpec = true; }
        }

        if (hasName && hasSpec) {
            cout << "\n[Query Plan]: Multi-Index Set Intersection (Boolean AND)\n"
                 << "  -> Step 1: Query Doctor Name Secondary Index for '" << nameVal << "'\n";
            vector<string> listA = ie.getFromSecondary(ie.docNameSec, ie.DOC_NAME_INV_FILE, nameVal);
            cout << "     Candidates from Doctor Name inverted list: " << listA.size() << " key(s) [";
            for (size_t i = 0; i < listA.size(); i++) cout << (i > 0 ? ", " : "") << listA[i];
            cout << "]\n  -> Step 2: Query Doctor Specialty Secondary Index for '" << specVal << "'\n";

            vector<string> listB = ie.getFromSecondary(ie.docSpecSec, ie.DOC_SPEC_INV_FILE, specVal);
            cout << "     Candidates from Doctor Specialty inverted list: " << listB.size() << " key(s) [";
            for (size_t i = 0; i < listB.size(); i++) cout << (i > 0 ? ", " : "") << listB[i];
            cout << "]\n";

            sort(listA.begin(), listA.end());
            sort(listB.begin(), listB.end());
            vector<string> common;
            set_intersection(listA.begin(), listA.end(), listB.begin(), listB.end(), back_inserter(common));

            cout << "  -> Step 3: Compute Mathematical Set Intersection (A ∩ B)\n"
                 << "     Common Doctor ID(s): " << common.size() << " key(s) [";
            for (size_t i = 0; i < common.size(); i++) cout << (i > 0 ? ", " : "") << common[i];
            cout << "]\n";

            if (common.empty()) {
                cout << "[Result]: No doctors found matching both criteria.\n";
                return true;
            }

            cout << "  -> Step 4: Retrieve records via Doctor Primary Index Binary Search\n";
            printDoctorsTable(fetchDocs(common), pq.selectField);
            return true;
        }
    }

    cout << "\n[Query Error]: Unsupported query condition.\n";
    return false;
}

bool QueryEngine::executeAppointmentQuery(const ParsedQuery& pq) {
    IndexEngine& ie = businessLogic.getIndexEngine();

    if (!pq.hasWhere) {
        cout << "\n[Query Plan]: Full Table Scan on Appointments\n";
        printAppointmentsTable(businessLogic.listAllAppointments(), pq.selectField);
        return true;
    }

    if (pq.conditions.size() == 1) {
        const auto& cond = pq.conditions[0];
        if (cond.column == "Doctor ID") {
            cout << "\n[Query Plan]: Secondary Index Lookup via Inverted List on Appointments by Doctor ID ('" << cond.value << "')\n";
            vector<string> appIds = ie.getFromSecondary(ie.appDocIdSec, ie.APP_DOCID_INV_FILE, cond.value);
            cout << "[Index Trace]: Found " << appIds.size() << " Appointment(s) referencing Doctor ID '" << cond.value << "'.\n";
            if (appIds.empty()) {
                cout << "[Result]: No appointments found for Doctor ID = '" << cond.value << "'.\n";
                return true;
            }
            vector<AppointmentRecord> apps;
            for (const string& appId : appIds) {
                int offset = ie.binarySearchPrimary(ie.appPrimary, appId);
                if (offset != -1) {
                    AppointmentRecord app;
                    if (StorageEngine::readAppointment(offset, app)) apps.push_back(app);
                }
            }
            printAppointmentsTable(apps, pq.selectField);
            return true;
        } else if (cond.column == "Appointment ID") {
            cout << "\n[Query Plan]: Binary Search on Appointment Primary Index (Key = '" << cond.value << "')\n";
            int offset = ie.binarySearchPrimary(ie.appPrimary, cond.value);
            if (offset == -1) {
                cout << "[Result]: Appointment with ID '" << cond.value << "' not found.\n";
                return true;
            }
            AppointmentRecord app;
            if (StorageEngine::readAppointment(offset, app)) {
                printAppointmentsTable({app}, pq.selectField);
            } else {
                cout << "[Result]: Record at offset " << offset << " is deleted or invalid.\n";
            }
            return true;
        }
    }

    cout << "\n[Query Error]: Unsupported Appointments query condition.\n";
    return false;
}

void QueryEngine::printDoctorsTable(const vector<DoctorRecord>& docs, const string& selectField) {
    if (docs.empty()) {
        cout << "\n[Result]: 0 records returned.\n";
        return;
    }
    string s = toLower(trimString(selectField));
    bool isAll = (s == "all" || s == "*" || s.empty());

    if (isAll) {
        cout << "\n+-----------------+--------------------------------+----------------------+--------------------------------+\n"
             << "| " << left << setw(15) << "Doctor ID" << " | " << setw(30) << "Doctor Name" << " | " << setw(20) << "Specialty" << " | " << setw(30) << "Address" << " |\n"
             << "+-----------------+--------------------------------+----------------------+--------------------------------+\n";
        for (const auto& d : docs) {
            cout << "| " << left << setw(15) << trim(d.doctorID, 15) << " | " << setw(30) << trim(d.doctorName, 30)
                 << " | " << setw(20) << trim(d.specialty, 20) << " | " << setw(30) << trim(d.address, 30) << " |\n";
        }
        cout << "+-----------------+--------------------------------+----------------------+--------------------------------+\n"
             << "Total Records: " << docs.size() << "\n\n";
    } else {
        cout << "\n+--------------------------------+\n"
             << "| " << left << setw(30) << selectField << " |\n"
             << "+--------------------------------+\n";
        for (const auto& d : docs) {
            string val = (s == "doctor name" || s == "name") ? trim(d.doctorName, 30)
                       : (s == "doctor id" || s == "id") ? trim(d.doctorID, 15)
                       : (s == "doctor specialty" || s == "specialty") ? trim(d.specialty, 20)
                       : (s == "address") ? trim(d.address, 30) : "[Unknown Field]";
            cout << "| " << left << setw(30) << val << " |\n";
        }
        cout << "+--------------------------------+\nTotal Records: " << docs.size() << "\n\n";
    }
}

void QueryEngine::printAppointmentsTable(const vector<AppointmentRecord>& apps, const string& selectField) {
    if (apps.empty()) {
        cout << "\n[Result]: 0 records returned.\n";
        return;
    }
    string s = toLower(trimString(selectField));
    bool isAll = (s == "all" || s == "*" || s.empty());

    if (isAll) {
        cout << "\n+-----------------+--------------------------------+-----------------+\n"
             << "| " << left << setw(15) << "Appointment ID" << " | " << setw(30) << "Appointment Date" << " | " << setw(15) << "Doctor ID" << " |\n"
             << "+-----------------+--------------------------------+-----------------+\n";
        for (const auto& a : apps) {
            cout << "| " << left << setw(15) << trim(a.appointmentID, 15) << " | " << setw(30) << trim(a.appointmentDate, 30)
                 << " | " << setw(15) << trim(a.doctorID, 15) << " |\n";
        }
        cout << "+-----------------+--------------------------------+-----------------+\n"
             << "Total Records: " << apps.size() << "\n\n";
    } else {
        cout << "\n+--------------------------------+\n"
             << "| " << left << setw(30) << selectField << " |\n"
             << "+--------------------------------+\n";
        for (const auto& a : apps) {
            string val = (s == "appointment id" || s == "id") ? trim(a.appointmentID, 15)
                       : (s == "appointment date" || s == "date") ? trim(a.appointmentDate, 30)
                       : (s == "doctor id" || s == "doctorid") ? trim(a.doctorID, 15) : "[Unknown Field]";
            cout << "| " << left << setw(30) << val << " |\n";
        }
        cout << "+--------------------------------+\nTotal Records: " << apps.size() << "\n\n";
    }
}
