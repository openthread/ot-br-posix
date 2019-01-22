#include <regex>
#include <stdio.h>

int main() 
{
    std::string s("/form_network");
    std::regex r("^/form_network$", std::regex::extended);
    std::smatch match;
    bool res = std::regex_match(s, match, r);
    printf("Regex match gives res %d\n", res);
    return !res;
}
