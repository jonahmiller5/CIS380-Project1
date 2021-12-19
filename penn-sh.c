#include "penn-sh.h"

// NOTES ABOUT SHELL
// Shell will need to contain a special block of code to send signals from keyboard (^Z, ^C, etc)
// Send S_SIGCONT in fg/bg calls

//global variables
int pid;
int pgid;
struct Job * head;

/*
 Handle signals
*/
void signal_handler(int signo){
	if (signo == SIGINT && pid != 0){
		killpg(pgid, SIGKILL);	
	}
	if (signo == SIGTSTP && pid != 0){
		kill(pid, SIGTSTP);
	}
	write(STDOUT_FILENO, "\npenn-sh>  ", 11);
}

/*
Helper function checking if ampersand is in user input.
*/

int has_ampersand(char* user_input) {
	for (int i = strlen(user_input) - 2; i >= 0; i--) {
		if (user_input[i] == ' ') {
			continue;
		}

		if (user_input[i] == '&') {
			return TRUE;
		} else {
			return FALSE;
		}
	}
	return FALSE;
}

/**
*  Helper function to check if user input is formatted correctly.
*/
char check_input(char * user_input) {
	// first char is not a pipe or redirect or ampersand
	if (user_input[0] == '|' ) {
		return '|';
	}

	if (user_input[0] == '<') {
		return '<';
	}

	if (user_input[0] == '>') {
		return '>';
	}

	if (user_input[0] == '&' ) {
		return '&';
	}

	// last char is not a pipe or redirect
	if (user_input[strlen(user_input) - 2] == '|' || 
		user_input[strlen(user_input) - 2] == '<' || 
		user_input[strlen(user_input) - 2] == '>') {
		return '\n';
	}

	// make sure there are no syntax issues between redirects and pipes
	int num_input = 0;
	int num_output = 0;
	int num_pipes = 0;
	int num_amper = 0;

	char last_seen = '\0';
	for (int i = 0; i < strlen(user_input); i++) {
		if (user_input[i] == '&') {
			if (last_seen == '|' || last_seen == '<' || last_seen == '>' || last_seen == '&') {
				return '&';
			} else if (num_amper > 0) {
				return '&';
			}

			num_amper++;
		} else if (user_input[i] == '|') {
			if (last_seen == '|' || last_seen == '<' || last_seen == '>' || last_seen == '&') {
				return '|';
			}

			num_pipes++;
		} else if (user_input[i] == '<') {
			if (last_seen == '|' || last_seen == '<' || last_seen == '>' || last_seen == '&') {
				return '<';
			} else if (num_input > 0) {
				return '<';
			}

			num_input++;
		} else if (user_input[i] == '>') {
			if (last_seen == '|' || last_seen == '<' || last_seen == '>' || last_seen == '&') {
				return '>';
			} else if (num_output > 0) {
				return '>';
			}

			num_output++;
		}

		last_seen = user_input[i];
	}

	// if there are pipes, make sure > only comes before first pipe and < only comes after last pipe
	if (num_pipes > 0) {
		if (num_input > 0) {
			int first_pipe = 0;
			while (user_input[first_pipe] != '|') {
				first_pipe++;
			}

			int found_input = 0;
			for (int i = 0; i < first_pipe; i++) {
				if (user_input[i] == '<') {
					found_input = 1;
					break;
				}
			}
			
			if (!found_input) {
				return '<';
			}
		}

		if (num_output > 0) {
			int last_pipe = strlen(user_input) - 1;
			while (user_input[last_pipe] != '|') {
				last_pipe--;
			}

			int found_output = 0;
			for (int i = strlen(user_input)-1; i > last_pipe; i--) {
				if (user_input[i] == '>') {
					found_output = 1;
					break;
				}
			}
			
			if (!found_output) {
				return '>';
			}
		}
	}

	if (num_amper > 0) {
		int amper_found = has_ampersand(user_input);
		if (!amper_found) {
			return '&';
		}
	}

	return '\0';
}

