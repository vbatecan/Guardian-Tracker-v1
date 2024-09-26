//
// Created by Nytri on 22/09/2024.
//

#ifndef DTO_H
#define DTO_H
#include <WString.h>

#include "enums/CodeStatus.h"

struct MessageDTO {
  String message;
  CodeStatus status;
};

#endif //DTO_H
