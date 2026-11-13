#include "QueryEngine.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>

using namespace std;

QueryEngine::QueryEngine(BusinessLogic& bl) : businessLogic(bl) {}

string QueryEngine::toLower(const string& str) {
    string res = str;
    for (char& c : res) c = (char)tolower((unsigned char)c);
    return res;
}

string QueryEngine::trimString(const string& str) {
    size_t start = 0;
    while (start < str.size() && (isspace((unsigned char)str[start]) || str[start] == '\0')) start++;
    if (start >= str.size()) return "";
    size_t end = str.size() - 1;
    while (end > start && (isspace((unsigned char)str[end]) || str[end] == '\0')) end--;
    return str.substr(start, end - start + 1);
}

string QueryEngine::extractQuotedValue(const string& str) {
    string trimmed = trimString(str);
    if (trimmed.size() >= 2) {
        if ((trimmed.front() == '\'' && trimmed.back() == '\'') ||
            (trimmed.front() == '"' && trimmed.back() == '"')) {
            return trimmed.substr(1, trimmed.size() - 2);
        }
    }
    return trimmed;
}

static size_t findKeywordOutsideQuotes(const string& str, const string& keyword, size_t startPos = 0) {
    string lowerStr = QueryEngine::toLower(str);
    string lowerKw = QueryEngine::toLower(keyword);
    bool inSingle = false;
    bool inDouble = false;

    for (size_t i = startPos; i < str.size(); i++) {
        if (str[i] == '\'' && !inDouble) inSingle = !inSingle;
        else if (str[i] == '"' && !inSingle) inDouble = !inDouble;
        else if (!inSingle && !inDouble) {
            if (i + lowerKw.size() <= str.size()) {
                if (lowerStr.substr(i, lowerKw.size()) == lowerKw) {
                    return i;
                }
            }
        }
    }
    return string::npos;
}

