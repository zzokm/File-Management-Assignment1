#pragma once

#include "BusinessLogic.h"
#include <string>
#include <vector>

using namespace std;

struct QueryCondition {
    string column;
    string value;
};

struct ParsedQuery {
    string selectField; // "all", "*", or specific column name
    string table;       // "doctors" or "appointments"
    vector<QueryCondition> conditions;
    bool hasWhere = false;
    bool isValid = false;
    string errorMessage;
};

class QueryEngine {
public:
    explicit QueryEngine(BusinessLogic& bl);

    // Executes an SQL-like query string and prints results / index trace
    bool executeQuery(const string& queryString);

    // Parses query string into structured representation
    static ParsedQuery parseQuery(const string& queryString);

    // Helper display methods
    static void printDoctorsTable(const vector<DoctorRecord>& docs, const string& selectField = "all");
    static void printAppointmentsTable(const vector<AppointmentRecord>& apps, const string& selectField = "all");

    // String utility helpers
    static string toLower(const string& str);
    static string trimString(const string& str);
    static string extractQuotedValue(const string& str);

private:
    BusinessLogic& businessLogic;

    bool executeDoctorQuery(const ParsedQuery& pq);
    bool executeAppointmentQuery(const ParsedQuery& pq);
};
