#pragma once
#include <string>

struct Results{
    public:
    int failed = 0;
    int cantBeDeleted = 0;
    int doesntExist = 0;
    int successful = 0;
    int successfulButRisky = 0;
    int insideAppBundle = 0;
    int notEmpty = 0;
    int prohibitedFolder = 0;

    std::string GiveResultsAsText();
    friend Results operator+(const Results &left, const Results &right);
    friend Results &operator+=(Results &left, const Results &right);
};