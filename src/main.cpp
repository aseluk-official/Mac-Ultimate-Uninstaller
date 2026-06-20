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

bool isRootOwned(const std::string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) == 0)
    {
        return !info.st_uid;
    }
    throw std::runtime_error("File or directory does not exist: " + path);
}

bool canWriteAndModify(const std::string &path)
{
    return (access(path.c_str(), W_OK) == 0);
}

const std::string helpText = "Help text";
int main(){
    std::cout << "=========================================\n";
    std::cout << "    Mac Ultimate Uninstaller CLI v1.0      \n";
    std::cout << "=========================================\n";

    while (true)
    {
        std::string command;
        std::cout << ">";
        std::getline(std::cin, command);

        if (command == "quit")
        {
            return 0;
        }
        else if (command == "help"){
            std::cout << helpText << "\n";
        }
        else if (command == "list"){
            std::string output = runCommand("pkgutil --packages");
            std::cout << output;
        }
        else if (command == "clear"){
            std::cout << std::string(100, '\n');
        }
        else if (command.find("delete ") == 0){
            command.erase(0, 7); // delete the "delete " from the text
            std::string filesCreated = runCommand("pkgutil --files " + command);
            std::cout << filesCreated << "\n";
            //runCommandAsAdmin("pkgutil --forget " + command); NOT DOING IT NOW I DON'T WANNA SCREW MYSELF FOR NOW
        }
        else {
            std::cout << "Type \"help\" for help\n";
        }
    }
}