#include <string.h>
#include <stdlib.h> 
#include <stdio.h>

//struct Job;

struct Job {
	int pgid;
	int current_job_number;
	int bool_type; // for background or forground
	int last_modified_counter;
	struct Job * next;
	char * user_input;
	int status; // tell whether its running or stopped
	int num_processes;
	int * pids;
	int * pids_finished; // boolean array list that checks every pid is finished
};


struct Job * create_job(int given_pgid, int ground, int num_commands, int counter, char * input );

void free_job(struct Job * j);

void update_status(struct Job * job, int status, int counter);

void update_ground_type(struct Job * job, int type, int counter);

struct Job * add_job(struct Job * head, struct Job * new_job);

struct Job * remove_job_index(struct Job * head, int num);

void print_job(struct Job* job);


#define TRUE 1
#define FALSE 0
#define FG 1
#define BG 0
#define RUNNING 0
#define STOPPED 1
#define FINISHED 2