#include "BusinessLogic.h"
#include <fstream>
#include <filesystem>

BusinessLogic::BusinessLogic() {
    StorageEngine::initStorage();
    indexEngine.initInvertedFiles();
    indexEngine.rebuild();

    if (indexEngine.docPrimary.empty() && indexEngine.appPrimary.empty()) {
        seed();
    }
}

void BusinessLogic::rebuildIndexes() {
    indexEngine.rebuild();
}

bool BusinessLogic::hasDoctor(const string& doctorId) const {
    string id = trim(doctorId.data(), doctorId.size());
    return indexEngine.binarySearchPrimary(indexEngine.docPrimary, id) != -1;
}

bool BusinessLogic::hasAppointment(const string& appointmentId) const {
    string id = trim(appointmentId.data(), appointmentId.size());
    return indexEngine.binarySearchPrimary(indexEngine.appPrimary, id) != -1;
}

bool BusinessLogic::seed() {
    StorageEngine::initStorage();
    if (!indexEngine.docPrimary.empty()) return true;

    vector<pair<string, vector<string>>> docs = {
        {"D001", {"Alice Johnson", "Cardiology", "Cairo"}},
        {"D002", {"Omar Hassan", "Neurology", "Alexandria"}},
        {"D003", {"Sara Ahmed", "Pediatrics", "Giza"}},
        {"D004", {"Hassan Ali", "Dermatology", "Mansoura"}},
        {"D005", {"Nadia Mostafa", "Orthopedics", "Luxor"}}
    };
    for (const auto& d : docs) {
        addDoctor(d.first, d.second[0], d.second[1], d.second[2]);
    }

    vector<pair<string, vector<string>>> apps = {
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
    for (const auto& a : apps) {
        addAppointment(a.first, a.second[0], a.second[1]);
    }

    return true;
}

bool BusinessLogic::addDoctor(const string& doctorId, const string& doctorName,
                              const string& specialty, const string& address) {
    string id = trim(doctorId.data(), doctorId.size());
    string name = trim(doctorName.data(), doctorName.size());
    string spec = trim(specialty.data(), specialty.size());
    string addr = trim(address.data(), address.size());

    if (id.empty() || name.empty() || spec.empty() || hasDoctor(id)) return false;

    int offset = StorageEngine::allocateSlot(StorageEngine::DOCTORS_FILE);
    DoctorRecord doc(id, name, spec, addr);
    StorageEngine::writeDoctor(offset, doc);

    indexEngine.insertPrimary(indexEngine.docPrimary, id, offset);
    indexEngine.addSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, name, id);
    indexEngine.addSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE, spec, id);
    return true;
}

bool BusinessLogic::deleteDoctor(const string& doctorId) {
    string id = trim(doctorId.data(), doctorId.size());
    int offset = indexEngine.binarySearchPrimary(indexEngine.docPrimary, id);
    if (offset == -1) return false;

    // Foreign key constraint: cannot delete doctor with active appointments
    for (const auto& entry : indexEngine.appPrimary) {
        AppointmentRecord app;
        if (StorageEngine::readAppointment(entry.offset, app)) {
            if (trim(app.doctorID, sizeof(app.doctorID)) == id) return false;
        }
    }

    DoctorRecord doc;
    if (!StorageEngine::readDoctor(offset, doc)) return false;

    StorageEngine::freeSlot(StorageEngine::DOCTORS_FILE, offset);
    indexEngine.deletePrimary(indexEngine.docPrimary, id);
    indexEngine.deleteFromSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE,
                                   trim(doc.doctorName, sizeof(doc.doctorName)), id);
    indexEngine.deleteFromSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE,
                                   trim(doc.specialty, sizeof(doc.specialty)), id);
    return true;
}

bool BusinessLogic::updateDoctorName(const string& doctorId, const string& newDoctorName) {
    string id = trim(doctorId.data(), doctorId.size());
    string newName = trim(newDoctorName.data(), newDoctorName.size());
    if (id.empty() || newName.empty()) return false;

    int offset = indexEngine.binarySearchPrimary(indexEngine.docPrimary, id);
    if (offset == -1) return false;

    DoctorRecord doc;
    if (!StorageEngine::readDoctor(offset, doc)) return false;

    string oldName = trim(doc.doctorName, sizeof(doc.doctorName));
    if (oldName == newName) return true;

    StorageEngine::updateDoctorName(offset, newName);
    indexEngine.deleteFromSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, oldName, id);
    indexEngine.addSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, newName, id);
    return true;
}

