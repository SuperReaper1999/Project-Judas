#pragma once
#include <string>
#include <vector>
#include <cstddef>
// ICU extended grapheme boundaries in UTF-8 byte offsets. UI selections use
// grapheme indices, so emoji sequences/combining marks are indivisible.
std::vector<size_t> TextGraphemes(const std::string& text);
struct TextSelection {size_t anchor=0,caret=0;};
bool EditText(std::string& text,TextSelection& selection,const std::string& insert,size_t maxBytes);
