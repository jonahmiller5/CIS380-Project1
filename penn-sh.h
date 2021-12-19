#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h> 
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "tokenizer.h"
#include "jobs.h"


#define TRUE 1
#define FALSE 0
#define FG 1
#define BG 0
#define RUNNING 0
#define STOPPED 1
