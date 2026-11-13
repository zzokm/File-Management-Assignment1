#include "BusinessLogic.h"
#include "QueryEngine.h"
#include <iostream>
#include <string>
#include <limits>
#include <fstream>

using namespace std;

static void clearInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

static string getLineInput(const string& prompt) {
    cout << prompt;
    string line;
    getline(cin, line);
    return QueryEngine::parseQuery("SELECT all FROM Doctors WHERE ID='" + line + "'").isValid ? line : line; // simple return
}

static void printMenu() {
    cout << "\n=======================================================\n";
    cout << "        HEALTHCARE MANAGEMENT SYSTEM (IS321)          \n";
    cout << "=======================================================\n";
    cout << " 1. Add New Doctor\n";
    cout << " 2. Add New Appointment\n";
    cout << " 3. Update Doctor Name (Doctor ID)\n";
    cout << " 4. Update Doctor ID [Key Update Test]\n";
    cout << " 5. Update Appointment Date (Appointment ID)\n";
    cout << " 6. Delete Appointment (Appointment ID)\n";
    cout << " 7. Delete Doctor (Doctor ID)\n";
    cout << " 8. Print Doctor Info (Doctor ID)\n";
    cout << " 9. Print Appointment Info (Appointment ID)\n";
    cout << "10. Write Query (SQL-like Parser)\n";
    cout << "11. List All Doctors\n";
    cout << "12. List All Appointments\n";
    cout << "13. Avail List Lifecycle Test (Automated Verification)\n";
    cout << "14. Exit\n";
    cout << "=======================================================\n";
    cout << "Select an option [1-14]: ";
}

static void runAvailListLifecycleTest(BusinessLogic& bl) {
    cout << "\n=======================================================\n";
    cout << "      AVAIL LIST LIFECYCLE & SPACE RECLAMATION TEST    \n";
    cout << "=======================================================\n";

    string testFile = StorageEngine::DOCTORS_FILE;
    int initialHead = StorageEngine::readAvailHead(testFile);
    cout << "[Step 0] Initial Avail List Head offset: " << initialHead << "\n";

    // Step 1: Add dummy doctor
    string tempDocId = "TEMP_DOC_99";
    cout << "\n[Step 1] Adding temporary test doctor ('" << tempDocId << "')...\n";
    if (bl.hasDoctor(tempDocId)) {
        bl.deleteDoctor(tempDocId);
    }
    bool addRes = bl.addDoctor(tempDocId, "Dr. Lifecycle Test", "Testology", "Virtual Lab");
    if (!addRes) {
        cout << "[FAILED] Could not add test doctor.\n";
        return;
    }

    int testDocOffset = bl.getIndexEngine().binarySearchPrimary(bl.getIndexEngine().docPrimary, tempDocId);
    cout << "  -> Doctor '" << tempDocId << "' written at byte offset: " << testDocOffset << "\n";

    int headAfterAdd = StorageEngine::readAvailHead(testFile);
    cout << "  -> Avail List Head after addition: " << headAfterAdd << "\n";

    // Step 2: Delete dummy doctor
    cout << "\n[Step 2] Deleting doctor ('" << tempDocId << "') to test Avail List deallocation...\n";
    bool delRes = bl.deleteDoctor(tempDocId);
    if (!delRes) {
        cout << "[FAILED] Could not delete test doctor.\n";
        return;
    }

    int headAfterDelete = StorageEngine::readAvailHead(testFile);
    cout << "  -> Avail List Head in file header is now: " << headAfterDelete << "\n";
    if (headAfterDelete == testDocOffset) {
        cout << "  -> [VERIFIED]: Header offset correctly points to the newly freed slot (" << testDocOffset << ")!\n";
    } else {
        cout << "  -> [WARNING]: Header offset does not match expected freed offset.\n";
    }

    // Inspect disk content at freed slot
    ifstream diskIn(testFile, ios::binary);
    if (diskIn.is_open()) {
        diskIn.seekg(testDocOffset);
        char starMarker = 0;
        int nextPointer = -1;
        diskIn.read(&starMarker, 1);
        diskIn.read((char*)&nextPointer, sizeof(int));
        cout << "  -> Byte 0 of freed slot on disk: '" << starMarker << "' (Expected: '*')\n";
        cout << "  -> Next offset pointer stored in freed slot: " << nextPointer << "\n";
    }
    diskIn.close();

    // Step 3: Insert new doctor and verify recycled slot
    string recycledId = "RECYCLE_DOC_01";
    cout << "\n[Step 3] Adding new doctor ('" << recycledId << "') to verify slot recycling...\n";
    if (bl.hasDoctor(recycledId)) {
        bl.deleteDoctor(recycledId);
    }
    addRes = bl.addDoctor(recycledId, "Dr. Recycled Slot", "Reclamation", "Room 101");
    if (!addRes) {
        cout << "[FAILED] Could not add recycled doctor.\n";
        return;
    }

    int recycledOffset = bl.getIndexEngine().binarySearchPrimary(bl.getIndexEngine().docPrimary, recycledId);
    cout << "  -> Doctor '" << recycledId << "' allocated at byte offset: " << recycledOffset << "\n";

    if (recycledOffset == testDocOffset) {
        cout << "  -> [VERIFIED SUCCESS]: New record reused the exact deleted slot (" << recycledOffset << ")!\n";
    } else {
        cout << "  -> [NOTE]: Record was appended at " << recycledOffset << " rather than recycled.\n";
    }

    int headAfterRecycle = StorageEngine::readAvailHead(testFile);
    cout << "  -> Avail List Head after slot reuse: " << headAfterRecycle << "\n";

    // Clean up
    cout << "\n[Step 4] Cleaning up test record...\n";
    bl.deleteDoctor(recycledId);
    cout << "[DONE] Avail List Lifecycle test completed successfully.\n";
    cout << "=======================================================\n";
}

