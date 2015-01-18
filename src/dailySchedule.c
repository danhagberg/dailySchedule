//TODO: Change strncpy functions to strdup functions.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include "schedule.h"
#include "schedule.tab.h"

#define SUCCESS 0
#define ERROR 1
// Max length of entry from file including null term.  Must allow for
// MAX_TASK_LEN + MAX_REMINDER_LEN + other fields.
#define MAX_SCHED_ENTRY_LEN 400
// Max length of task entry *not* including null term
#define MAX_TASK_LEN 40
// Max length of reminder text *not* including null term
#define MAX_REMINDER_LEN 300
// Max length of reminder command *not* including null term
#define MAX_CMD_LEN 300
// Max length of schedule file location *not* including null term
#define MAX_FILE_LOC_LEN 300
// Wildcard value for schedule field
#define SCHED_WILDCARD -1

#define ERR_FILE stdout

/* -----------------------------------------------------------------------------
 *  Internal Structures
 * ---------------------------------------------------------------------------*/


/**
 * Schedule file variables
 */
#define NUMBER_OF_TOKENS 9
enum TokenIndex {TI_YEAR, TI_MOY, TI_DOM, TI_DOW, TI_HOUR, TI_MIN, TI_DUR, TI_TASK, TI_REMINDER};

static time_t debugTime = 0;

/* -----------------------------------------------------------------------------
 *  Prototypes
 * ---------------------------------------------------------------------------*/
void usage();
void getFileLoc(const char *fileLoc);
int processArgs(int argc, char **argv);

int processScheduleFile(const char * fileName);

/* -----------------------------------------------------------------------------
 *  Arg Processing.
 * ---------------------------------------------------------------------------*/
enum ActionVals {PRINT=1, TODAY=2, NOTIFY=4};
int actions = 0;
char *scheduleFileLoc = NULL;


/* -----------------------------------------------------------------------------
 *  Function definitions.
 * ---------------------------------------------------------------------------*/
int main(int argc, char **argv) {
	int status;
	status = processArgs(argc, argv);
	if (status == ERROR) {
		usage();
		return(status);
	}
	status = processScheduleFile(scheduleFileLoc);
	if (status == ERROR) {
		return(status);
	}
	if (actions & PRINT) {
     	displaySchedule(stdout);
	}
	if (actions & TODAY) {
    	displayTodaysSchedule(stdout);
	}
	if (actions & NOTIFY) {
    	runNotifications();
	}
	// TODO: release all memory
	return SUCCESS;
}

int processArgs(int argc, char **argv) {
	int i;
	for (i=1; i < argc; i++) {
		switch (argv[i][0]) {
		case '-':
    		switch (argv[i][1]) {
    		case 'p':
				actions |= PRINT;
				break;
    		case 't':
    			actions |= TODAY;
    			break;
    		case 'n':
    			actions |= NOTIFY;
    			break;
    		case 'f':
    			getFileLoc(argv[++i]);
    			break;
    		default: return ERROR;
    		}
    		break;
		default: return ERROR;
		}
	}
	// If no file provided or no action specified, return error.
	// TODO: Provide better message.
	if (scheduleFileLoc == NULL || actions == 0) {
		return ERROR;
	}
	return SUCCESS;
}

void getFileLoc(const char *fileLoc) {
	int nameLen;
	if (fileLoc == NULL) {
		return;
	}
	// nameLen = strnlen(fileLoc, MAX_FILE_LOC_LEN) + 1;
	nameLen = strlen(fileLoc) + 1;
	// TODO:  Should check to see if we truncated it.
	if (nameLen == 1) {
		return;
	}
	scheduleFileLoc = (char*)malloc(sizeof(char)*nameLen);
	assert(scheduleFileLoc != NULL);
	memset(scheduleFileLoc, 0, nameLen);
	strncpy(scheduleFileLoc, fileLoc, nameLen);
}

void usage() {
	printf("Usage:  schedule [-n] [-p] [-t] -f <file path>\n");
}

int processScheduleFile(const char * fileName) {
    extern FILE *yyin;
	yyin = fopen(fileName, "r");
    if(yyin == NULL) {
    	perror("Failed to open input file: ");
    	return ERROR;
    }
    yyparse();

    fclose(yyin);
    return SUCCESS;
}

