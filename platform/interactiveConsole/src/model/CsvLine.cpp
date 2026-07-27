#include "CsvLine.h"

std::vector<std::string> splitCsvLine(const std::string &line) {
  std::vector<std::string> fields;
  std::string field;

  for (char character : line) {
    if (character == ',') {
      fields.push_back(field);
      field.clear();
      continue;
    }
    field += character;
  }
  fields.push_back(field);

  return fields;
}