ParsedQuery QueryEngine::parseQuery(const string& queryString) {
    ParsedQuery pq;
    string q = trimString(queryString);

    if (!q.empty() && q.back() == ';') {
        q.pop_back();
        q = trimString(q);
    }

    if (q.empty()) {
        pq.errorMessage = "Empty query string.";
        return pq;
    }

    if (toLower(q.substr(0, 7)) != "select ") {
        pq.errorMessage = "Query must start with SELECT keyword.";
        return pq;
    }

    size_t fromPos = findKeywordOutsideQuotes(q, " from ", 6);
    if (fromPos == string::npos) {
        pq.errorMessage = "Syntax error: missing FROM clause.";
        return pq;
    }

    string selectPart = trimString(q.substr(7, fromPos - 7));
    pq.selectField = selectPart;

    size_t afterFrom = fromPos + 6; // length of " from "
    size_t wherePos = findKeywordOutsideQuotes(q, " where ", afterFrom);

    string tablePart;
    if (wherePos == string::npos) {
        tablePart = trimString(q.substr(afterFrom));
        pq.hasWhere = false;
    } else {
        tablePart = trimString(q.substr(afterFrom, wherePos - afterFrom));
        pq.hasWhere = true;
    }

    string tableLower = toLower(tablePart);
    if (tableLower == "doctors" || tableLower == "doctor") {
        pq.table = "doctors";
    } else if (tableLower == "appointments" || tableLower == "appointment") {
        pq.table = "appointments";
    } else {
        pq.errorMessage = "Unknown table '" + tablePart + "'. Valid tables: Doctors, Appointments.";
        return pq;
    }

    if (pq.hasWhere) {
        string wherePart = trimString(q.substr(wherePos + 7)); // length of " where "
        if (wherePart.empty()) {
            pq.errorMessage = "Syntax error: empty WHERE clause.";
            return pq;
        }

        // Split where clause by " and "
        vector<string> condTokens;
        size_t current = 0;
        while (current < wherePart.size()) {
            size_t andPos = findKeywordOutsideQuotes(wherePart, " and ", current);
            if (andPos == string::npos) {
                condTokens.push_back(trimString(wherePart.substr(current)));
                break;
            } else {
                condTokens.push_back(trimString(wherePart.substr(current, andPos - current)));
                current = andPos + 5; // length of " and "
            }
        }

        for (const string& condStr : condTokens) {
            size_t eqPos = findKeywordOutsideQuotes(condStr, "=", 0);
            if (eqPos == string::npos) {
                pq.errorMessage = "Syntax error in condition: '" + condStr + "'. Expected format: Column = 'Value'.";
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
                else {
                    pq.errorMessage = "Unknown Doctors column '" + rawCol + "'.";
                    return pq;
                }
            } else {
                if (colLower == "appointment id" || colLower == "appointmentid" || colLower == "id") normCol = "Appointment ID";
                else if (colLower == "appointment date" || colLower == "appointmentdate" || colLower == "date") normCol = "Appointment Date";
                else if (colLower == "doctor id" || colLower == "doctorid") normCol = "Doctor ID";
                else {
                    pq.errorMessage = "Unknown Appointments column '" + rawCol + "'.";
                    return pq;
                }
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

    if (pq.table == "doctors") {
        return executeDoctorQuery(pq);
    } else {
        return executeAppointmentQuery(pq);
    }
}

bool QueryEngine::executeDoctorQuery(const ParsedQuery& pq) {
    IndexEngine& ie = businessLogic.getIndexEngine();

    if (!pq.hasWhere) {
        cout << "\n[Query Plan]: Full Table Scan on Doctors\n";
        vector<DoctorRecord> docs = businessLogic.listAllDoctors();
        printDoctorsTable(docs, pq.selectField);
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
            vector<DoctorRecord> docs;
            for (const string& pk : pks) {
                int offset = ie.binarySearchPrimary(ie.docPrimary, pk);
                if (offset != -1) {
                    DoctorRecord doc;
                    if (StorageEngine::readDoctor(offset, doc)) {
                        docs.push_back(doc);
                    }
                }
            }
            printDoctorsTable(docs, pq.selectField);
            return true;
        } else if (cond.column == "Doctor Specialty") {
            cout << "\n[Query Plan]: Secondary Index Lookup via Inverted List for Doctor Specialty ('" << cond.value << "')\n";
            vector<string> pks = ie.getFromSecondary(ie.docSpecSec, ie.DOC_SPEC_INV_FILE, cond.value);
            cout << "[Index Trace]: Found " << pks.size() << " primary key reference(s) in inverted list.\n";
            if (pks.empty()) {
                cout << "[Result]: No doctors found with Specialty = '" << cond.value << "'.\n";
                return true;
            }
            vector<DoctorRecord> docs;
            for (const string& pk : pks) {
                int offset = ie.binarySearchPrimary(ie.docPrimary, pk);
                if (offset != -1) {
                    DoctorRecord doc;
                    if (StorageEngine::readDoctor(offset, doc)) {
                        docs.push_back(doc);
                    }
                }
            }
            printDoctorsTable(docs, pq.selectField);
            return true;
        } else {
            // Address scan
            cout << "\n[Query Plan]: Linear scan on non-indexed field Address\n";
            vector<DoctorRecord> allDocs = businessLogic.listAllDoctors();
            vector<DoctorRecord> matched;
            for (const auto& d : allDocs) {
                if (trim(d.address, sizeof(d.address)) == cond.value) {
                    matched.push_back(d);
                }
            }
            printDoctorsTable(matched, pq.selectField);
            return true;
        }
    } else if (pq.conditions.size() == 2) {
        // Multi-Index Set Intersection (Boolean AND)
        string nameVal, specVal;
        bool hasName = false, hasSpec = false;

        for (const auto& c : pq.conditions) {
            if (c.column == "Doctor Name") {
                nameVal = c.value;
                hasName = true;
            } else if (c.column == "Doctor Specialty") {
                specVal = c.value;
                hasSpec = true;
            }
        }

        if (hasName && hasSpec) {
            cout << "\n[Query Plan]: Multi-Index Set Intersection (Boolean AND)\n";
            cout << "  -> Step 1: Query Doctor Name Secondary Index for '" << nameVal << "'\n";
            vector<string> listA = ie.getFromSecondary(ie.docNameSec, ie.DOC_NAME_INV_FILE, nameVal);
            cout << "     Candidates from Doctor Name inverted list: " << listA.size() << " key(s) [";
            for (size_t i = 0; i < listA.size(); i++) cout << (i > 0 ? ", " : "") << listA[i];
            cout << "]\n";

            cout << "  -> Step 2: Query Doctor Specialty Secondary Index for '" << specVal << "'\n";
            vector<string> listB = ie.getFromSecondary(ie.docSpecSec, ie.DOC_SPEC_INV_FILE, specVal);
            cout << "     Candidates from Doctor Specialty inverted list: " << listB.size() << " key(s) [";
            for (size_t i = 0; i < listB.size(); i++) cout << (i > 0 ? ", " : "") << listB[i];
            cout << "]\n";

            // Mathematical Set Intersection (A ∩ B)
            sort(listA.begin(), listA.end());
            sort(listB.begin(), listB.end());
            vector<string> intersection;
            set_intersection(listA.begin(), listA.end(), listB.begin(), listB.end(), back_inserter(intersection));

            cout << "  -> Step 3: Compute Mathematical Set Intersection (A ∩ B)\n";
            cout << "     Common Doctor ID(s): " << intersection.size() << " key(s) [";
            for (size_t i = 0; i < intersection.size(); i++) cout << (i > 0 ? ", " : "") << intersection[i];
            cout << "]\n";

            if (intersection.empty()) {
                cout << "[Result]: No doctors found matching both criteria.\n";
                return true;
            }

            cout << "  -> Step 4: Retrieve records via Doctor Primary Index Binary Search\n";
            vector<DoctorRecord> docs;
            for (const string& pk : intersection) {
                int offset = ie.binarySearchPrimary(ie.docPrimary, pk);
                if (offset != -1) {
                    DoctorRecord doc;
                    if (StorageEngine::readDoctor(offset, doc)) {
                        docs.push_back(doc);
                    }
                }
            }
            printDoctorsTable(docs, pq.selectField);
            return true;
        } else {
            cout << "\n[Query Plan]: Compound condition on unsupported combination of columns.\n";
            return false;
        }
    }

    cout << "\n[Query Error]: Unsupported condition count.\n";
    return false;
}

bool QueryEngine::executeAppointmentQuery(const ParsedQuery& pq) {
    IndexEngine& ie = businessLogic.getIndexEngine();

    if (!pq.hasWhere) {
        cout << "\n[Query Plan]: Full Table Scan on Appointments\n";
        vector<AppointmentRecord> apps = businessLogic.listAllAppointments();
        printAppointmentsTable(apps, pq.selectField);
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
                    if (StorageEngine::readAppointment(offset, app)) {
                        apps.push_back(app);
                    }
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

    string sLower = toLower(trimString(selectField));
    bool isAll = (sLower == "all" || sLower == "*" || sLower.empty());

    if (isAll) {
        cout << "\n+-----------------+--------------------------------+----------------------+--------------------------------+\n";
        cout << "| " << left << setw(15) << "Doctor ID"
             << " | " << left << setw(30) << "Doctor Name"
             << " | " << left << setw(20) << "Specialty"
             << " | " << left << setw(30) << "Address" << " |\n";
        cout << "+-----------------+--------------------------------+----------------------+--------------------------------+\n";

        for (const auto& d : docs) {
            cout << "| " << left << setw(15) << trim(d.doctorID, sizeof(d.doctorID))
                 << " | " << left << setw(30) << trim(d.doctorName, sizeof(d.doctorName))
                 << " | " << left << setw(20) << trim(d.specialty, sizeof(d.specialty))
                 << " | " << left << setw(30) << trim(d.address, sizeof(d.address)) << " |\n";
        }
        cout << "+-----------------+--------------------------------+----------------------+--------------------------------+\n";
        cout << "Total Records: " << docs.size() << "\n\n";
    } else {
        // Specific column projected
        cout << "\n+--------------------------------+\n";
        cout << "| " << left << setw(30) << selectField << " |\n";
        cout << "+--------------------------------+\n";

        for (const auto& d : docs) {
            string val;
            if (sLower == "doctor name" || sLower == "name") val = trim(d.doctorName, sizeof(d.doctorName));
            else if (sLower == "doctor id" || sLower == "id") val = trim(d.doctorID, sizeof(d.doctorID));
            else if (sLower == "doctor specialty" || sLower == "specialty") val = trim(d.specialty, sizeof(d.specialty));
            else if (sLower == "address") val = trim(d.address, sizeof(d.address));
            else val = "[Unknown Field]";

            cout << "| " << left << setw(30) << val << " |\n";
        }
        cout << "+--------------------------------+\n";
        cout << "Total Records: " << docs.size() << "\n\n";
    }
}

void QueryEngine::printAppointmentsTable(const vector<AppointmentRecord>& apps, const string& selectField) {
    if (apps.empty()) {
        cout << "\n[Result]: 0 records returned.\n";
        return;
    }

    string sLower = toLower(trimString(selectField));
    bool isAll = (sLower == "all" || sLower == "*" || sLower.empty());

    if (isAll) {
        cout << "\n+-----------------+--------------------------------+-----------------+\n";
        cout << "| " << left << setw(15) << "Appointment ID"
             << " | " << left << setw(30) << "Appointment Date"
             << " | " << left << setw(15) << "Doctor ID" << " |\n";
        cout << "+-----------------+--------------------------------+-----------------+\n";

        for (const auto& a : apps) {
            cout << "| " << left << setw(15) << trim(a.appointmentID, sizeof(a.appointmentID))
                 << " | " << left << setw(30) << trim(a.appointmentDate, sizeof(a.appointmentDate))
                 << " | " << left << setw(15) << trim(a.doctorID, sizeof(a.doctorID)) << " |\n";
        }
        cout << "+-----------------+--------------------------------+-----------------+\n";
        cout << "Total Records: " << apps.size() << "\n\n";
    } else {
        cout << "\n+--------------------------------+\n";
        cout << "| " << left << setw(30) << selectField << " |\n";
        cout << "+--------------------------------+\n";

        for (const auto& a : apps) {
            string val;
            if (sLower == "appointment id" || sLower == "id") val = trim(a.appointmentID, sizeof(a.appointmentID));
            else if (sLower == "appointment date" || sLower == "date") val = trim(a.appointmentDate, sizeof(a.appointmentDate));
            else if (sLower == "doctor id" || sLower == "doctorid") val = trim(a.doctorID, sizeof(a.doctorID));
            else val = "[Unknown Field]";

            cout << "| " << left << setw(30) << val << " |\n";
        }
        cout << "+--------------------------------+\n";
        cout << "Total Records: " << apps.size() << "\n\n";
    }
}
