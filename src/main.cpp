#include <iostream>
#include <memory>
#include <cstdio>
#include <stdexcept>
#include <array>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <ranges>
#include <set>
#include <regex>

namespace fs = std::filesystem;

std::vector<std::string> SplitString(std::string& text, char delimiter, std::vector<std::string>& output)
{
    output.clear();
    std::string token;
    std::stringstream stream(text);

    while (std::getline(stream, token, delimiter)){
        output.push_back(token);
    }
    return output;
}

std::vector<std::string> RemoveEmptyItems(std::vector<std::string>& list){
    for(int i = list.size() - 1; i >= 0; --i){
        if (list[i].empty()) list.erase(list.begin() + i);
    }
    return list;
}

std::string runCommand(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    
    if (!pipe) {
        throw std::runtime_error("popen() failed to open the pipe.");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

std::string runCommandAsAdmin(const std::string& cmd){
    std::string finalCmd = "osascript -e 'do shell script \"" + cmd + " 2>&1\" with administrator privileges'";
    std::string result = runCommand(finalCmd);
    std::replace(result.begin(), result.end(), '\r', '\n');

    return result;
}

bool isRootOwned(const fs::path& path)
{
    struct stat info;
    if (stat(path.string().c_str(), &info) == 0)
    {
        return !info.st_uid;
    }
    throw std::runtime_error("File or directory does not exist: " + path.string());
}

bool canDelete(const fs::path &path)
{
    fs::file_status s = fs::status(path);
    fs::perms p = s.permissions();

    if ((p & fs::perms::owner_write) != fs::perms::none) return true;
    return false;
}

fs::path GetPackageParentDirectory(const std::string &packageName)
{
    static std::vector<std::string> output;

    std::string volume;
    std::string location;

    std::string textOutput = runCommand("pkgutil --info " + packageName);

    SplitString(textOutput, '\n', output);
    RemoveEmptyItems(output);

    volume = output[2];
    volume.erase(0, static_cast<std::string>("volume: ").length());

    location = output[3];
    location.erase(0, static_cast<std::string>("location: ").length());
    location = location == "/" ? "" : location;

    return static_cast<fs::path>(volume + location);
}

const std::set<fs::path>& getProhibitedFolders(){
    static std::set<fs::path> prohibitedFolders = {"Applications", "/Library", "~/Library", "/Users", "~", "/System", "/ Library", "/bin", "/sbin", "/usr", "~/Desktop", "~/Documents", "~/Downloads", "/private", "/etc", "/var", "/tmp", "/Volumes", "/Library/Audio/Plug-Ins/Components", "~/Library/Audio/Plug-Ins/Components", "/Library/Audio/Plug-Ins/VST", "~/Library/Audio/Plug-Ins/VST", "/Library/Audio/Plug-Ins/VST3", "~/Library/Audio/Plug-Ins/VST3", "/Library/Application Support/Avid/Audio/Plug-Ins", "~/Library/Application Support/Avid/Audio/Plug-Ins", "/Library/Audio/Plug-Ins/CLAP", "~/Library/Audio/Plug-Ins/CLAP"}; // "~" means user folder   
    static bool initialized = false;

    if (!initialized){
        const char *homeDirAsChar = std::getenv("HOME");

        if (!homeDirAsChar) throw std::runtime_error("Can't get the user folder");

        std::string homeDir(homeDirAsChar);
        std::set<fs::path> updatedFolders;

        for (const auto &i : prohibitedFolders)
        {
            std::string updatedPath = std::regex_replace(i.string(), std::regex("~"), homeDir);
            updatedFolders.insert(updatedPath);
        }
        prohibitedFolders = std::move(updatedFolders);

        initialized = true;
    }
    return prohibitedFolders;
}

bool isAProhibitedFolder(fs::path path)
{
    const auto& prohibitedFolders =  getProhibitedFolders();

    if (path == path.root_path()) return true;
    return prohibitedFolders.count(path);
}

bool isAnBundleFolder(fs::path path){
    const static std::set<std::string> appBundleExtensions = {".app", ".vst3", ".vst", ".component", ".aaxplugin"}; // more extensions might be added

    return appBundleExtensions.count(path.extension());
}
bool insideBundleFolder(fs::path path){
    auto current = path.parent_path();
    while (current != current.root_path() && !current.empty()){
        if (isAnBundleFolder(current)){
            return true;
        }
        current = current.parent_path();
    }

    return false;
}
void deletePackage(const std::string& packageName)
{
    static std::vector<std::string> packagePaths;

    int failed = 0;
    int cantBeDeleted = 0;
    int doesntExist = 0;
    int successful = 0;
    int successfulButRisky = 0;
    int insideAppBundle = 0;
    int notEmpty = 0;
    int prohibitedFolder = 0;

    fs::path parentDirectory = GetPackageParentDirectory(packageName);

    std::string filesCreated = runCommand("pkgutil --files " + packageName);
    SplitString(filesCreated, '\n', packagePaths);
    for (const auto& i : packagePaths | std::views::reverse){
        fs::path path = i;
        path = parentDirectory / path;

        std::error_code ec;
        if (!fs::exists(path, ec))
        {
            if (ec){
                std::cout << "❌ " << path << " Failed" << ec.message() << "\n";
                ++failed;
                continue;
            }
            std::cout << "☢️  " << path << " Doesn't Exist\n";
            ++doesntExist;
            continue;
        }
        if (isAProhibitedFolder(path))
        {
            std::cout << "⛔️ " << path << " is a prohibited folder\n";
            ++prohibitedFolder;
            continue;
        }
        if (insideBundleFolder(path))
        {
            std::cout << "⏩ " << path << " is inside an app bundle so it is skipped\n";
            ++insideAppBundle;
            continue;
        }
        if (canDelete(path))
        {
            if (isRootOwned(path) && false){
                std::cout << "⚠️  " << path << " Is risky to delete\n";
                ++successfulButRisky;
            }
            else{
                if (fs::is_directory(path) && !isAnBundleFolder(path) && !fs::is_empty(path))
                {
                    std::cout << "⚠️  " << path << " Is not empty\n";
                    ++notEmpty;
                }
                else{
                    std::cout << "✅ " << path << " Is deleted\n";
                    if (isAnBundleFolder(path)){
                        fs::remove_all(path);
                    }
                    else{
                        fs::remove(path);
                    }
                    ++successful;
                }
            }
        }
        else {
            std::cout << "❌ " << path << " Can't be deleted\n";
            ++cantBeDeleted;
        }
    }
    std::cout << "\n\n⏩ " << insideAppBundle << " files was in a app bundle so they are skipped\n";
    std::cout << "✅ " << successful << " successful deletions\n";
    std::cout << "⚠️  " << notEmpty << " folders are not empty\n";
    //std::cout << "⚠️  " << successfulButRisky << " risky deletions\n";
    std::cout << "☢️  " << doesntExist << " files doesn't exist\n";
    std::cout << "⛔️ " << prohibitedFolder << " folders are prohibited\n";
    std::cout << "❌ " << failed << " failed deletions\n";
    std::cout << "❌ " << cantBeDeleted << " files can't be deleted\n";

    runCommandAsAdmin("pkgutil --forget " + packageName);
}

const std::string helpText = "-------HELP-------\n"
"quit: quits the program\n"
"help: gives you this help text\n"
"list: lists all of the programs\n"
"search <program>: searches a program\n"
"clear: clears the console\n"
"delete <program>: deletes a program\n"
"delete-multiple <program1>, <program2>...: deletes multiple programs\n"
"-------HELP-------\n";

const std::string emptyInputHint = "Type \"help\" for help\n";
const std::string doesNotTakeParam = " command does not take a parameter\n";
const std::string doesTakeParam = " command needs a parameter\n";
const std::string doesTakeMultipleParam = " command needs multiple parameters\n";
const std::string doesTakeParams = " command needs parameters\n";
const std::string doesTakeOneParam = " command only takes one parameter\n";
int main(){
    if (getuid()){
        std::cout << "Needs sudo\n";
        return 1;
    }

    std::cout << "=========================================\n";
    std::cout << "    Mac Ultimate Uninstaller CLI v1.0      \n";
    std::cout << "=========================================\n";

    std::string pureCommand;
    std::vector<std::string> command;

    while (true)
    {
        std::cout << ">";
        std::getline(std::cin, pureCommand);

        SplitString(pureCommand, ' ', command);
        RemoveEmptyItems(command);
        if (command.empty()){
            std::cout << emptyInputHint;
            continue;
        }

        if (command[0] == "quit")
        {
            if (command.size() > 1){
                std::cout << "quit" << doesNotTakeParam;
                continue;
            }
            break;
        }
        else if (command[0] == "help")
        {
            if (command.size() > 1)
            {
                std::cout << "help" << doesNotTakeParam;
                continue;
            }
            std::cout << helpText;
        }
        else if (command[0] == "list")
        {
            if (command.size() > 1)
            {
                std::cout << "list" << doesNotTakeParam;
                continue;
            }
            std::string output = runCommand("pkgutil --packages");
            std::cout << output;
        }
        else if (command[0] == "search")
        {
            if (command.size() == 1){
                std::cout << "search" << doesTakeParam;
                continue;
            } 
            else if (command.size() > 2){
                std::cout << "search" << doesTakeOneParam;
                continue;
            }
            std::string output = runCommand("pkgutil --packages | grep -i \"" + command[1] + "\"");
            std::cout << output;
        }
        else if (command[0] == "clear"){
            if (command.size() > 1)
            {
                std::cout << "clear" << doesNotTakeParam;
                continue;
            }
            std::cout << std::string(100, '\n');
        }
        else if (command[0] == "delete"){
            if (command.size() == 1){
                std::cout << "delete" << doesTakeParam;
                continue;
            }
            else if (command.size() > 2){
                std::cout << "delete" << doesTakeOneParam;
                continue;
            }
            deletePackage(command[1]);
        }
        else if (command[0] == "delete-multiple")
        {
            if (command.size() == 1 || command.size() == 2)
            {
                std::cout << "delete-multiple" << doesTakeMultipleParam;
                continue;
            }
            for (int32_t i = 1; i < command.size(); ++i)
                deletePackage(command[i]);
        }
        else {
            std::cout << emptyInputHint;
        }
    }
    return 0;
}