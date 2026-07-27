#pragma once

#include <string>
#include <vector>

// Splits a single comma-separated firmware reply line (e.g. from
// "get <id>") into its fields.
std::vector<std::string> splitCsvLine(const std::string &line);
