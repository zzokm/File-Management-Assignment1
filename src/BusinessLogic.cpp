#include "BusinessLogic.h"

#include <fstream>
#include <utility>

namespace {
long long fileSizeStatic(const string& filePath) {
    ifstream input(filePath, ios::binary | ios::ate);
    if (!input.is_open()) return 0;
    return input.tellg();
}

string normalizeKey(const string& value) {
    string result = value;
    while (!result.empty() && (result.back() == ' ' || result.back() == '\0')) {
        result.pop_back();
    }
    return result;
}

bool recordExistsByPrimary(const vector<PrimaryIndexEntry>& index, const string& key) {
    for (const auto& entry : index) {
        if (entry.key == key) return true;
    }
    return false;
}

vector<PrimaryIndexEntry>::const_iterator findPrimaryEntry(
    const vector<PrimaryIndexEntry>& index, const string& key) {
    for (auto it = index.begin(); it != index.end(); ++it) {
        if (it->key == key) return it;
    }
    return index.end();
}

DoctorRecord buildDoctorRecord(const string& doctorId, const string& doctorName,
                              const string& specialty, const string& address) {
    return DoctorRecord(doctorId, doctorName, specialty, address);
}

AppointmentRecord buildAppointmentRecord(const string& appointmentId,
                                        const string& appointmentDate,
                                        const string& doctorId) {
    return AppointmentRecord(appointmentId, appointmentDate, doctorId);
}

int findAppointmentOffset(const vector<PrimaryIndexEntry>& appointments, const string& id) {
    for (const auto& entry : appointments) {
        if (entry.key == id) return entry.offset;
    }
    return -1;
}

int findDoctorOffset(const vector<PrimaryIndexEntry>& doctors, const string& id) {
    for (const auto& entry : doctors) {
        if (entry.key == id) return entry.offset;
    }
    return -1;
}
}

BusinessLogic::BusinessLogic() : initialized(false) {
    StorageEngine::initStorage();
    indexEngine.initInvertedFiles();
    loadIndexes();

    if (fileSizeStatic(StorageEngine::DOCTORS_FILE) <= 4 &&
        fileSizeStatic(StorageEngine::APPOINTMENTS_FILE) <= 4) {
        seed();
    }

    initialized = true;
}

void BusinessLogic::rebuildIndexes() {
    indexEngine.rebuild();
}

void BusinessLogic::loadIndexes() {
    indexEngine.rebuild();
}

string BusinessLogic::trimKey(const string& value) {
    return normalizeKey(value);
}

long long BusinessLogic::fileSize(const string& filePath) {
    return fileSizeStatic(filePath);
}

bool BusinessLogic::fileExists(const string& filePath) {
    ifstream input(filePath, ios::binary);
    return input.is_open();
}

bool BusinessLogic::seed() {
    StorageEngine::initStorage();
    if (fileSizeStatic(StorageEngine::DOCTORS_FILE) > 4 ||
        fileSizeStatic(StorageEngine::APPOINTMENTS_FILE) > 4) {
        loadIndexes();
        return true;
    }

    vector<pair<string, vector<string>>> doctorData = {
        {"D001", {"Alice Johnson", "Cardiology", "Cairo"}},
        {"D002", {"Omar Hassan", "Neurology", "Alexandria"}},
        {"D003", {"Sara Ahmed", "Pediatrics", "Giza"}},
        {"D004", {"Hassan Ali", "Dermatology", "Mansoura"}},
        {"D005", {"Nadia Mostafa", "Orthopedics", "Luxor"}}
    };

    for (const auto& doctor : doctorData) {
        addDoctor(doctor.first, doctor.second[0], doctor.second[1], doctor.second[2]);
    }

    vector<pair<string, vector<string>>> appointmentData = {
        {"A001", {"2026-11-13", "D001"}},
        {"A002", {"2026-11-15", "D002"}},
        {"A003", {"2026-11-16", "D001"}},
        {"A004", {"2026-11-18", "D003"}},
        {"A005", {"2026-11-20", "D005"}},
        {"A006", {"2026-11-21", "D002"}},
        {"A007", {"2026-11-22", "D004"}},
        {"A008", {"2026-11-23", "D003"}},
        {"A009", {"2026-11-25", "D001"}},
        {"A010", {"2026-11-27", "D005"}}
    };

    for (const auto& appointment : appointmentData) {
        addAppointment(appointment.first, appointment.second[0], appointment.second[1]);
    }

    initialized = true;
    return true;
}

bool BusinessLogic::hasDoctor(const string& doctorId) const {
    return findDoctorOffset(indexEngine.docPrimary, trimKey(doctorId)) != -1;
}

bool BusinessLogic::hasAppointment(const string& appointmentId) const {
    return findAppointmentOffset(indexEngine.appPrimary, trimKey(appointmentId)) != -1;
}

