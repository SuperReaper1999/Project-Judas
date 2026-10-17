#include "TextEditing.h"
#include "TextLayout.h"
#include <unicode/ubrk.h>
#include <unicode/utext.h>
#include <algorithm>
std::vector<size_t> TextGraphemes(const std::string& text){
 UErrorCode error=U_ZERO_ERROR;UText value=UTEXT_INITIALIZER;utext_openUTF8(&value,text.data(),int64_t(text.size()),&error);
 auto* it=ubrk_open(UBRK_CHARACTER,"",nullptr,0,&error);if(U_SUCCESS(error))ubrk_setUText(it,&value,&error);
 std::vector<size_t> result;if(U_SUCCESS(error))for(int32_t p=ubrk_first(it);p!=UBRK_DONE;p=ubrk_next(it))result.push_back(size_t(p));
 if(it)ubrk_close(it);
 utext_close(&value);if(result.empty())result.push_back(0);return result;
}
bool EditText(std::string& text,TextSelection& selection,const std::string& insert,size_t maxBytes){
 std::string error;if(!ValidTextUTF8(insert,error)||insert.find_first_of("\r\n\0",0,3)!=std::string::npos)return false;
 auto b=TextGraphemes(text);auto a=std::min(selection.anchor,b.size()-1),c=std::min(selection.caret,b.size()-1);auto first=b[std::min(a,c)],last=b[std::max(a,c)];
 if(text.size()-(last-first)+insert.size()>maxBytes)return false;
 text.replace(first,last-first,insert);auto after=TextGraphemes(text);selection.caret=size_t(std::lower_bound(after.begin(),after.end(),first+insert.size())-after.begin());selection.caret=std::min(selection.caret,after.size()-1);selection.anchor=selection.caret;return true;
}
