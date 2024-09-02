#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>


#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

void redirection(Command* cmd, bool hasInput, bool hasOutput){
    if(hasInput){
        //cout << "hasInput" << endl;
        int input_fd = open(cmd->in_file.c_str(),O_RDONLY, 0644);
        if(input_fd == -1){
            perror("OPEN FILE FAILED");
            exit(1);
        }
        dup2(input_fd, STDIN_FILENO);
        close(input_fd);
    }

    if(hasOutput){
        //cout << "hasOutput" << endl;
        int out_fd = open(cmd->out_file.c_str(), O_CREAT|O_WRONLY, 0644);
        if(out_fd == -1){
            perror("OPEN FILE FAILED");
            exit(1);
        }
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);

    }

}

int main () {
    //Copy stdin/stdout of the original process
    int origin_stdin = dup(0);
    int origin_stdout = dup(1);


    //create a vector of pids
    vector<pid_t> background_pids;

    //Storing previous directory
    string prev_dir;
    string curr_dir;
    string change_dir;

    for (;;) {
        //check for background process if any finishes running
        for (size_t i = 0; i < background_pids.size(); i++) {
            int status = 0;
            if (waitpid(background_pids[i], &status, WNOHANG) != 0) {
                background_pids.erase(background_pids.begin() + i);
                cout << "[" << i+1 << "] PID: " << background_pids[i] << " Completed." << endl;
            }
        }
        // need date/time, username, and absolute path to current dir
        cout << YELLOW << "Shell$" << NC << " ";
        
        //get current time
        time_t curtime;
        time(&curtime);
        string timeString = (string)ctime(&curtime);
        //cout << "test4" << endl;
        timeString = timeString.substr(4, timeString.size()-9);
        //cout << timeString << endl;

        //get user
        char* username = getenv("USER");

        //get the current working directory
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        curr_dir = (string)cwd;

        //print the shell prompt
        cout << YELLOW << timeString << BLUE << username << NC << ":" << GREEN << curr_dir << NC << "$";

        // get user inputted command
        string input;
        getline(cin, input);

        //cout << "test1" << endl;
        //cout << input << endl;
        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }
        //check if want to change directory
        else if(input.substr(0,2) == "cd"){
            //cout << "test2" << endl;
            if(input.size() > 2){
                //cout << "test3" << endl;
                //change dir variable to the directory that wants to be change to
                change_dir = input.substr(3, input.size()-3);
                if(change_dir == "-"){//change to previous directory
                    if(prev_dir.empty()){
                        cout << "No previous directory" << endl;
                    }
                    else{
                        if(chdir(prev_dir.c_str()) == -1){
                            perror("Change directory failed");
                            exit(1);
                        }
                        else{
                            prev_dir = curr_dir;
                        }
                    }
                }
                else{//change to new directory
                    if(chdir(change_dir.c_str()) == -1){
                        perror("Change directory failed");
                        exit(1);
                    }
                    else{
                        prev_dir = curr_dir;
                    }
                }
            }
            else{
                //else means its only cd, meaning cd to root
                if(chdir(getenv("HOME")) == -1){
                    perror("Change directory failed");
                    exit(1);
                }
                else{
                    prev_dir = curr_dir;
                }
            }
            continue;
        }


        // get tokenized commands from user input
        //cout << "test5" << endl;
        Tokenizer tknr(input);
        //cout << "tknr" << endl;
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            cerr << "Error parsing command." << endl;
            continue;
        }
        //cout << "test6" << endl;
        vector<Command*> commandsVec = tknr.commands;
        // for(size_t i = 0; i < commandsVec.size(); ++i){
        //     Command* cmd = commandsVec[i];
        //     cout << "Command " << i+1 << ": " << endl;
        //     cout << "Background: " << (cmd->isBackground() ? "Yes" : "No") << endl;
        //     if(cmd->hasInput()){
        //         cout << "input file: " << cmd->in_file << endl;
        //     }
        //     if(cmd->hasOutput()){
        //         cout << "output file: " << cmd->out_file << endl;
        //     }
        //     cout << "Arguments: ";
        //     vector<string> args = cmd->args;
        //     for(const string& arg : args){
        //         cout << arg << " ";
        //     }
        //     cout << endl;

        // }

        // int originSTDIN = STDIN_FILENO;
        //cout << commandsVec.size() << endl;
        for(size_t i = 0; i < commandsVec.size(); i++){
            // fork to create child
            //cout << "for loop" << endl;
            int fd[2];
            pipe(fd);

            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if (pid == 0) {  // if child, exec to run command
                //cout << "Child Process" << endl;
                //getting the commands with flags
                vector<char*> args;
                for(size_t j = 0; j < commandsVec[i]->args.size(); ++j){
                    args.push_back(const_cast<char*>(commandsVec[i]->args[j].c_str()));
                }
                args.push_back(NULL);
                
                //cout << "redirect" << endl;
                //check if needs redirection, and redirects
                if(commandsVec[i]->hasInput() || commandsVec[i]->hasOutput()){
                    redirection(commandsVec[i],commandsVec[i]->hasInput(), commandsVec[i]->hasOutput());
                }

                //cout << "check last command" << endl;
                //redirect stdout to write end of pipe
                if(i < commandsVec.size() - 1){
                    dup2(fd[1], STDOUT_FILENO);
                }

                //run the command
                //cout << args[0] << endl;
                //cout << &args[0] << endl;
                if (execvp(args[0], &args[0]) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
                //cout << "finish execute command" << &args[0] << endl;
            }
            else {  // if parent, wait for child to finish
                //in parent, copy the write read end of the pipe to the standard in of the parent(Shell)
                dup2(fd[0], STDIN_FILENO);
                //close the write end of the pipe so the program knows there is no more things writing in
                close(fd[1]);

                //dealing with background process
                if(commandsVec[i]->isBackground()){
                    background_pids.push_back(pid);
                    for(size_t index = 0; index < background_pids.size(); index++){
                        printf("[%i] %i \n", (int)index+1, background_pids[index]);
                    }
                }
                else{
                    if(i == commandsVec.size()-1){
                        //if it is the last command, wait for the child to finish
                        int status = 0;
                        waitpid(pid, &status, 0);
                        if (status > 1) {  // exit if child didn't exec properly
                            exit(status);
                        }
                    }
                }
            }
        }
        //after finish the last command/child, redirect the stdin and stdout to the original file descriptors
        dup2(origin_stdin, STDIN_FILENO);
        dup2(origin_stdout, STDOUT_FILENO);
    }
}