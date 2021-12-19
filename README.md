# 19fa-project-1-group-11

Name: Nicholas Gomez (ncgomez), Jonah Miller (jonahmil), and Martin Reider (mreider)
Files: penn-sh.h, penn-sh.c, jobs.h, jobs.c Makefile, README.md, tokenizer.c, tokenizer.h
Extra Credit: None

Compiliation Instruction:
1. Get in a directory with all the files.
2. Run "make" or "make all" to create object/executables.
3. To run, put "./penn-sh" in terminal. 
4. Penn Shell is now running. To exit ctrl-D.

Overview of work accomplished:
MS 1: We were able to complete the first milestone for penn-sh. This involved a complete implementation of redirection and piping. Basic shell implementations works such as ls, rm etc. Redirection works as it should. Out redirection works at the end of the pipe. In redirection only works at the begining. If there are no pipes, then redirection for both in and out on a single command. N stage piping works passing inputs/outputs between multiple commands. All invalid inputs are handled and throw errors.

Final: We were able to complete most functionality of terminal and job control. We added a custom linked list that adds and removes jobs in the jobs.c file.  We are successfully bring process from fore ground to back ground and vice versa. We also succesfully differentiate a process being a fg and bg based on the ampersand. FG processes hang when running (until it finishes) and BG processes run in the background and announce when they are finished. Jobs also successfully prints out the LL of all jobs stopped and running.

Description of code and code layout (So far):
The main method holds the implementation of the whole shell. We have helper functions that check input for proper formatting and signal handling. The main method first takes in the input, sets up a matrix of all the arguments for each command. Additionally there is a check to set booleans of redirection. After there is a loop that goes through all the commands and performs the fork and dup2 (if necessary). Each user input essentially creates a job. We create a LL of jobs where each node is a job structure and we are able to keep track of last modified jobs based on a counter field in the job structure. The LL is also ordered. When runnnig the command jobs, the LL of the shell is printed.  Within each job we succesfully determine BG or FG based on &. If there is a BG, it successfully runs in the background and gets checked if finished before the next user prompt. If its a FG, it hangs appropriately until finished or stopped. The bg and fg commands  are also checked after taking in user input and successfully brings processes to fg or bg based on what jobs are available. Note that when we switch between processes, if bg we wait until we reprompt the user and if its a new fg, it hangs until it finished completely.

General comments and anything additional:
