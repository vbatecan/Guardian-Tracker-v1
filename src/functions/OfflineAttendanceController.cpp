//
// Created by Nytri on 22/09/2024.
//

#include "../../include/OfflineAttendanceController.h"

OfflineAttendanceController::OfflineAttendanceController(const uint8_t pin) {
  SD.begin(pin);
}

/**
 * @brief Read student data from a file.
 *
 * This function reads student data from the specified file and stores it in the given Student object.
 * The file is expected to be in CSV format with two columns: student_id, hashedLrn.
 * The function reads only the first line of the file for demonstration purposes.
 * @param filename The name of the file to read from.
 * @param student The Student object to store the read data in.
 */
void OfflineAttendanceController::readStudentData(const String &filename, Student &student) {
  // Open the file for reading
  openFile(filename, 'r');

  if (_file) {
    while (_file.available()) {
      const String line = _file.readStringUntil('\n');
      const int commaIndex = line.indexOf(',');
      if (commaIndex != -1) {
        // Extract student_id and hashedLrn from the line
        student.student_id = line.substring(0, commaIndex);
        student.hashedLrn = line.substring(commaIndex + 1);
      }
      break; // Read only the first line for demonstration purposes
    }
    _file.close(); // Close the file after reading
  } else {
    Serial.println("Error: Unable to open students.csv for reading.");
  }
}

/**
 * @brief Write attendance data to a file.
 *
 * This function writes attendance data in the form of a CSV line to the specified file.
 * The file is opened in write mode and the data is written to the end of the file.
 * @param filename The name of the file to write to.
 * @param attendance The attendance data to write.
 */
void OfflineAttendanceController::writeAttendanceData(const String &filename, const Attendance &attendance) {
  // Open the file for writing
  openFile(filename, 'a');

  if (_file) {
    // Write attendance data as CSV: student_id, date, time_in, time_out, status
    const String line = attendance.student_id + "," + attendance.date + "," +
                        attendance.time_in + "," + attendance.time_out + "," +
                        attendance.status;
    _file.println(line);
    _file.flush();
    _file.close(); // Close the file after writing
  } else {
    Serial.println("Error: Unable to open attendances.csv for writing.");
  }
}

/**
 * @brief Open a file for reading or writing.
 *
 * This function closes any currently open file and opens the specified file with the given mode.
 * @param filename The name of the file to open.
 * @param mode The mode to open the file in (FILE_READ or FILE_WRITE).
 */
void OfflineAttendanceController::openFile(const String &filename, const char mode) {
  // Close any previously opened file
  if (_file) {
    _file.close();
  }

  // Open the new file with the specified mode
  _file = SD.open(filename, &mode);
}
