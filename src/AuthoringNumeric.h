#pragma once
#include <locale.h>
// The currently supported platform is Linux. Keep numeric source spelling
// independent of the user's locale without mutating process-global locale or
// changing legacy C-locale float/fingerprint spelling. uselocale is thread local.
class AuthoringNumericLocale {
public:
    AuthoringNumericLocale():previous(uselocale(Classic())){}
    ~AuthoringNumericLocale(){uselocale(previous);}
    AuthoringNumericLocale(const AuthoringNumericLocale&)=delete;
private:
    static locale_t Classic(){static locale_t value=newlocale(LC_NUMERIC_MASK,"C",nullptr);return value;}
    locale_t previous;
};