/**
*  Main implementation of penn-sh.
*/
int main(int argc, char const *argv[]) {
	int bytes;
	int status;
	int invalid_command;
	int dimension;
	int largest_cell;
	char shell_name[15]= "penn-sh>  ";
	char redirect_failure[45] = "Invalid: syntax error near unexpected token `";
	char user_input[1024];
	char user_input_cpy[1024];
	char* tok;
	TOKENIZER *dimensionizer;
	TOKENIZER *jobs_or_other;
	int amper_found = 0;
	struct Job * current_job = NULL;
	head = NULL;
	int job_counter = 0;

	signal(SIGINT, signal_handler);
	signal(SIGTSTP, signal_handler);
	signal(SIGTTOU, SIG_IGN);
	while (TRUE) {
		memset(user_input, '\0',  1024 * sizeof(char));
		memset(user_input_cpy, '\0',  1024 * sizeof(char));

		// Baclground check
		struct Job * job_count = head;
		while (job_count != NULL) {
			if (job_count->status == STOPPED) {
				struct Job * new_job_loop = job_count->next;
				job_count = new_job_loop;
				continue;
			}
			int num_children = job_count->num_processes;
			int curr_pid;
			for (int i = 0; i < num_children; i ++) {
				status = -1;
				if (job_count->pids_finished[i] == TRUE) {
					continue;
				}
				curr_pid = waitpid(job_count->pids[i], &status, WNOHANG | WUNTRACED);

				// no more active children to read.
				if (curr_pid == -1){
					break;
				}
				if (WIFSTOPPED(status)) {
					// make the job BG, set status stopped
					if (WSTOPSIG(status) != SIGTSTP) {
						job_count->status = STOPPED;
						job_count->last_modified_counter = job_counter;
						job_counter++;
						printf("Stopped: %s\n", current_job->user_input);
						tcsetpgrp(STDOUT_FILENO, getpgid(0));
						break;
					}
					
				}
				
				//if job finished
				if (WIFEXITED(status)){
					job_count->pids_finished[i] = TRUE;
				}
			}
			struct Job * next_job = job_count->next;
			job_count = next_job;
		}

		job_count = head;

		while (job_count != NULL) {
			if (job_count->status == STOPPED || job_count->bool_type == FG){
				job_count = job_count->next;
				continue;
			}
			int all_finished = TRUE;
			for (int i = 0; i < job_count->num_processes; i ++){
				if(job_count->pids_finished[i] == FALSE){
					all_finished = FALSE;
					break;
				}
			}
			if (job_count)
			if (all_finished){
				char * command_tok;
				char command_cpy[1024];
				memset(command_cpy, '\0',  1024 * sizeof(char));
				strcpy(command_cpy, job_count->user_input);
				//printf("command copy:%s\n", command_cpy);
				command_tok = strtok(command_cpy,"&");
				// make sure to trim last element in 
				printf("Finished: %s\n", command_tok);
				head = remove_job_index(head, job_count->current_job_number);
			}
			struct Job * next_job = job_count->next;
			job_count = next_job;
		}

		// Get input
		if (head == NULL){
			job_counter = 0;
		}

		write(STDOUT_FILENO, shell_name, 15);

		bytes = read(STDIN_FILENO, user_input, 1024);

		strcpy(user_input_cpy, user_input);
		
		//hit CTRl D
		if (bytes == 0) {
			if (head != NULL) {
				struct Job * cur_job = head;
				while (cur_job != NULL) {
					// print_job(cur_job);
					struct Job* removal = cur_job;
					cur_job = cur_job->next;
					free_job(removal);
				}
			}
			printf("\n");
			exit(EXIT_SUCCESS);
		}

		//handle 1024
		if (bytes >= 1024 && user_input[1023] != '\n') {
			user_input[1023] = '\0';

			char* clear_buf = malloc(1024);
			int read_clear = read(STDIN_FILENO, clear_buf, 1024);
			while (read_clear > 1024) {
				read_clear = read(STDIN_FILENO, clear_buf, 1024);
			}

			int length = strlen(user_input);

			for (int i = 1024; i < length; i++) {
				user_input[i] = '\0';
			}
		}

		//press just enter
		if (user_input[0] == '\n'){
			continue;
		}
		
		// check whether input has proper syntax
		char valid_input = check_input(user_input);

		if (valid_input != '\0') {
			char out_str[2] = {valid_input, '\0'};

			printf("%s", redirect_failure);
			if (valid_input == '\n') {
				printf("newline'\n");
			} else {
				printf("%s'\n", out_str);
			}

			continue;
		}

		// determine if there is an ampersand
		amper_found = has_ampersand(user_input);

		// check jobs
		jobs_or_other = init_tokenizer(user_input);
		char* j_tok = get_next_token(jobs_or_other);
		if (j_tok == NULL) {
			continue;
		}

		if (strcmp(j_tok, "jobs") == 0) {
			free(j_tok);
			j_tok = get_next_token(jobs_or_other);
			if (j_tok == NULL) {
				struct Job * curr = head;
				while (curr != NULL) {
					char temp[5];
					memset(temp, '\0', sizeof(*temp));
					// figure this out
					if (curr->status == RUNNING) {
						printf("[%d]   %s   (running)\n", curr->current_job_number, curr->user_input);
					}
					else if (curr->status == STOPPED) {
						printf("[%d]   %s   (stopped)\n", curr->current_job_number, curr->user_input);
					}
					curr = curr -> next;
				}
				free(j_tok);
				free_tokenizer(jobs_or_other);
				continue;
			}
		}

		// Check fg
		if (strcmp(j_tok, "fg") == 0) {

			free(j_tok);
			j_tok = get_next_token(jobs_or_other);

			struct Job* traveler = head;
			struct Job* candidate = NULL;

			int is_stopped = 0;

			// argument passed to fg
			if (j_tok != NULL) {
				int fg_arg = atoi(j_tok);
				// if second argument to fg is not an integer, reprompt
				if (fg_arg == 0) {
					printf("Invalid: fg: %s: no such job\n", j_tok);
					free(j_tok);
					free_tokenizer(jobs_or_other);
					continue;
				}

				// find the job as specified in the argument (should be a pgid)
				while (traveler != NULL) {
					if (traveler->current_job_number == fg_arg) {
						candidate = traveler;
						break;
					}
					traveler = traveler->next;
				}

				// job with pgid from argument was not found
				if (candidate == NULL) {
					printf("Invalid: fg: %s: no such job\n", j_tok);
					free(j_tok);
					free_tokenizer(jobs_or_other);
					tcsetpgrp(STDOUT_FILENO, getpgid(0));
					continue;
				}

				if (candidate->status == STOPPED) {
					printf("Restarting: ");
					is_stopped = 0;
				}
				printf("%s\n", candidate->user_input);

			} else { // no argument passed to fg
				// find most recently stopped job, if any jobs are stopped
				int highest_counter = -1;
				while (traveler != NULL) {
					if (traveler->status == STOPPED) {
						if (traveler->last_modified_counter > highest_counter) {
							highest_counter = traveler->last_modified_counter;
							candidate = traveler;
						}
					}
					traveler = traveler->next;
				}
				// if there are no stopped jobs, find most recently modified background job
				if (candidate == NULL) {
					traveler = head;
					while (traveler != NULL) {
						if (traveler->last_modified_counter > highest_counter) {
							highest_counter = traveler->last_modified_counter;
							candidate = traveler;
						}
						traveler = traveler->next;
					}
				} else {
					is_stopped = 1;
				}

				// if there are no background jobs running, fg is invalid
				if (candidate == NULL) {
					printf("Invalid: fg: current: no such job\n");
					free(j_tok);
					free_tokenizer(jobs_or_other);
					tcsetpgrp(STDOUT_FILENO, getpgid(0));
					continue;
				}
				if (is_stopped && candidate->bool_type == BG) {
					printf("Restarting: ");
				}
				printf("%s\n", candidate->user_input);
			}

			// update current job to be the specified job
			current_job = candidate;
			current_job->status = RUNNING;
			current_job->last_modified_counter = job_counter;
			job_counter++;
			pgid = current_job->pgid;

			// give terminal control to the newfound current job
			tcsetpgrp(STDIN_FILENO, current_job->pgid);

			// if job is a stopped job that has been continued, send SIGCONT
			if (is_stopped) {
				killpg(current_job->pgid, SIGCONT);
			}
			int bool_signal = FALSE;
			int fg_finished = TRUE;
			// perform hanging wait on foreground process
			int fg_pid_wait = -1;
			for (int i = 0; i < current_job->num_processes; i++) {
				status = -1;

				fg_pid_wait = waitpid(current_job->pids[i], &status, WUNTRACED);

				if (WIFSIGNALED(status)) {
					// print_job(current_job);
					tcsetpgrp(STDOUT_FILENO, getpgid(0));
					printf("\n");
					bool_signal = TRUE;
					continue;
				}

				if (WIFSTOPPED(status)) {
					// Make job BG
					// Make job stopped
					// print (Stopped)
					// break the loop
					current_job->status = STOPPED;
					current_job->bool_type = BG;

					printf("\nStopped: %s\n", current_job->user_input);
					tcsetpgrp(STDOUT_FILENO, getpgid(0));
					break;
				}

				if (WIFEXITED(status)) {
					// print exited
					// update pid of finishedlist in job struct
					// find index of current pid
					// change the index of finish_pids[index] to TRUE
					current_job->pids_finished[i] = TRUE;
					tcsetpgrp(STDOUT_FILENO, getpgid(0));
				}
			}

			if (bool_signal == TRUE){
				head = remove_job_index(head, current_job->current_job_number);
				current_job = NULL;
				free(j_tok);
				free_tokenizer(jobs_or_other);
				continue;
			}

			if (current_job != NULL) {
				for (int i = 0; i < current_job->num_processes; i++) {
					if (!current_job->pids_finished[i]) {
						fg_finished = FALSE;
						break;
					}
				}
			}
			
			if (fg_finished == TRUE) {
				head = remove_job_index(head, current_job->current_job_number);
			}

			// free memory and reprompt user
			free(j_tok);
			free_tokenizer(jobs_or_other);
			continue;

		} else if (strcmp(j_tok, "bg") == 0) { 
			//bg command
			current_job = NULL;
			free(j_tok);
			//linked list is empty
			if (head == NULL){
				printf("Invalid: No jobs exist.\n");
				free_tokenizer(jobs_or_other);
				continue;
			}
			// check if number specified in next arg
			j_tok = get_next_token(jobs_or_other);
			// no number specified
			if (j_tok == NULL) {
				// go thorugh LL to find most recent stopped job; if none found, return nothing
				struct Job * bg_job = head;
				while (bg_job != NULL){
					if (bg_job->status== STOPPED && bg_job->bool_type == BG){
						if (current_job == NULL){
							current_job = bg_job;
						} else if (current_job->last_modified_counter < bg_job->last_modified_counter){
							current_job = bg_job;
						}
					}
					struct Job * next_job = bg_job->next;
					bg_job = next_job;
					
				}
				if (current_job == NULL){
					printf("Invalid : No jobs that can be ran in bg. BG jobs already running and or no stopped jobs.\n");
					free_tokenizer(jobs_or_other);
					free(j_tok);
					continue;
				} else {
					// set the selected bg stopped process to running
					current_job->status = RUNNING;
					current_job->last_modified_counter = job_counter;
					job_counter ++;
					killpg(current_job->pgid, SIGCONT);
					// print running message without amp
					char * command_tok;
					char command_cpy[1024];
					memset(command_cpy, '\0',  1024 * sizeof(char));
					strcpy(command_cpy, current_job->user_input);
					command_tok = strtok(command_cpy,"&");
					// make sure to trim last element in 
					printf("Running: %s\n", command_tok);
				}

			} else {
				// number specified
				int specific_job = atoi(j_tok);
				if (specific_job == 0){
					//something went wrong with atoi so throw error
					printf("Invalid: Error in bg input\n" );
					free(j_tok);
					free_tokenizer(jobs_or_other);
					continue;
				}
				// go through LL to find matching job number and set it to current job
				// if running in bg say its running; if doesnt exist return nothing or error message
				struct Job * bg_job = head;
				while (bg_job != NULL){
					if (bg_job->current_job_number == specific_job){
						current_job = bg_job;
						//found_bg = TRUE;
						break;
					}
					struct Job * new_bg_job = bg_job->next;
					bg_job = new_bg_job;
				}
				if (bg_job == NULL){
					//nothing found with that job number
					//return error
					printf("Invalid: No stopped job with that number\n" );
					free(j_tok);
					free_tokenizer(jobs_or_other);
					continue;
				}
				if(current_job->status == RUNNING){
					// bg process already running
					printf("Job already running in bg\n" );
					free(j_tok);
					free_tokenizer(jobs_or_other);
					continue;
				}
				if (current_job->status == STOPPED){
					// bg needs tom move from stopped to runnning in BG
					current_job->status = RUNNING;
					current_job->last_modified_counter = job_counter;
					job_counter ++;
					// restart process in bg
					killpg(current_job->pgid, SIGCONT);
					// print running message
					char * command_tok;
					char command_cpy[1024];
					memset(command_cpy, '\0',  1024 * sizeof(char));
					strcpy(command_cpy, current_job->user_input);
					command_tok = strtok(command_cpy,"&");
					// make sure to trim last element in 
					printf("Running: %s\n", command_tok);
				}
				free(j_tok);
				free_tokenizer(jobs_or_other);
				continue;
			}
			free(j_tok);
			free_tokenizer(jobs_or_other);
			continue;
		}
		free(j_tok);
		free_tokenizer(jobs_or_other);

		// make calculations to create commands matrix
		dimensionizer = init_tokenizer(user_input);
		dimension = 1;
		largest_cell = 1;
		int largest_cell_local = 1;
		int largest_string = 0;
		while( (tok = get_next_token(dimensionizer)) != NULL ) {
			largest_cell_local++;
			if (strlen(tok) > largest_string){
				largest_string = strlen(tok);
			}
			if (strcmp(tok, "|") == 0) {
				dimension++;
				if (largest_cell_local > largest_cell) {
					largest_cell = largest_cell_local;
				}
				largest_cell_local = 0;
			}
      		free(tok);
   		 }
   		 free_tokenizer(dimensionizer);


   		 // separate by pipes
   		 char** pipe_split;
   		 pipe_split = calloc(dimension, sizeof(*pipe_split));
   		 char* tok_pipes = strtok(user_input_cpy, "|") ;
   		 int current_pipe_row = 0;
   		 while (tok_pipes != NULL && current_pipe_row < dimension) {
   		 	pipe_split[current_pipe_row] = calloc(1024, sizeof(char));
   		 	strcpy(pipe_split[current_pipe_row], tok_pipes);
   		 	tok_pipes = strtok(NULL,"|");
   		 	current_pipe_row++;
   		 }

		status = -1;
		invalid_command = 0;
		// go through and determine the command (parsing)
		char *** commands;
		commands = calloc(dimension, sizeof(char **));
		int commands_size[dimension];
		int first_in = FALSE;
		int first_out = FALSE;
		int last_out = FALSE;
		// determine redirects on first and lasts
		for(int index = 0; index < dimension; index++){
			char* tok_r;
			if (index == 0 || index == dimension - 1) {
				TOKENIZER* redirect;
				redirect = init_tokenizer(pipe_split[index]);
				//determine type of redirection
				while ((tok_r = get_next_token(redirect)) != NULL) {
					if (strcmp(tok_r, ">") == 0 && index == 0) {
						if (first_out == FALSE) {
							first_out = TRUE;
						} else { // case with more than one >
							// this might be unnecessary
							for (int f = 0; f < dimension; f++) {
								memset(pipe_split[f], '\0', 1024 * sizeof(char));
								free(pipe_split[f]);
							}
							memset(pipe_split, '\0', sizeof(*pipe_split));
					   		free(pipe_split);
					   		struct Job* removal = head;
					   		while (removal != NULL) {
								struct Job* temp = removal;
								removal = removal->next;
								free_job(temp);
							}
							perror("Invalid");
							exit(EXIT_FAILURE);
						}
					}
					if (strcmp(tok_r, ">") == 0 && index == dimension - 1 && dimension > 1) {
						if (last_out == FALSE){
							last_out = TRUE;
						} else { // case with more than one >
							// this might be unnecessary
							for (int f = 0; f < dimension; f++){
								memset(pipe_split[f], '\0', 1024 * sizeof(char));
								free(pipe_split[f]);
							}
							memset(pipe_split, '\0', sizeof(*pipe_split));
					   		free(pipe_split);
					   		struct Job* removal = head;
					   		while (removal != NULL) {
								struct Job* temp = removal;
								removal = removal->next;
								free_job(temp);
							}
							perror("Invalid");
							exit(EXIT_FAILURE);						
						}
					}
				    if (strcmp(tok_r, "<") == 0){
						if (first_in == FALSE){
							first_in = TRUE;
						} else { // case with more than one <
							// this might be unnecessary
							for (int f = 0; f < dimension; f++){
								memset(pipe_split[f], '\0', 1024 * sizeof(char));
								free(pipe_split[f]);
							}
							memset(pipe_split, '\0', sizeof(*pipe_split));
					   		free(pipe_split);
					   		struct Job* removal = head;
					   		while (removal != NULL) {
								struct Job* temp = removal;
								removal = removal->next;
								free_job(temp);
							}
							perror("Invalid");
							exit(EXIT_FAILURE);
						}
					}
					free(tok_r);
	   		 	}	
	   		 	free_tokenizer(redirect);
			}

			// determine commands size
			int argv_size = 0;
   		 	TOKENIZER* command_tokenizer;
   		 	command_tokenizer = init_tokenizer(pipe_split[index]);
   		 	char * tok_c;
   		 	while((tok_c = get_next_token(command_tokenizer)) != NULL && (strcmp(tok_c,"<") != 0 && strcmp(tok_c, ">") != 0 && strcmp(tok_c, "&") != 0)) {
   		 		argv_size++;
   		 		free(tok_c);
   		 	}

   		 	if (tok_c != NULL) {
   		 		free(tok_c);
   		 	}

   		 	argv_size++; // for NULL
   		 	free_tokenizer(command_tokenizer);
   		 	commands_size[index] = argv_size;
   		 	commands[index] = calloc(argv_size, sizeof(char*));
   		 	 //fill commands
   			int current_argv = 0;
   			command_tokenizer = init_tokenizer(pipe_split[index]);
   		 	while((tok_c = get_next_token(command_tokenizer)) != NULL  && (strcmp(tok_c,"<") != 0 && strcmp(tok_c, ">") != 0 && strcmp(tok_c, "&") != 0)) {
   				commands[index][current_argv] = calloc(1, sizeof(char)*strlen(tok_c));
   		 		strcpy(commands[index][current_argv], tok_c);
   		 		current_argv++;
   		 		free(tok_c);
   		 	}

   		 	if (tok_c != NULL) {
   		 		free(tok_c);
   		 	}
   		 	commands[index][argv_size-1] = NULL;
   		 	free_tokenizer(command_tokenizer);
		}

		// creation of pipes
		int pipe_list[dimension-1][2];

		for (int i = 0; i < dimension-1; i++) {
			if (pipe(pipe_list[i]) == -1) {

				for (int k = 0; k < dimension; k++){
					for (int j = 0; j < commands_size[i]; j++) {
						free(commands[k][j]);
					}
				free(commands[k]);
				}

				free(commands);

				for (int f = 0; f < dimension; f++){
					memset(pipe_split[f], '\0', 1024 * sizeof(char));
					free(pipe_split[f]);
				}
				struct Job* removal = head;
		   		while (removal != NULL) {
					struct Job* temp = removal;
					removal = removal->next;
					free_job(temp);
				}
				perror("Invalid");
				exit(EXIT_FAILURE);
			}

		}

		pgid = -1;
		// loop going through individual commands
		for (int i = 0; i < dimension ; i++){
   		 	char out_redirect_file[1024];
   		 	char in_redirect_file[1024];
   		 	memset(out_redirect_file, '\0', 1024 * sizeof(char));
   		 	memset(in_redirect_file, '\0', 1024 * sizeof(char));


   		 	// get in redirect file
			if (first_in == TRUE && i == 0){
				TOKENIZER* in_tokenizer;
				char * tok_ir;
				in_tokenizer = init_tokenizer(pipe_split[i]);
				while ((tok_ir = get_next_token(in_tokenizer)) != NULL){
					if (strcmp(tok_ir, "<") == 0){
						free(tok_ir);
						tok_ir = get_next_token(in_tokenizer);

						if (tok_ir == NULL){
							for (int k = 0; k < dimension; k++){
								for (int j = 0; j < commands_size[i]; j++) {
   		 							free(commands[k][j]);
								}
								free(commands[k]);
   		 					}
   		 					free(commands);
							for (int f = 0; f < dimension; f++){
								memset(pipe_split[f], '\0', 1024 * sizeof(char));
								free(pipe_split[f]);
							}
							memset(pipe_split, '\0', sizeof(*pipe_split));
					   		free(pipe_split);

					   		free(tok_ir);
					   		free_tokenizer(in_tokenizer);
					   		struct Job* removal = head;
					   		while (removal != NULL) {
								struct Job* temp = removal;
								removal = removal->next;
								free_job(temp);
							}

							perror("Invalid");
							exit(EXIT_FAILURE);
						} else {
							// POSSIBLE CHANGE AFTER ACCOMODATING AMPERSAND IMPLEMENTATION
							strcpy(in_redirect_file, tok_ir);
						}
					}
					free(tok_ir);

				}
				free_tokenizer(in_tokenizer);
			}

			// Get out redirect file
			if ((last_out== TRUE && i == dimension - 1)  || (first_out == TRUE && i == 0)) {
				TOKENIZER* out_tokenizer;
				char* tok_or;
				out_tokenizer = init_tokenizer(pipe_split[i]);
				while ((tok_or = get_next_token(out_tokenizer)) != NULL) {
					if (strcmp(tok_or, ">") == 0){
						free(tok_or);
						tok_or = get_next_token(out_tokenizer);
						if (tok_or == NULL){
							for (int k = 0; k < dimension; k++){
								for (int j = 0; j < commands_size[k]; j++) {
   		 							free(commands[k][j]);
								}
								free(commands[k]);
   		 					}
   		 					free(commands);
							for (int f = 0; f < dimension; f++){
								memset(pipe_split[f], '\0', 1024 * sizeof(char));
								free(pipe_split[f]);
							}
							memset(pipe_split, '\0', sizeof(*pipe_split));
					   		free(pipe_split);

					   		free(tok_or);
					   		free_tokenizer(out_tokenizer);
					   		struct Job* removal = head;
					   		while (removal != NULL) {
								struct Job* temp = removal;
								removal = removal->next;
								free_job(temp);
							}

							perror("Invalid");
							exit(EXIT_FAILURE);
						} else {
							// POSSIBLE CHANGE AFTER ACCOMODATING AMPERSAND IMPLEMENTATION
							strcpy(out_redirect_file, tok_or);
						}
					}
					free(tok_or);

				}
				free_tokenizer(out_tokenizer);
				
			}

			// fork
			pid = fork();


			if (pid) {
				if (i == 0) {

 					pgid = pid;
 					strtok(user_input, "\n");
 					strtok(user_input, "&");
 					struct Job * new_job;


 					if (amper_found == 1) {
 						new_job = create_job(pgid, BG, dimension, job_counter, user_input);
 					} else {
 						new_job = create_job(pgid, FG, dimension, job_counter, user_input);
 						tcsetpgrp(STDIN_FILENO, new_job->pgid);
 					}
 					job_counter++;
 					head = add_job(head, new_job);
 					current_job = new_job;
				}
				current_job->pids[i] = pid;
				current_job->pids_finished[i] = FALSE;
				setpgid(pid, pgid);

			}
			

			// redirections
			int new_stdout = -1;
			int new_stdin = -1;
			if (!pid) {
				// child
				if (first_out == TRUE || last_out == TRUE){
					new_stdout = open(out_redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					dup2(new_stdout, STDOUT_FILENO);
				}
				if (first_in == TRUE){
					new_stdin = open(in_redirect_file, O_RDONLY, 0644);
					dup2(new_stdin, STDIN_FILENO);
				}

				//connect pipes
				if (dimension > 1) {
					// first command
					if (i == 0){
						dup2(pipe_list[0][1], STDOUT_FILENO);
						for (int j = 0; j < dimension -1 ; j++){
							close(pipe_list[j][0]);
							close(pipe_list[j][1]);
						}
					} else if (i == dimension-1){ //last command
						dup2(pipe_list[dimension-2][0],STDIN_FILENO);
						for (int j = 0; j < dimension -1 ; j++){
							close(pipe_list[j][0]);
							close(pipe_list[j][1]);
						}
					} else {
						dup2(pipe_list[i][1],STDOUT_FILENO);
						dup2(pipe_list[i-1][0],STDIN_FILENO);
						for (int j = 0; j < dimension -1 ; j++){
							close(pipe_list[j][0]);
							close(pipe_list[j][1]);
						}
					}
							
				}

				invalid_command = execvp(commands[i][0], commands[i]);

				if (invalid_command == -1) {
					perror("Invalid");
					head = remove_job_index(head, current_job->current_job_number);
					for (int k = 0; k < dimension; k++){
						for (int j = 0; j < commands_size[k]; j++) {
   		 					free(commands[k][j]);
						}
						free(commands[k]);
   		 			}
   		 			free(commands);

					for (int f = 0; f < dimension; f++){
						memset(pipe_split[f], '\0', 1024 * sizeof(char));
						free(pipe_split[f]);
					}
					memset(pipe_split, '\0', sizeof(*pipe_split));
			   		free(pipe_split);
					exit(EXIT_FAILURE);
				}
			}
		}

		for (int j = 0; j < dimension -1 ; j++){
			close(pipe_list[j][0]);
			close(pipe_list[j][1]);
		}
		//parent in foreground
		int current_pid_wait;
		int job_finished = 0;
		int bool_stopped = FALSE;

		if (amper_found == FALSE) {
			for (int k = 0; k < dimension; k++) {
				status = -1;
				current_pid_wait = waitpid(current_job->pids[k], &status, WUNTRACED);
				if (WIFSIGNALED(status)) {
					// maybe loop through all children make sure u kill them all? except for the one
					// print_job(current_job);
					tcsetpgrp(STDOUT_FILENO, getpgid(0));

					//head = remove_job_index(head, current_job->current_job_number);
					bool_stopped = TRUE;
					job_finished = 1;
					//current_job = NULL;
					continue;
					//break;
				}
				if (WIFSTOPPED(status)){
					// make the job BG, set status stopped
					current_job->bool_type = BG;
					current_job->status = STOPPED;
					current_job->last_modified_counter = job_counter;
					job_counter++;
					printf("\nStopped: %s\n", current_job->user_input);
					job_finished = 1;
					tcsetpgrp(STDOUT_FILENO, getpgid(0));
					break;
					//remove amp: strtok with &
					//break the loop
				}
				if (WIFEXITED(status)){
					//update pid finishedlist
					current_job->pids_finished[k] = TRUE;
				}
			}
			
			tcsetpgrp(STDOUT_FILENO, getpgid(0));
			if (bool_stopped == TRUE){
				printf("\n");
				head = remove_job_index(head, current_job->current_job_number);
				current_job = NULL;
			}
			// loop through pids_finish to make sure the job is finished completely
			if (current_job != NULL) {
				for (int k = 0; k < dimension; k++) {
					if (current_job->pids_finished[k] == FALSE) {
						job_finished = 1;
					}
				}
			}

			// job is finished -- remove it from linked list
			if (job_finished == 0) {
				head = remove_job_index(head, current_job->current_job_number);
			}
		}

		for (int k = 0; k < dimension ; k++){
			for (int j = 0; j < commands_size[k]; j++) {
				free(commands[k][j]);
			}

			free(commands[k]);
		}
		free(commands);

		for (int f = 0; f < dimension; f++){
			memset(pipe_split[f], '\0', 1024 * sizeof(char));
			free(pipe_split[f]);
		}
		memset(pipe_split, '\0', sizeof(*pipe_split));
   		free(pipe_split);
		
	}
	return 0; 
}