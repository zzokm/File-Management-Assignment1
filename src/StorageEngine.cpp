#include "StorageEngine.h"
#include <fstream>

using namespace std;

const string StorageEngine::DOCTORS_FILE = "data/doctors.bin";
const string StorageEngine::APPOINTMENTS_FILE = "data/appointments.bin";

static void ensureInit(const string& path) {
    ifstream in(path, ios::binary);
    if (!in.is_open()) {
        ofstream out(path, ios::binary);
        int head = -1;
        out.write((char*)&head, sizeof(int));
    }
}

void StorageEngine::initStorage() {
    ensureInit(DOCTORS_FILE);
    ensureInit(APPOINTMENTS_FILE);
}

int StorageEngine::readAvailHead(const string& path) {
    ensureInit(path);
    ifstream in(path, ios::binary);
    int head = -1;
    in.read((char*)&head, sizeof(int));
    return head;
}

int StorageEngine::allocateSlot(const string& path) {
    int head = readAvailHead(path);
    if (head != -1) {
        int next = -1;
        ifstream in(path, ios::binary);
        in.seekg(head + 1); // skip '*'
        in.read((char*)&next, sizeof(int));
        fstream out(path, ios::binary | ios::in | ios::out);
        out.seekp(0);
        out.write((char*)&next, sizeof(int));
        return head;
    }
    ifstream in(path, ios::binary);
    in.seekg(0, ios::end);
    return (int)in.tellg();
}

void StorageEngine::freeSlot(const string& path, int offset) {
    int oldHead = readAvailHead(path);
    fstream fs(path, ios::binary | ios::in | ios::out);
    fs.seekp(offset);
    char star = '*';
    fs.write(&star, 1);
    fs.write((char*)&oldHead, sizeof(int));
    fs.seekp(0);
    fs.write((char*)&offset, sizeof(int));
}

bool StorageEngine::readDoctor(int offset, DoctorRecord& doc) {
    ifstream in(DOCTORS_FILE, ios::binary);
    in.seekg(offset);
    in.read((char*)&doc, sizeof(DoctorRecord));
    return (in.gcount() == sizeof(DoctorRecord) && doc.doctorID[0] != '*');
}

void StorageEngine::writeDoctor(int offset, const DoctorRecord& doc) {
    fstream fs(DOCTORS_FILE, ios::binary | ios::in | ios::out);
    fs.seekp(offset);
    fs.write((char*)&doc, sizeof(DoctorRecord));
}

bool StorageEngine::readAppointment(int offset, AppointmentRecord& app) {
    ifstream in(APPOINTMENTS_FILE, ios::binary);
    in.seekg(offset);
    in.read((char*)&app, sizeof(AppointmentRecord));
    return (in.gcount() == sizeof(AppointmentRecord) && app.appointmentID[0] != '*');
}

void StorageEngine::writeAppointment(int offset, const AppointmentRecord& app) {
    fstream fs(APPOINTMENTS_FILE, ios::binary | ios::in | ios::out);
    fs.seekp(offset);
    fs.write((char*)&app, sizeof(AppointmentRecord));
}

void StorageEngine::updateDoctorID(int offset, const string& newID) {
    DoctorRecord temp(newID);
    fstream fs(DOCTORS_FILE, ios::binary | ios::in | ios::out);
    fs.seekp(offset);
    fs.write(temp.doctorID, sizeof(temp.doctorID));
}

void StorageEngine::updateDoctorName(int offset, const string& newName) {
    DoctorRecord temp("", newName);
    fstream fs(DOCTORS_FILE, ios::binary | ios::in | ios::out);
    fs.seekp(offset + 15);
    fs.write(temp.doctorName, sizeof(temp.doctorName));
}

void StorageEngine::updateAppointmentDate(int offset, const string& newDate) {
    AppointmentRecord temp("", newDate);
    fstream fs(APPOINTMENTS_FILE, ios::binary | ios::in | ios::out);
    fs.seekp(offset + 15);
    fs.write(temp.appointmentDate, sizeof(temp.appointmentDate));
}
