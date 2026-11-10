#pragma once
#include "Models.h"
#include <string>

using namespace std;

class StorageEngine {
public:
    static const string DOCTORS_FILE;
    static const string APPOINTMENTS_FILE;

    static void initStorage();
    static int readAvailHead(const string& filename);
    static int allocateSlot(const string& filename);
    static void freeSlot(const string& filename, int offset);

    static bool readDoctor(int offset, DoctorRecord& doc);
    static void writeDoctor(int offset, const DoctorRecord& doc);

    static bool readAppointment(int offset, AppointmentRecord& app);
    static void writeAppointment(int offset, const AppointmentRecord& app);

    static void updateDoctorID(int offset, const string& newID);
    static void updateDoctorName(int offset, const string& newName);
    static void updateAppointmentDate(int offset, const string& newDate);
};
