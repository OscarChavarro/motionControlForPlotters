#pragma once

#include <string>
#include <vector>

// One item drawn on a widget row inside an element box: either a
// clickable/editable control rendered as "[ text ]" (is_widget true), or
// a plain static label with no brackets (is_widget false, e.g. "Steps:"
// next to the input it names). Widgets are highlighted (reverse video)
// only when both the element itself is selected and this particular
// widget has focus, so LEFT/RIGHT can move focus between several widgets
// without highlighting all of them at once.
struct ElementBoxWidget {
  std::string text;
  bool focused;
  bool is_widget = true;
};

// Shared ASCII-art box drawing used by the per-Element-type renderers:
// a bordered rectangle with a centered title on the top edge and a list
// of content lines, colored green when selected or gray when not.
//
// widget_rows draws additional lines below the info lines: each entry is
// one row, and the widgets within a row are drawn side by side (e.g. a
// button next to the text input it controls).
void drawElementBox(
    int top_row,
    int left_col,
    int width,
    int height,
    const std::string &title,
    const std::vector<std::string> &lines,
    bool selected,
    const std::vector<std::vector<ElementBoxWidget>> &widget_rows =
        std::vector<std::vector<ElementBoxWidget>>());
