#include "jobs.h"


/*
Create a new job node given the pgid, ground type, num of processes, counter and the user input.
*/
struct Job * create_job(int given_pgid, int ground, int num_commands, int counter, char * input ){
	struct Job * new_job;
	new_job = (struct Job *) malloc(sizeof(struct Job));
	new_job->user_input = malloc(strlen(input) * sizeof(char));
	strcpy(new_job->user_input, input);
	new_job->next = NULL;
	new_job->num_processes = num_commands;
	new_job->bool_type = ground;
	new_job->pgid = given_pgid;
	new_job->last_modified_counter = counter;
	// always starts out running
	new_job->status = RUNNING;
	new_job->pids = malloc(num_commands * sizeof(int));
	new_job->pids_finished = malloc(num_commands * sizeof(int));
	return new_job;
}
/*
Frees a specific job
*/
void free_job(struct Job * j){
	free(j->user_input); // says its invalid
	free(j->pids);
	free(j->pids_finished);
	free(j);
}
/*
Status setter
*/
void update_status(struct Job * job, int status, int counter){
	job->last_modified_counter= counter;
	job->status = status;
}
/*
Ground setter
*/
void update_ground_type(struct Job * job, int type, int counter){
	job->last_modified_counter= counter;
	job->bool_type = type;
}

/*
Adds a new job given the head and the new job node that will be added. Note that
the LL is ordered so LL will look for holes in job count numbers before adding to the end.
*/
struct Job * add_job(struct Job * head, struct Job * new_job){
	//no jobs
	if (head == NULL){
		head = new_job;
		new_job->current_job_number = 1;
		return head;
	}
	// hole at the head
	if (head->current_job_number > 1){
		new_job->current_job_number = 1;
		new_job->next = head;
		return new_job;
	}
	// singleton
	if (head->next == NULL){
		head->next = new_job;
		new_job->current_job_number = 2;
		return head;
	}

	struct Job * current_job = head->next;
    while (current_job->next != NULL && current_job->current_job_number - current_job->next->current_job_number == -1) {

    	current_job = current_job->next;
    }
    // adding to the end
    if (current_job->next == NULL){
    	int new_job_number = current_job->current_job_number + 1;
    	new_job->current_job_number = new_job_number;
    	current_job->next = new_job;
    	new_job->next = NULL;
    	return head;
    }
    //adding to a hole
    if (current_job->current_job_number - current_job->next->current_job_number > 1){
    	new_job->current_job_number = current_job->current_job_number + 1;
    	new_job->next = current_job->next;
    	current_job->next = new_job;
    	return head;
    }

    return head;
}

/*
Removes a job give a specifc job number.
*/
struct Job * remove_job_index(struct Job * head, int num){
	if (num == 1){
		struct Job * new_head = head->next;
		free_job(head);
		return new_head;
	}
	struct Job * current_job = head;

	// check body
	while (current_job->next != NULL){
		if (current_job->next->current_job_number == num){
			struct Job * removed_job = current_job->next;
			current_job->next = current_job->next->next;
			removed_job->next = NULL;
			free_job(removed_job);
			return head;
		}
		current_job = current_job->next;
	}
	return head;

}
/*
Print a specific job with its parameter.
*/
void print_job(struct Job* job) {
	printf("\npgid: %d\n", job->pgid);
	printf("current_job_number: %d\n", job->current_job_number);

	if (job->bool_type == BG) {
		printf("bool_type: BG\n");
	} else if (job->bool_type == FG) {
		printf("bool_type: FG\n");
	} else {
		printf("bool_type:  NOT WORKING\n");
	}

	printf("last_modified_counter: %d\n", job->last_modified_counter);

	if (job->next == NULL) {
		printf("next: NULL\n");
	} else {
		printf("next: non-NULL\n");
	}

	printf("user_input: %s\n", job->user_input);
	
	if (job->status == RUNNING) {
		printf("status: RUNNING\n");
	} else if (job->status == STOPPED) {
		printf("status: STOPPED\n");
	} else {
		printf("status: NOT WORKING\n");
	}

	printf("num_processes: %d\n", job->num_processes);

	printf("pids:\n");
	for (int i = 0; i < job->num_processes; i++) {
		printf("%d\n", job->pids[i]);
	}

	printf("pids_finished:\n");
	for (int i = 0; i < job->num_processes; i++) {
		if (job->pids_finished[i]) {
			printf("TRUE\n");
		} else {
			printf("FALSE\n");
		}
		
	}
	
}
