#include "results.hpp"
#include <string>

std::string Results::GiveResultsAsText(){
    return "⏩ " + std::to_string(insideAppBundle) + " files was in a app bundle so they are skipped\n"
    "✅ " + std::to_string(successful) + " successful deletions\n"
    "⚠️  " + std::to_string(notEmpty) + " folders are not empty\n"
    "☢️  " + std::to_string(doesntExist) + " files doesn't exist\n"
    "⛔️ " + std::to_string(prohibitedFolder) + " folders are prohibited\n"
    "❌ " + std::to_string(failed) + " failed deletions\n"
    "❌ " + std::to_string(cantBeDeleted) + " files can't be deleted\n";
}

Results operator+(const Results& left, const Results& right)
{
    Results temp;

    temp.failed = left.failed + right.failed;
    temp.cantBeDeleted = left.cantBeDeleted + right.cantBeDeleted;
    temp.doesntExist = left.doesntExist + right.doesntExist;
    temp.successful = left.successful + right.successful;
    temp.successfulButRisky = left.successfulButRisky + right.successfulButRisky;
    temp.insideAppBundle = left.insideAppBundle + right.insideAppBundle;
    temp.notEmpty = left.notEmpty + right.notEmpty;
    temp.prohibitedFolder = left.prohibitedFolder + right.prohibitedFolder;

    return temp;
}

Results &operator+=(Results& left, const Results& right)
{
    left = left + right;
    return left;
}