bool BusinessLogic::updateDoctorID(const string& oldDoctorId, const string& newDoctorId) {
    string oldId = trim(oldDoctorId.data(), oldDoctorId.size());
    string newId = trim(newDoctorId.data(), newDoctorId.size());
    if (oldId.empty() || newId.empty() || oldId == newId || hasDoctor(newId)) return false;

    int offset = indexEngine.binarySearchPrimary(indexEngine.docPrimary, oldId);
    if (offset == -1) return false;

    DoctorRecord doc;
    if (!StorageEngine::readDoctor(offset, doc)) return false;

    string name = trim(doc.doctorName, sizeof(doc.doctorName));
    string spec = trim(doc.specialty, sizeof(doc.specialty));

    StorageEngine::updateDoctorID(offset, newId);

    // Update Primary Index with array shifting
    indexEngine.updatePrimaryKey(indexEngine.docPrimary, oldId, newId, offset);

    // Update Doctor Secondary Inverted Lists
    indexEngine.deleteFromSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, name, oldId);
    indexEngine.addSecondary(indexEngine.docNameSec, indexEngine.DOC_NAME_INV_FILE, name, newId);
    indexEngine.deleteFromSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE, spec, oldId);
    indexEngine.addSecondary(indexEngine.docSpecSec, indexEngine.DOC_SPEC_INV_FILE, spec, newId);

    // Update referencing Appointments (Foreign Key cascade)
    vector<string> appIds = indexEngine.getFromSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE, oldId);
    for (const string& appId : appIds) {
        int appOffset = indexEngine.binarySearchPrimary(indexEngine.appPrimary, appId);
        if (appOffset == -1) continue;

        AppointmentRecord app;
        if (StorageEngine::readAppointment(appOffset, app)) {
            AppointmentRecord updatedApp(trim(app.appointmentID, sizeof(app.appointmentID)),
                                         trim(app.appointmentDate, sizeof(app.appointmentDate)), newId);
            StorageEngine::writeAppointment(appOffset, updatedApp);
            indexEngine.deleteFromSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE, oldId, appId);
            indexEngine.addSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE, newId, appId);
        }
    }
    return true;
}

bool BusinessLogic::addAppointment(const string& appointmentId, const string& appointmentDate, const string& doctorId) {
    string id = trim(appointmentId.data(), appointmentId.size());
    string date = trim(appointmentDate.data(), appointmentDate.size());
    string docId = trim(doctorId.data(), doctorId.size());

    if (id.empty() || date.empty() || docId.empty()) return false;
    if (!hasDoctor(docId) || hasAppointment(id)) return false;

    int offset = StorageEngine::allocateSlot(StorageEngine::APPOINTMENTS_FILE);
    AppointmentRecord app(id, date, docId);
    StorageEngine::writeAppointment(offset, app);

    indexEngine.insertPrimary(indexEngine.appPrimary, id, offset);
    indexEngine.addSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE, docId, id);
    return true;
}

bool BusinessLogic::deleteAppointment(const string& appointmentId) {
    string id = trim(appointmentId.data(), appointmentId.size());
    int offset = indexEngine.binarySearchPrimary(indexEngine.appPrimary, id);
    if (offset == -1) return false;

    AppointmentRecord app;
    if (!StorageEngine::readAppointment(offset, app)) return false;

    string docId = trim(app.doctorID, sizeof(app.doctorID));
    StorageEngine::freeSlot(StorageEngine::APPOINTMENTS_FILE, offset);
    indexEngine.deletePrimary(indexEngine.appPrimary, id);
    indexEngine.deleteFromSecondary(indexEngine.appDocIdSec, indexEngine.APP_DOCID_INV_FILE, docId, id);
    return true;
}

bool BusinessLogic::updateAppointmentDate(const string& appointmentId, const string& newDate) {
    string id = trim(appointmentId.data(), appointmentId.size());
    string date = trim(newDate.data(), newDate.size());
    if (id.empty() || date.empty()) return false;

    int offset = indexEngine.binarySearchPrimary(indexEngine.appPrimary, id);
    if (offset == -1) return false;

    StorageEngine::updateAppointmentDate(offset, date);
    return true;
}

DoctorRecord BusinessLogic::getDoctor(const string& doctorId) const {
    string id = trim(doctorId.data(), doctorId.size());
    int offset = indexEngine.binarySearchPrimary(indexEngine.docPrimary, id);
    if (offset != -1) {
        DoctorRecord doc;
        if (StorageEngine::readDoctor(offset, doc)) return doc;
    }
    return DoctorRecord();
}

AppointmentRecord BusinessLogic::getAppointment(const string& appointmentId) const {
    string id = trim(appointmentId.data(), appointmentId.size());
    int offset = indexEngine.binarySearchPrimary(indexEngine.appPrimary, id);
    if (offset != -1) {
        AppointmentRecord app;
        if (StorageEngine::readAppointment(offset, app)) return app;
    }
    return AppointmentRecord();
}

vector<DoctorRecord> BusinessLogic::listAllDoctors() const {
    vector<DoctorRecord> docs;
    for (const auto& e : indexEngine.docPrimary) {
        DoctorRecord d;
        if (StorageEngine::readDoctor(e.offset, d)) docs.push_back(d);
    }
    return docs;
}

vector<AppointmentRecord> BusinessLogic::listAllAppointments() const {
    vector<AppointmentRecord> apps;
    for (const auto& e : indexEngine.appPrimary) {
        AppointmentRecord a;
        if (StorageEngine::readAppointment(e.offset, a)) apps.push_back(a);
    }
    return apps;
}