bool BusinessLogic::addDoctor(const string& doctorId, const string& doctorName,
                            const string& specialty, const string& address) {
    const string id = trimKey(doctorId);
    const string name = trimKey(doctorName);
    const string spec = trimKey(specialty);
    const string addr = trimKey(address);

    if (id.empty() || name.empty() || spec.empty()) {
        return false;
    }

    if (hasDoctor(id)) {
        return false;
    }

    int offset = StorageEngine::allocateSlot(StorageEngine::DOCTORS_FILE);
    DoctorRecord record = buildDoctorRecord(id, name, spec, addr);
    StorageEngine::writeDoctor(offset, record);

    indexEngine.insertPrimary(indexEngine.docPrimary, id, offset);
    indexEngine.addSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, name, id);
    indexEngine.addSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE, spec, id);

    return true;
}

bool BusinessLogic::deleteDoctor(const string& doctorId) {
    const string id = trimKey(doctorId);
    int doctorOffset = findDoctorOffset(indexEngine.docPrimary, id);
    if (doctorOffset == -1) return false;

    for (const auto& entry : indexEngine.appPrimary) {
        AppointmentRecord appointment;
        if (StorageEngine::readAppointment(entry.offset, appointment)) {
            if (trimKey(appointment.doctorID) == id) {
                return false;
            }
        }
    }

    DoctorRecord doctorRecord;
    if (!StorageEngine::readDoctor(doctorOffset, doctorRecord)) {
        return false;
    }

    StorageEngine::freeSlot(StorageEngine::DOCTORS_FILE, doctorOffset);
    indexEngine.deletePrimary(indexEngine.docPrimary, id);
    indexEngine.deleteFromSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE,
                                   trimKey(doctorRecord.doctorName), id);
    indexEngine.deleteFromSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE,
                                   trimKey(doctorRecord.specialty), id);

    return true;
}

bool BusinessLogic::updateDoctorName(const string& doctorId, const string& newDoctorName) {
    const string id = trimKey(doctorId);
    const string newName = trimKey(newDoctorName);
    if (id.empty() || newName.empty()) return false;

    int doctorOffset = findDoctorOffset(indexEngine.docPrimary, id);
    if (doctorOffset == -1) return false;

    DoctorRecord doctorRecord;
    if (!StorageEngine::readDoctor(doctorOffset, doctorRecord)) {
        return false;
    }

    const string oldName = trimKey(doctorRecord.doctorName);
    if (oldName == newName) return true;

    DoctorRecord updated = buildDoctorRecord(trimKey(doctorRecord.doctorID),
                                            newName,
                                            trimKey(doctorRecord.specialty),
                                            trimKey(doctorRecord.address));
    StorageEngine::writeDoctor(doctorOffset, updated);

    indexEngine.deleteFromSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE,
                                   oldName, id);
    indexEngine.addSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, newName, id);

    return true;
}

bool BusinessLogic::updateDoctorID(const string& oldDoctorId, const string& newDoctorId) {
    const string oldId = trimKey(oldDoctorId);
    const string newId = trimKey(newDoctorId);
    if (oldId.empty() || newId.empty() || oldId == newId) {
        return false;
    }

    if (hasDoctor(newId)) {
        return false;
    }

    int doctorOffset = findDoctorOffset(indexEngine.docPrimary, oldId);
    if (doctorOffset == -1) return false;

    DoctorRecord doctorRecord;
    if (!StorageEngine::readDoctor(doctorOffset, doctorRecord)) {
        return false;
    }

    const string currentName = trimKey(doctorRecord.doctorName);
    const string currentSpecialty = trimKey(doctorRecord.specialty);
    const string currentAddress = trimKey(doctorRecord.address);

    DoctorRecord updated = buildDoctorRecord(newId, currentName, currentSpecialty, currentAddress);
    StorageEngine::writeDoctor(doctorOffset, updated);

    indexEngine.deletePrimary(indexEngine.docPrimary, oldId);
    indexEngine.deleteFromSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE,
                                   currentName, oldId);
    indexEngine.deleteFromSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE,
                                   currentSpecialty, oldId);

    indexEngine.insertPrimary(indexEngine.docPrimary, newId, doctorOffset);
    indexEngine.addSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, currentName, newId);
    indexEngine.addSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE, currentSpecialty, newId);

    vector<string> appointmentIds = indexEngine.getFromSecondary(indexEngine.appDocIdSec,
                                                               indexEngine.APP_DOCID_INV_FILE,
                                                               oldId);
    for (const string& appointmentId : appointmentIds) {
        int appointmentOffset = findAppointmentOffset(indexEngine.appPrimary, appointmentId);
        if (appointmentOffset == -1) continue;

        AppointmentRecord appointment;
        if (!StorageEngine::readAppointment(appointmentOffset, appointment)) {
            continue;
        }

        AppointmentRecord updatedAppointment = buildAppointmentRecord(
            trimKey(appointment.appointmentID), trimKey(appointment.appointmentDate), newId);
        StorageEngine::writeAppointment(appointmentOffset, updatedAppointment);

        indexEngine.deleteFromSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE,
                                       oldId, appointmentId);
        indexEngine.addSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE, newId,
                                appointmentId);
    }

    return true;
}