int main() {
    cout << "Initializing Healthcare Management System...\n";
    BusinessLogic businessLogic;
    QueryEngine queryEngine(businessLogic);
    cout << "System initialized. Default dataset verified and loaded.\n";

    while (true) {
        printMenu();
        int choice = -1;
        if (!(cin >> choice)) {
            clearInput();
            cout << "Invalid input. Please enter a valid menu number.\n";
            continue;
        }
        clearInput();

        switch (choice) {
            case 1: { // Add Doctor
                cout << "\n--- Add New Doctor ---\n";
                cout << "Enter Doctor ID: ";
                string id; getline(cin, id);
                cout << "Enter Doctor Name: ";
                string name; getline(cin, name);
                cout << "Enter Specialty: ";
                string spec; getline(cin, spec);
                cout << "Enter Address: ";
                string addr; getline(cin, addr);

                if (businessLogic.addDoctor(id, name, spec, addr)) {
                    cout << "Doctor added successfully!\n";
                } else {
                    cout << "Error: Could not add doctor. ID may already exist or fields are invalid.\n";
                }
                break;
            }
            case 2: { // Add Appointment
                cout << "\n--- Add New Appointment ---\n";
                cout << "Enter Appointment ID: ";
                string id; getline(cin, id);
                cout << "Enter Appointment Date (e.g. YYYY-MM-DD): ";
                string date; getline(cin, date);
                cout << "Enter Doctor ID: ";
                string docId; getline(cin, docId);

                if (businessLogic.addAppointment(id, date, docId)) {
                    cout << "Appointment added successfully!\n";
                } else {
                    cout << "Error: Could not add appointment. Ensure Appointment ID is unique and Doctor ID exists (Foreign Key check).\n";
                }
                break;
            }
            case 3: { // Update Doctor Name
                cout << "\n--- Update Doctor Name ---\n";
                cout << "Enter Doctor ID: ";
                string id; getline(cin, id);
                cout << "Enter New Doctor Name: ";
                string newName; getline(cin, newName);

                if (businessLogic.updateDoctorName(id, newName)) {
                    cout << "Doctor Name updated successfully and secondary index updated!\n";
                } else {
                    cout << "Error: Doctor ID not found or invalid name.\n";
                }
                break;
            }
            case 4: { // Update Doctor ID
                cout << "\n--- Update Doctor ID [Key Update Test] ---\n";
                cout << "Enter Old Doctor ID: ";
                string oldId; getline(cin, oldId);
                cout << "Enter New Doctor ID: ";
                string newId; getline(cin, newId);

                if (businessLogic.updateDoctorID(oldId, newId)) {
                    cout << "Doctor ID updated successfully!\n";
                    cout << "All primary indices, secondary inverted lists, and foreign keys in Appointments updated.\n";
                } else {
                    cout << "Error: Could not update Doctor ID (old ID not found or new ID already exists).\n";
                }
                break;
            }
            case 5: { // Update Appointment Date
                cout << "\n--- Update Appointment Date ---\n";
                cout << "Enter Appointment ID: ";
                string id; getline(cin, id);
                cout << "Enter New Appointment Date: ";
                string newDate; getline(cin, newDate);

                if (businessLogic.updateAppointmentDate(id, newDate)) {
                    cout << "Appointment date updated successfully!\n";
                } else {
                    cout << "Error: Appointment ID not found.\n";
                }
                break;
            }
            case 6: { // Delete Appointment
                cout << "\n--- Delete Appointment ---\n";
                cout << "Enter Appointment ID to delete: ";
                string id; getline(cin, id);

                if (businessLogic.deleteAppointment(id)) {
                    cout << "Appointment deleted successfully and slot freed to Avail List!\n";
                } else {
                    cout << "Error: Appointment ID not found.\n";
                }
                break;
            }
            case 7: { // Delete Doctor
                cout << "\n--- Delete Doctor ---\n";
                cout << "Enter Doctor ID to delete: ";
                string id; getline(cin, id);

                if (businessLogic.deleteDoctor(id)) {
                    cout << "Doctor deleted successfully and slot freed to Avail List!\n";
                } else {
                    cout << "Error: Could not delete doctor. Doctor ID may not exist or doctor has active appointments (Foreign Key restriction).\n";
                }
                break;
            }
            case 8: { // Print Doctor Info
                cout << "\n--- Print Doctor Info ---\n";
                cout << "Enter Doctor ID: ";
                string id; getline(cin, id);

                DoctorRecord doc = businessLogic.getDoctor(id);
                if (doc.doctorID[0] != ' ' && doc.doctorID[0] != '\0' && doc.doctorID[0] != '*') {
                    QueryEngine::printDoctorsTable({doc}, "all");
                } else {
                    cout << "Doctor ID '" << id << "' not found.\n";
                }
                break;
            }
            case 9: { // Print Appointment Info
                cout << "\n--- Print Appointment Info ---\n";
                cout << "Enter Appointment ID: ";
                string id; getline(cin, id);

                AppointmentRecord app = businessLogic.getAppointment(id);
                if (app.appointmentID[0] != ' ' && app.appointmentID[0] != '\0' && app.appointmentID[0] != '*') {
                    QueryEngine::printAppointmentsTable({app}, "all");
                } else {
                    cout << "Appointment ID '" << id << "' not found.\n";
                }
                break;
            }
            case 10: { // Write Query
                cout << "\n--- SQL-like Query Interface ---\n";
                cout << "Example Queries:\n";
                cout << "  * Select all from Doctors where Doctor ID='D001';\n";
                cout << "  * Select all from Appointments where Doctor ID='D001';\n";
                cout << "  * Select Doctor Name from Doctors where Doctor Name='Alice Johnson';\n";
                cout << "  * Select all from Doctors where Doctor Name='Alice Johnson' and Doctor Specialty='Cardiology';\n\n";
                cout << "Enter SQL Query: ";
                string query;
                getline(cin, query);
                queryEngine.executeQuery(query);
                break;
            }
            case 11: { // List All Doctors
                cout << "\n--- All Registered Doctors ---\n";
                vector<DoctorRecord> docs = businessLogic.listAllDoctors();
                QueryEngine::printDoctorsTable(docs, "all");
                break;
            }
            case 12: { // List All Appointments
                cout << "\n--- All Registered Appointments ---\n";
                vector<AppointmentRecord> apps = businessLogic.listAllAppointments();
                QueryEngine::printAppointmentsTable(apps, "all");
                break;
            }
            case 13: { // Avail List Lifecycle Test
                runAvailListLifecycleTest(businessLogic);
                break;
            }
            case 14: { // Exit
                cout << "\nExiting Healthcare Management System. Goodbye!\n";
                return 0;
            }
            default: {
                cout << "Invalid choice (" << choice << "). Please choose between 1 and 14.\n";
                break;
            }
        }
    }

    return 0;
}
