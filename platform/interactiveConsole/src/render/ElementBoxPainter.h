#pragma once

#include <string>
#include <vector>

// Shared ASCII-art box drawing used by the per-Element-type renderers:
// a bordered rectangle with a centered title on the top edge and a list
// of content lines, colored green when selected or gray when not.
void drawElementBox(
    int top_row,
    int left_col,
    int width,
    int height,
    const std::string &title,
    const std::vector<std::string> &lines,
    bool selected);
