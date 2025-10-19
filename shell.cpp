#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>

#include <fcntl.h>

#include "Tokenizer.h"
#include "Command.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main() {
    string prevDir = "";

    while (true) {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));

        const char* user = getenv("USER");
        time_t now = time(0);
        string dt = ctime(&now);
        dt.pop_back(); 

        cout << GREEN << user << " "
             << BLUE << dt << " "
             << YELLOW << cwd << "$ " << NC;

        string input;
        if (!getline(cin, input)) break; 

        if (input == "exit") {
            cout << RED << "Now exiting shell..." << endl
                 << "Goodbye" << NC << endl;
            break;
        }

        Tokenizer tknr(input);
        if (tknr.hasError()) continue;

        if (!tknr.commands.empty() && tknr.commands[0]->args[0] == "cd") {
            string target;
            char cwdBefore[1024];
            getcwd(cwdBefore, sizeof(cwdBefore));

            if (tknr.commands[0]->args.size() == 1)
                target = getenv("HOME");
            else if (tknr.commands[0]->args[1] == "-") {
                target = prevDir.empty() ? cwdBefore : prevDir;
            } else {
                target = tknr.commands[0]->args[1];
            }

            prevDir = cwdBefore;
            if (chdir(target.c_str()) != 0)
                perror("cd failed");
            continue;
        }
        int numCmds = tknr.commands.size();
        int in_fd = 0;
        int default_stdin = dup(STDIN_FILENO);
        int default_stdout = dup(STDOUT_FILENO);

        for (int i = 0; i < numCmds; i++) {
            int fd[2];
            if (i < numCmds - 1) pipe(fd);

            pid_t pid = fork();
            if (pid == 0) {
                if (in_fd != 0) {
                    dup2(in_fd, STDIN_FILENO);
                    close(in_fd);
                }
                if (i < numCmds - 1) {
                    dup2(fd[1], STDOUT_FILENO);
                    close(fd[0]);
                    close(fd[1]);
                }
                if (tknr.commands[i]->hasInput()) {
                    int infd = open(tknr.commands[i]->in_file.c_str(), O_RDONLY);
                    if (infd < 0) {
                        perror("input open failed");
                        exit(1);
                    }
                    dup2(infd, STDIN_FILENO);
                    close(infd);
                }

                if (tknr.commands[i]->hasOutput()) {
                    int outfd = open(tknr.commands[i]->out_file.c_str(),
                                     O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (outfd < 0) {
                        perror("output open failed");
                        exit(1);
                    }
                    dup2(outfd, STDOUT_FILENO);
                    close(outfd);
                }
                vector<string>& args = tknr.commands[i]->args;
                vector<char*> argv;
                for (auto& s : args) argv.push_back((char*)s.c_str());
                argv.push_back(nullptr);

                if (execvp(argv[0], argv.data()) < 0) {
                    perror("exec failed");
                    exit(1);
                }
            }
            else if (pid > 0) {
                if (i < numCmds - 1) {
                    close(fd[1]);
                    in_fd = fd[0];
                }

                if (tknr.commands[i]->isBackground()) {
                    cout << "[background pid " << pid << "]" << endl;
                } else {
                    waitpid(pid, nullptr, 0);
                }
            } else {
                perror("fork failed");
            }
        }
        dup2(default_stdin, STDIN_FILENO);
        dup2(default_stdout, STDOUT_FILENO);
        close(default_stdin);
        close(default_stdout);
    }
    return 0;
}

