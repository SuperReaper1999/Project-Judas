#pragma once
#include <locale.h>
#include <string>
// Keep numeric source spelling
// independent of the user's locale without mutating process-global locale or
// changing legacy C-locale float/fingerprint spelling. Both guards are thread local.
class AuthoringNumericLocale {
public:
#ifdef _WIN32
    AuthoringNumericLocale()
        : previous(Current()), previousThreadLocale(_configthreadlocale(_ENABLE_PER_THREAD_LOCALE)) {
        ::setlocale(LC_NUMERIC, "C");
    }
    ~AuthoringNumericLocale(){
        ::setlocale(LC_NUMERIC, previous.c_str());
        _configthreadlocale(previousThreadLocale);
    }
#else
    AuthoringNumericLocale():previous(uselocale(Classic())){}
    ~AuthoringNumericLocale(){uselocale(previous);}
#endif
    AuthoringNumericLocale(const AuthoringNumericLocale&)=delete;
private:
#ifdef _WIN32
    static std::string Current(){
        const char* value=::setlocale(LC_NUMERIC,nullptr);
        return value ? value : "C";
    }
    std::string previous;
    int previousThreadLocale;
#else
    static locale_t Classic(){static locale_t value=newlocale(LC_NUMERIC_MASK,"C",nullptr);return value;}
    locale_t previous;
#endif
};
