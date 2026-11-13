#pragma once

#include "IndexEngine.h"
#include "StorageEngine.h"
#include <string>
#include <vector>

using namespace std;

class BusinessLogic {
public:
    BusinessLogic();

    bool seed();
    void rebuildIndexes();

    bool hasDoctor(const string& doctorId) const;
    bool hasAppointment(const string& appointmentId) const;

    bool addDoctor(const string& doctorId, const string& doctorName,
                   const string& specialty, const string& address);
    bool deleteDoctor(const string& doctorId);
    bool updateDoctorName(const string& doctorId, const string& newDoctorName);
    bool updateDoctorID(const string& oldDoctorId, const string& newDoctorId);

    bool addAppointment(const string& appointmentId, const string& appointmentDate,
                        const string& doctorId);
    bool deleteAppointment(const string& appointmentId);
    bool updateAppointmentDate(const string& appointmentId, const string& newDate);

    DoctorRecord getDoctor(const string& doctorId) const;
    AppointmentRecord getAppointment(const string& appointmentId) const;
    vector<DoctorRecord> listAllDoctors() const;
    vector<AppointmentRecord> listAllAppointments() const;

    IndexEngine& getIndexEngine() { return indexEngine; }
    const IndexEngine& getIndexEngine() const { return indexEngine; }

private:
    IndexEngine indexEngine;
    bool initialized;

    void loadIndexes();
    static string trimKey(const string& value);
    static long long fileSize(const string& filePath);
    static bool fileExists(const string& filePath);
};
