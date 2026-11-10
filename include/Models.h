#pragma once
#include <cstring>
#include <string>
#include <algorithm>

using namespace std;

#pragma pack(push, 1)
struct DoctorRecord {
    char doctorID[15];
    char doctorName[30];
    char specialty[20];
    char address[30];

    DoctorRecord(const string& id = "", const string& name = "", 
                 const string& spec = "", const string& addr = "") {
        memset(this, ' ', sizeof(DoctorRecord));
        if (!id.empty()) memcpy(doctorID, id.data(), min(id.size(), sizeof(doctorID)));
        if (!name.empty()) memcpy(doctorName, name.data(), min(name.size(), sizeof(doctorName)));
        if (!spec.empty()) memcpy(specialty, spec.data(), min(spec.size(), sizeof(specialty)));
        if (!addr.empty()) memcpy(address, addr.data(), min(addr.size(), sizeof(address)));
    }
};

struct AppointmentRecord {
    char appointmentID[15];
    char appointmentDate[30];
    char doctorID[15];

    AppointmentRecord(const string& id = "", const string& date = "", const string& docId = "") {
        memset(this, ' ', sizeof(AppointmentRecord));
        if (!id.empty()) memcpy(appointmentID, id.data(), min(id.size(), sizeof(appointmentID)));
        if (!date.empty()) memcpy(appointmentDate, date.data(), min(date.size(), sizeof(appointmentDate)));
        if (!docId.empty()) memcpy(doctorID, docId.data(), min(docId.size(), sizeof(doctorID)));
    }
};

struct InvertedNode {
    char primaryKey[15];
    int nextOffset;
};
#pragma pack(pop)

inline string trim(const char* buf, size_t len) {
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\0')) len--;
    return string(buf, len);
}
