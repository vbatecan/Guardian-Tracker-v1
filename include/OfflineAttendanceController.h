//
// Created by Nytri on 22/09/2024.
//

#ifndef SDCONTROLLER_H
#define SDCONTROLLER_H
#include <SD.h>
#include <WString.h>

struct Student {
  String student_id;
  String hashedLrn;
};

struct Attendance {
  String student_id;
  String date;
  String time_in;
  String time_out;
  String status;
};


class OfflineAttendanceController {
public:
  explicit OfflineAttendanceController(uint8_t pin);
  void readStudentData(const String &filename, Student &student);
  void writeAttendanceData(const String &filename, const Attendance &attendance);
private:
  void openFile(const String &filename, char mode);
  File _file;
};

#endif //SDCONTROLLER_H