bool BusinessLogic::addAppointment(const string& appointmentId, const string& appointmentDate,
                                   const string& doctorId) {
    const string id = trimKey(appointmentId);
    const string date = trimKey(appointmentDate);
    const string docId = trimKey(doctorId);

    if (id.empty() || date.empty() || docId.empty()) {
        return false;
    }

    if (!hasDoctor(docId)) {
        return false;
    }

    if (hasAppointment(id)) {
        return false;
    }

    int offset = StorageEngine::allocateSlot(StorageEngine::APPOINTMENTS_FILE);
    AppointmentRecord record = buildAppointmentRecord(id, date, docId);
    StorageEngine::writeAppointment(offset, record);

    indexEngine.insertPrimary(indexEngine.appPrimary, id, offset);
    indexEngine.addSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE, docId, id);

    return true;
}

bool BusinessLogic::deleteAppointment(const string& appointmentId) {
    const string id = trimKey(appointmentId);
    int appointmentOffset = findAppointmentOffset(indexEngine.appPrimary, id);
    if (appointmentOffset == -1) return false;

    AppointmentRecord record;
    if (!StorageEngine::readAppointment(appointmentOffset, record)) {
        return false;
    }

    const string doctorId = trimKey(record.doctorID);
    StorageEngine::freeSlot(StorageEngine::APPOINTMENTS_FILE, appointmentOffset);
    indexEngine.deletePrimary(indexEngine.appPrimary, id);
    indexEngine.deleteFromSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE,
                                   doctorId, id);

    return true;
}

bool BusinessLogic::updateAppointmentDate(const string& appointmentId, const string& newDate) {
    const string id = trimKey(appointmentId);
    const string newValue = trimKey(newDate);
    if (id.empty() || newValue.empty()) return false;

    int appointmentOffset = findAppointmentOffset(indexEngine.appPrimary, id);
    if (appointmentOffset == -1) return false;

    AppointmentRecord record;
    if (!StorageEngine::readAppointment(appointmentOffset, record)) {
        return false;
    }

    StorageEngine::updateAppointmentDate(appointmentOffset, newValue);
    return true;
}

DoctorRecord BusinessLogic::getDoctor(const string& doctorId) const {
    const string id = trimKey(doctorId);
    for (const auto& entry : indexEngine.docPrimary) {
        if (entry.key == id) {
            DoctorRecord record;
            if (StorageEngine::readDoctor(entry.offset, record)) {
                return record;
            }
            break;
        }
    }
    return DoctorRecord();
}

AppointmentRecord BusinessLogic::getAppointment(const string& appointmentId) const {
    const string id = trimKey(appointmentId);
    for (const auto& entry : indexEngine.appPrimary) {
        if (entry.key == id) {
            AppointmentRecord record;
            if (StorageEngine::readAppointment(entry.offset, record)) {
                return record;
            }
            break;
        }
    }
    return AppointmentRecord();
}

vector<DoctorRecord> BusinessLogic::listAllDoctors() const {
    vector<DoctorRecord> doctors;
    ifstream input(StorageEngine::DOCTORS_FILE, ios::binary);
    if (!input.is_open()) {
        return doctors;
    }

    input.seekg(0, ios::end);
    const long long fileLength = input.tellg();
    const int recordSize = sizeof(DoctorRecord);

    for (int offset = 4; offset + recordSize <= fileLength; offset += recordSize) {
        DoctorRecord doctor;
        if (StorageEngine::readDoctor(offset, doctor)) {
            doctors.push_back(doctor);
        }
    }

    return doctors;
}

vector<AppointmentRecord> BusinessLogic::listAllAppointments() const {
    vector<AppointmentRecord> appointments;
    ifstream input(StorageEngine::APPOINTMENTS_FILE, ios::binary);
    if (!input.is_open()) {
        return appointments;
    }

    input.seekg(0, ios::end);
    const long long fileLength = input.tellg();
    const int recordSize = sizeof(AppointmentRecord);

    for (int offset = 4; offset + recordSize <= fileLength; offset += recordSize) {
        AppointmentRecord appointment;
        if (StorageEngine::readAppointment(offset, appointment)) {
            appointments.push_back(appointment);
        }
    }

    return appointments;
}
