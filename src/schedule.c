#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
#include "schedule.h"
#include "timeRoutines.h"
#include "schedule.tab.h"

#define SUCCESS 0
#define ERROR 1
// Max length of reminder command *not* including null term
#define MAX_CMD_LEN 300
// Wildcard value for schedule field
#define SCHED_WILDCARD -1

#define TIME_IN_PAST -1

#define ERR_FILE stdout

// Local implementation of strnlen.
size_t strnlen(const char *s, size_t n)
{
	const char *p = (const char *)memchr(s, 0, n);
	return(p ? p-s : n);
}

/* -----------------------------------------------------------------------------
 *  Internal Structures
 * ---------------------------------------------------------------------------*/

/*
 * Head and tail of linked list containing all schedule entries.
 */
static scheduleNode * schedHead;
static scheduleNode * schedTail;

/*
 * Current number of schedule entries
 */
int scheduleCount = 0;


/*
 * Head and tail of linked list containing all action entries.
 */
static actionNode * cmdHead;
static actionNode * cmdTail;

/*
 * Time calc variables
 */
const int monthDaysNorm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const int monthDaysLeap[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const int INIT_MON_VAL  = 0;
const int INIT_DOM_VAL  = 1;
const int INIT_HOUR_VAL = 0;
const int INIT_MIN_VAL  = 0;

enum CalIndex {CI_YEAR, CI_MOY, CI_DOM, CI_DOW, CI_HOUR, CI_MIN};

// Used in testing.  Allows test program to set "current" time.
static time_t timeOverride = 0;

/* -----------------------------------------------------------------------------
 *  Prototypes
 * ---------------------------------------------------------------------------*/
void usage();
void getFileLoc(const char *fileLoc);
int processArgs(int argc, char **argv);

int processScheduleFile(const char * fileName);

void runNotifications();
void displayCalValue(FILE * out, valueStruct * value);

int rollYear(scheduleEntry * entry, struct tm * scheduled);
int rollMonth(scheduleEntry * entry, struct tm * scheduled);
int rollDayOfMonth(scheduleEntry * entry, struct tm * scheduled);
int rollHour(scheduleEntry * entry, struct tm * scheduled);
int rollMinute(scheduleEntry * entry, struct tm * scheduled);

int daysInMonth(int year, int month);

void initValues(scheduleEntry *entry, struct tm *scheduled, enum CalIndex level);
int returnFirstValue(valueStruct values);

void execActionCommand(scheduleEntry * entry);

scheduleEntry * parseSchedule(const char * buffer);
void addEntryToList(scheduleEntry * entry);

time_t setDebugTime(time_t time);
time_t getCurrentTime();

valueStruct * copyValueStruct(valueStruct * dest, valueStruct * source);
void normalizeValueStruct(valueStruct * value);

int compareCurrentToSchedule(int current, valueStruct *values);

int compareEventTime(const void * event1, const void * event2);
eventEntry ** getScheduledEvents(time_t startTime, time_t stopTime, 
        int * numEvents);

int setTaskAlarm(time_t timeToSleep);

scheduleNode * createScheduleNode();
void freeScheduleNodeList(scheduleNode * current);
void freeEventSchedule(eventEntry ** schedule, int numEvents);
/**
 * Fork off the process and spawn the provided command using
 * the system(3) call.
 * @cmd Fully qualified command string to pass to system().
 */
void spawnCommand(char *cmd);

/* -----------------------------------------------------------------------------
 *  Function definitions.
 * ---------------------------------------------------------------------------*/

/**
 * Add a schedule entry to the list of entries using the 
 * values provided.  
 * Args:
 *  year        4 digit year 
 *  month       Valid values: 0 - 11 
 *  dayOfMonth  Valid values: 1 - 31 
 *  dayOfWeek   Valid values: 0 - 6 
 *              0 is equal to Sunday
 *  hour        Valid values: 0 - 23 
 *  minute      Valid values: 0 - 59
 *  duration    Duration of task in minutes
 *  task        Null terminated string containing task description
 *  reminder    Null terminated string containing reminder message 
 * Returns:
 *  SUCCESS if entry created.
 *  ERROR   if error occurred during add.  Entry was not created.
 */
int addScheduleEntryAdv(valueStruct * year, valueStruct * month, 
        valueStruct * dayOfMonth, valueStruct * dayOfWeek, valueStruct * hour, 
        valueStruct * minute,  int duration,
		const char * task, const char * reminder) {

	scheduleEntry * entry;

    // Create entry.  
	entry = createScheduleEntryAdv(year, month, dayOfMonth, dayOfWeek,
			hour, minute, duration, task, reminder);
    if(entry == NULL) {
        // Unable to create entry
        printf("Error for task: %s. Cannot continue.",
                task);
        return (ERROR);
    }
    addEntryToList(entry);
    return SUCCESS;
}

/**
 * Add a schedule entry to the list of entries using the 
 * values provided.  
 * Args:
 *  year        4 digit year 
 *  month       Valid values: 1 - 12 
 *  dayOfMonth  Valid values: 1 - 31 
 *  dayOfWeek   Valid values: 1 - 7 
 *              1 is equal to Sunday
 *  hour        Valid values: 0 - 23 
 *  minute      Valid values: 0 - 59
 *  duration    Duration of task in minutes
 *  task        Null terminated string containing task description
 *  reminder    Null terminated string containing reminder message 
 *  actionSet   Root node of action set.  May be NULL
 * Returns:
 *  SUCCESS if entry created.
 *  ERROR   if error occurred during add.  Entry was not created.
 */
int addScheduleEntryNormalize(valueStruct * year, valueStruct * month, 
        valueStruct * dayOfMonth, valueStruct * dayOfWeek, valueStruct * hour, 
        valueStruct * minute,  int duration,
		const char * task, const char * reminder,
        actionNode * actionSet) {

	scheduleEntry * entry;

    normalizeValueStruct(month);
    normalizeValueStruct(dayOfWeek);

    // Create entry.  
	entry = createScheduleEntryAdv(year, month, dayOfMonth, dayOfWeek,
			hour, minute, duration, task, reminder);
    if(entry == NULL) {
        // Unable to create entry
        printf("Error for task: %s. Cannot continue.",
                task);
        return (ERROR);
    }
    entry->actionSet = actionSet;
    addEntryToList(entry);
    return SUCCESS;
}

/**
 * Create a schedule entry using the values provided.  
 * Must be freed using freeScheduleEntry(entry *)
 * Args:
 *  year        4 digit year or -1 for wildcard
 *  month       Valid values: 0 - 11 or -1 for wildcard
 *  dayOfMonth  Valid values: 1 - 31 or -1 for wildcard
 *  dayOfWeek   Valid values: 0 - 6 or -1 for wildcard
 *              0 is equal to Sunday
 *  hour        Valid values: 0 - 23 or -1 for wildcard
 *  minute      Valid values: 0 - 59
 *  duration    Duration of task in minutes
 *  task        Null terminated string containing task description
 *  reminder    Null terminated string containing reminder message 
 * Returns:
 *  Newly allocated and popluated schedule entry
 *  NULL   if error occurred during creation
 */
scheduleEntry * createScheduleEntry(int year, int month, int dayOfMonth,
		int dayOfWeek, int hour, int minute,  int duration,
		const char * task, const char * reminder) {
    valueStruct yearVal, monthVal, domVal, dowVal, hourVal, minuteVal;
	yearVal.type = (year == SCHED_WILDCARD) ? WILDCARD : SINGLE;
	yearVal.value = year;
	monthVal.type = (month == SCHED_WILDCARD) ? WILDCARD : SINGLE;
	monthVal.value = month;
	domVal.type = (dayOfMonth == SCHED_WILDCARD) ? WILDCARD : SINGLE;
	domVal.value = dayOfMonth;
	dowVal.type = (dayOfWeek == SCHED_WILDCARD) ? WILDCARD : SINGLE;
	dowVal.value = dayOfWeek;
	hourVal.type = (hour == SCHED_WILDCARD) ? WILDCARD : SINGLE;
	hourVal.value = hour;
	minuteVal.type = SINGLE;
	minuteVal.value = minute;

	return createScheduleEntryAdv(&yearVal, &monthVal, &domVal, &dowVal,
        &hourVal, &minuteVal, duration, task, reminder);
}

/**
 * Create a schedule entry using the values contained within valueStruct
 * elements. This allows for more advanced scheduling by providing additional
 * methods for specifying values.  In addition to wildcard and single values, 
 * the following types may be used:
 *  - Ranges: start-end 
 *      Example for hour: 9-17 is from 9am to 5pm
 *      Ranges may also include an interval
 *      - Intervals: start-stop/interval 
 *          Example for day of month: 1/2 is every odd day
 *  - Lists: value,value,... 
 *      Example for hour: 9,12,14-16 is 9,12,and between 2 and 4 
 *
 * NOTE: Must be freed using freeScheduleEntry(entry *)
 *
 * Args:
 *  year - minute are valueStructs with the values constrained as shown below:
 *  year        4 digit year 
 *  month       Valid values: 0 - 11 
 *  dayOfMonth  Valid values: 1 - 31 
 *  dayOfWeek   Valid values: 0 - 6 
 *              0 is equal to Sunday
 *  hour        Valid values: 0 - 23 
 *  minute      Valid values: 0 - 59
 *  duration    Duration of task in minutes: 0 - max int. 
 *  task        Null terminated string containing task description
 *  reminder    Null terminated string containing reminder message 
 * Returns:
 *  Newly allocated and popluated schedule entry
 *  NULL   if error occurred during creation
 */
 //TODO Rename this 
scheduleEntry * createScheduleEntryAdv(valueStruct * year, valueStruct * month, 
        valueStruct * dayOfMonth, valueStruct * dayOfWeek, valueStruct * hour, 
        valueStruct * minute,  int duration,
		const char * task, const char * reminder) {
	scheduleEntry * entry;
	entry = (scheduleEntry*) malloc(sizeof(scheduleEntry));
	if(entry == NULL) {
		perror("Failed to allocate memory for schedule entry");
		return NULL;
	}
	copyValueStruct(&entry->year, year);
	copyValueStruct(&entry->monOfYear, month);
	copyValueStruct(&entry->dayOfMonth, dayOfMonth);
	copyValueStruct(&entry->dayOfWeek, dayOfWeek);
	copyValueStruct(&entry->hour, hour);
	copyValueStruct(&entry->minute, minute);
	entry->durationInMin = duration;

	// Task field
    entry->task = strdup(task);
	assert(entry->task != NULL);

	// Reminder message field
    entry->reminderMessage = strdup(reminder);
	assert(entry->reminderMessage != NULL);

	return entry;
}

void freeScheduleEntry(scheduleEntry *entry) {
	free(entry->task);
	free(entry->reminderMessage);
    if (entry->year.type == LIST)       freeValueStructList(&entry->year);
    if (entry->monOfYear.type == LIST)  freeValueStructList(&entry->monOfYear);
    if (entry->dayOfMonth.type == LIST) freeValueStructList(&entry->dayOfMonth);
    if (entry->dayOfWeek.type == LIST)  freeValueStructList(&entry->dayOfWeek);
    if (entry->hour.type == LIST)       freeValueStructList(&entry->hour);
    if (entry->minute.type == LIST)     freeValueStructList(&entry->minute);
	free(entry); 
}

void addEntryToList(scheduleEntry * entry) {
	scheduleNode * current;
	current = createScheduleNode();
	current->entry = entry;
	if (schedTail != NULL) {
		schedTail->next = current;
		schedTail = current;
	}
	else {
		schedHead = schedTail = current;
	}

    scheduleCount++;
}

void displaySchedule(FILE * out) {
	scheduleNode * current = schedHead;
	while (current != NULL) {
		printf("Y: ");
        displayCalValue(out, &current->entry->year);
		printf(" M: ");
        displayCalValue(out, &current->entry->monOfYear);
		printf(" D: ");
        displayCalValue(out, &current->entry->dayOfMonth);
		printf(" W: ");
        displayCalValue(out, &current->entry->dayOfWeek);
		printf(" H: ");
        displayCalValue(out, &current->entry->hour);
		printf(" M: ");
        displayCalValue(out, &current->entry->minute);
		printf(" D: %d Mins - %s : %s\n",
				current->entry->durationInMin,
				current->entry->task, current->entry->reminderMessage);
		current = current->next;
	}
}

void normalizeValueStruct(valueStruct * value) {
    switch (value->type) {
        case SINGLE:
            value->value--;
            break;
        case RANGE:
            value->range[0]--;
            value->range[1]--;
            break;
        case LIST:
            normalizeValueStruct(value->listNode.element); 
            if (value->listNode.next != NULL) {
                normalizeValueStruct(value->listNode.next);
            }
            break;
        default:
            // Nothing to do for other types
            break;
    }
}


void displayCalValue(FILE * out, valueStruct * value) {
    switch (value->type) {
        case SINGLE:
            fprintf(out, "%d", value->value);
            break;
        case RANGE:
            if (value->range[2] == 1) {
                fprintf(out, "%d-%d", value->range[0], value->range[1]);
            } else {
                fprintf(out, "%d-%d/%d", value->range[0],
                    value->range[1], value->range[2]);
                }
            break;
        case WILDCARD:
            fprintf(out, "*");
            break;
        case LIST:
            displayCalValue(out, value->listNode.element); 
            if (value->listNode.next != NULL) {
                fprintf(out, ",");
                displayCalValue(out, value->listNode.next);
            }
            break;
    }
}

void displayTodaysSchedulex(FILE * out) {
    int calCompDoW, calCompDoM;
	time_t timer;
	struct tm *today;
	timer = getCurrentTime();
	today = localtime(&timer);

	scheduleNode * current = schedHead;
	while (current != NULL) {
        calCompDoW = compareCurrentToSchedule(today->tm_wday, 
            &current->entry->dayOfWeek);
        calCompDoM = compareCurrentToSchedule(today->tm_mday, 
            &current->entry->dayOfMonth);
    	if (calCompDoW == 0 && calCompDoM == 0) {
    		printf("%02d:%02d For %d Minutes - %s : %s\n",
    				current->entry->hour.value, current->entry->minute.value,
    				current->entry->durationInMin,
    				current->entry->task, current->entry->reminderMessage);
    	}
		current = current->next;
	}
	fflush(out);
}

void displayTodaysSchedule(FILE * out) {
    int numEvents;
    int eventIdx;
    char *formattedTime;
    eventEntry ** schedule;
    eventEntry * currentEvent;
	time_t timer, startTime, stopTime;
	struct tm *today, start, stop;
	timer = getCurrentTime();
	today = localtime(&timer);

    memcpy(&start, today, sizeof(struct tm));
    start.tm_hour = 0;
    start.tm_min = 0;
    start.tm_sec = 0;
    startTime = mktime(&start);

    memcpy(&stop, today, sizeof(struct tm));
    stop.tm_hour = 23;
    stop.tm_min = 59;
    stop.tm_sec = 59;
    stopTime = mktime(&stop);

    schedule = getScheduledEvents(startTime, stopTime, &numEvents);

    for( eventIdx=0; eventIdx < numEvents; eventIdx++) {
        currentEvent = schedule[eventIdx];
        // ctime returns \n in formatted time at position second to last pos
        formattedTime = ctime(&(currentEvent)->nextTime);
        formattedTime[24] = '\0';
        printf("%s For %d Minutes - %s : %s\n",
                formattedTime, (currentEvent)->durationInMin,
                (currentEvent)->task, (currentEvent)->reminderMessage);
	}
	fflush(out);
    freeEventSchedule(schedule, numEvents);
}

/**
 * Create a new action set and populate with the provided action.
 */
actionNode * createActionSet(actionDef * action) {
	actionNode * newNode;

	newNode = (actionNode*)malloc(sizeof(actionNode));
	assert(newNode != NULL);
	memset(newNode, 0, sizeof(actionNode));

    newNode->action = action;
    
    return newNode;
}

/**
 * Create a command 
 */
actionDef * createActionCommand(char * commandName, char * commandStr, 
        enum ActionType type) {
    actionDef * action;
    int fieldLen;

	action = (actionDef*)malloc(sizeof(actionDef));
	assert(action != NULL);
	memset(action, 0, sizeof(actionDef));

    action->name = strdup(commandName);

    fieldLen = strnlen(commandStr, MAX_CMD_LEN) + 1;
	action->command = (char*) malloc( sizeof(char) * fieldLen);
	strncpy(action->command, commandStr, fieldLen);

    action->type = type;

    return action;
}


/**
 * Find an existing action command for the provided name.
 */
actionDef * findActionCommand(char * commandName) {
	actionNode * current = cmdHead;
	while (current != NULL) {
        if (strcmp(current->action->name, commandName) == 0 ) {
            return current->action;
        }
		current = current->next;
	}

    return NULL;
}


/**
 * Add an action to a provided set of actions.
 */
void addActionToActionSet(actionNode * node, actionDef * action) {
    actionNode * newNode;
    actionNode * currentNode = node;

    assert(node != NULL);
    while (currentNode->next != NULL) {
        currentNode = currentNode->next;
    }

	newNode = (actionNode*)malloc(sizeof(actionNode));
	assert(newNode != NULL);
	memset(newNode, 0, sizeof(actionNode));

    newNode->action = action;
    currentNode->next = newNode;
}

/**
 * Add a command string to the current list of reminder commands.
 */
void addActionCommand(char * commandName, char * commandStr, 
        enum ActionType type) {
    actionDef * action;
	actionNode * newNode;
    int fieldLen;

	action = (actionDef*)malloc(sizeof(actionDef));
	assert(action != NULL);
	memset(action, 0, sizeof(actionDef));

	newNode = (actionNode*)malloc(sizeof(actionNode));
	assert(newNode != NULL);
	memset(newNode, 0, sizeof(actionNode));

    action->name = strdup(commandName);

    fieldLen = strnlen(commandStr, MAX_CMD_LEN) + 1;
	action->command = (char*) malloc( sizeof(char) * fieldLen);
	strncpy(action->command, commandStr, fieldLen);

    action->type = type;

    newNode->action = action;

	if (cmdTail != NULL) {
		cmdTail->next = newNode;
		cmdTail = newNode;
	}
	else {
		cmdHead = cmdTail = newNode;
	}
}

/**
 * Fork off the process and spawn the provided command using
 * the system(3) call.
 * @cmd Fully qualified command string to pass to system().
 */
void spawnCommand(char *cmd) {
    int childPid = fork();
    if (childPid == 0) {
        // execv(cmd, (char*)0);
        system(cmd);
        _exit(1);
    }
}

/**
 * Execute all action commands for the current entry.  If the entry 
 * has specific commands, then these will be executed in order.  If not,
 * them all default commands will be executed in order. After all commands
 * are executed, all commands with the ALWAYS action type will be executed
 * in order.  
 */
void execActionCommand(scheduleEntry * entry) {
	char execBuffer[1024];
    memset(execBuffer, 0, sizeof(execBuffer));
    if (entry->actionSet != NULL) {
        actionNode * current = entry->actionSet;
        while (current != NULL) {
            sprintf(execBuffer, current->action->command, 
                    entry->reminderMessage);
            #ifdef DEBUG
            puts(execBuffer);
            #endif
            spawnCommand(execBuffer);
            current = current->next;
        }
    }
    else {
        actionNode * current = cmdHead;
        while (current != NULL) {
            if (current->action->type == DEFAULT) {
                sprintf(execBuffer, current->action->command, 
                        entry->reminderMessage);
                spawnCommand(execBuffer);
            }
            current = current->next;
        }
    }
    // Execute the ALWAYS tasks
    // TODO If there is an ALWAYS in the specific list, should it be 
    // executed twice?
    actionNode * current = cmdHead;
    while (current != NULL) {
        if (current->action->type == ALWAYS) {
            sprintf(execBuffer, current->action->command, 
                    entry->reminderMessage);
            spawnCommand(execBuffer);
        }
        current = current->next;
    }
}


/**
 * Compare two eventEntry elements and return - int, 0, or + int for 
 * event1 < event2, event1 = event2, or event1 > event2 respectively.
 */
int compareEventTime(const void * event1, const void * event2) {
    return (*(eventEntry**)event1)->nextTime - (*(eventEntry**)event2)->nextTime;
}

/**
 * Returns sorted array of scheduled events based on start and stop times.
 * For repeating events, will return only the next scheduled time from now
 * within the range.  
 * The size of the array will be returned in the out parameter numEvents
 * Returned array should be freed using freeEventSchedule.
 */
eventEntry ** getScheduledEvents(time_t startTime, time_t stopTime, 
        int * numEvents) {
    eventEntry ** eventList, ** base;
	time_t schedTimer;
	scheduleNode * current;
    int eventCount = 0;

    // Allocate an array of eventEntry * assuming that all tasks will
    // have an event created for the time period.
    /* TODO: Wasting resources.  Could start with smaller
       size and then double until reaching scheduleCount */
    base = eventList = malloc(sizeof(eventEntry *) * scheduleCount);
    assert(eventList != NULL);
    memset(eventList, 0, sizeof(eventEntry *) * scheduleCount);

    // Iterate schedule entries and convert to absolute time based on now.
    for (current = schedHead;current != NULL; current = current->next) {
        schedTimer = calcNextTimeForTask(current->entry);
        if (schedTimer >= startTime && schedTimer <= stopTime) {
            // Create event entry for scheduled event
            *eventList = malloc(sizeof(eventEntry));
            assert(*eventList != NULL);
            memset(*eventList, 0, sizeof(eventEntry));
            (*eventList)->nextTime = schedTimer;
            (*eventList)->durationInMin = current->entry->durationInMin;
            (*eventList)->task = strdup(current->entry->task);
            (*eventList)->reminderMessage = strdup(current->entry->reminderMessage);
            eventCount++;
            eventList++; // Set up for next entry
        }
    }
    eventList = base; 
    qsort(eventList, eventCount, sizeof(eventEntry *), compareEventTime);
    *numEvents = eventCount;

    return eventList;
}

void freeEventEntry(eventEntry * entry) {
    free(entry->task);
    free(entry->reminderMessage);
    free(entry);
}

void freeEventSchedule(eventEntry ** schedule, int numEvents) {
    int eventIdx;
    for( eventIdx=0; eventIdx < numEvents; eventIdx++) {
        if (schedule[eventIdx] != NULL) {
            freeEventEntry(schedule[eventIdx]);
        }
    }
    free(schedule);
}

void runNotifications() {
    scheduledExec * nextTask;
    
    nextTask = calcNextTaskAlarm();
    // This will start a event driven loop that will not return.
    waitForTask(nextTask);
}

/**
 * Return next scheduled task and the time to execute that task.  If no 
 * future tasks are scheduled, then a null value will be returned.  
 *
 * Returned value must be freed by caller. 
 */
scheduledExec * calcNextTaskAlarm() {
	time_t schedTimer, currentTime, nextTaskTime;
    // scheduleEntry * nextScheduledEntry = NULL;
	scheduleNode * nextSchedHead, * nextSchedTail, * listPtr, *listTemp;
	scheduleNode * current;
	scheduledExec * nextExec = NULL;
    char *formattedTime;

    nextTaskTime = LONG_MAX;
    nextSchedHead = nextSchedTail = NULL;
    currentTime = getCurrentTime();
    #ifdef DEBUG
    // ctime returns \n in formatted time at position second to last pos
    formattedTime = ctime(&currentTime);
    formattedTime[24] = '\0';
    printf("Time: %s\n", formattedTime);
    #endif // DEBUG
    
    // Iterate over all tasks and find the task closest to now.
    for (current = schedHead;current != NULL; current = current->next) {
        schedTimer = calcNextTimeForTask(current->entry);

        #ifdef DEBUG
        // ctime returns \n in formatted time at position second to last pos
        formattedTime = ctime(&schedTimer);
        formattedTime[24] = '\0';
        printf("%s\t%s\tSecs from now: %ld\n",
            formattedTime, current->entry->task, schedTimer - currentTime);
        #endif // DEBUG

        if (schedTimer > currentTime) {
            if (schedTimer < nextTaskTime) {
                nextTaskTime = schedTimer;

                if (nextSchedHead != NULL) {
                    // Replacing current schedule time.  Clear out list
                    for (listPtr = nextSchedHead; listPtr != NULL;) {
                        listTemp = listPtr->next;
                        free(listPtr);
                        listPtr = listTemp;
                    }
                }

                nextSchedHead = nextSchedTail = createScheduleNode();
                nextSchedHead->entry = current->entry;
            }
            else if (schedTimer == nextTaskTime) {
                // List must already exist, just append new entry
                nextSchedTail->next = createScheduleNode();
                nextSchedTail->next->entry = current->entry;
                nextSchedTail = nextSchedTail->next;
            }
        }
    }
    if (nextSchedHead != NULL) {
        #ifdef DEBUG
        printf("Next Entry: %s\n", nextSchedHead->entry->task);
        #endif // DEBUG
        nextExec = malloc(sizeof(scheduledExec));
        assert(nextExec != NULL);
        memset(nextExec, 0, sizeof(scheduledExec));
        nextExec->absTime = nextTaskTime;
        nextExec->taskHead = nextSchedHead;
    }

    return nextExec;

}

/**
 * Execute all actions associated with the scheduled task.
 */
void executeScheduledEntry( scheduledExec * task ) {
    scheduleNode *current;

    // Execute reminder using entry for which the sleep was entered.
    for (current = task->taskHead; current != NULL; current = current->next) {
        execActionCommand(current->entry);
    }
    freeScheduleNodeList(task->taskHead);
    // free(task);
}


/*
int setTaskAlarm(time_t timeToSleep) {
    
    int result = 0;

    struct itimerval value;

    struct sigaction sact;
    sigemptyset( &sact.sa_mask );
    sact.sa_flags = 0;
    sact.sa_handler = executeScheduledEntry;
    sigaction( SIGALRM, &sact, NULL );

    // Set a real time interval timer to repeat every 200 milliseconds

    value.it_interval.tv_sec = 0;   // Execute only once
    value.it_interval.tv_usec = 0;  
    value.it_value.tv_sec = timeToSleep; 
    value.it_value.tv_usec = 0;     

    result = setitimer( ITIMER_REAL, &value, NULL);

    return( result );
}
*/

/**
 * Increment the year if mutable.  If not, return ERROR
 */
int rollYear(scheduleEntry * entry, struct tm * scheduled) {
    unsigned int newYear;
    int calComp;
    switch (entry->year.type) {
    case SINGLE:
        // Unable to modify year
        return ERROR;
        
    case RANGE:
    case LIST:
        newYear = scheduled->tm_year + 1;
        calComp = 
            compareCurrentToSchedule(newYear + 1900, &entry->year);
        if (calComp < 0) {
            // In the past, can't roll
            return ERROR;
        } 
        else {
            scheduled->tm_year = newYear + calComp;
            initValues(entry, scheduled, CI_MOY);
        }
        break;

    case WILDCARD:
        scheduled->tm_year++;
        initValues(entry, scheduled, CI_MOY);
        break;
    }
    return SUCCESS;
}
/**
 * Increment the month if mutable.  If not, the attempt incrementing 
 * coarser grained calendar variables.  If date could not be incremented,
 * return ERROR.
 */
int rollMonth(scheduleEntry * entry, struct tm * scheduled) {
    unsigned int newMonth;
    int calComp;
    switch (entry->monOfYear.type) {
    case SINGLE:
        // Unable to modify month
        return rollYear(entry, scheduled);
        
    case RANGE:
    case LIST:
        newMonth = scheduled->tm_mon + 1;
        calComp = 
            compareCurrentToSchedule(newMonth, &entry->monOfYear);
        if (calComp < 0) {
            return rollYear(entry, scheduled);
        } 
        else {
            scheduled->tm_mon = newMonth + calComp;
            initValues(entry, scheduled, CI_DOM);
        }
        break;

    case WILDCARD:
        if (scheduled->tm_mon + 1 < 12) {
            scheduled->tm_mon++;
            initValues(entry, scheduled, CI_DOM);
        }
        else {
            // Would cause change in year.  If year is wildcard, update year
            // and that will init the month
            return rollYear(entry, scheduled);
        }
    }
    return SUCCESS;
}
/**
 * Increment the day of month, if mutable, by the number of days provided.  
 * This may be constrained by the Day of Week setting. 
 * If not, then attempt incrementing coarser grained calendar variables.  
 * If date could not be incremented, return ERROR.
 *
 * Assumptions: both scheduled tm_wday and tm_mday are set 
 */
int rollDayOfMonth(scheduleEntry * entry, struct tm * scheduled) {
    unsigned int newDay, daysInSchedMonth, numDays;
    int calComp;

    switch (entry->dayOfMonth.type) {
    case SINGLE:
        // Unable to modify day of month
        return rollMonth(entry, scheduled);
        
    case RANGE:
    case LIST:
        newDay = scheduled->tm_mday + 1; 
        daysInSchedMonth = daysInMonth(scheduled->tm_year, scheduled->tm_mon);
        if (newDay > daysInSchedMonth) {
            return rollMonth(entry, scheduled);
        }

        calComp = compareCurrentToSchedule(newDay, &entry->dayOfMonth);
        if (calComp < 0) {
            return rollMonth(entry, scheduled);
        } 
        else {
            scheduled->tm_mday = newDay + calComp;
        }
        break;

    case WILDCARD:

        // Day of Week processing.  Can be specified only if Day of Mon is wild
        switch (entry->dayOfWeek.type) {
        case SINGLE:
            numDays = 7;
            break;
        case RANGE:
        case LIST:
            newDay = scheduled->tm_wday + 1; 
            calComp = compareCurrentToSchedule(newDay, &entry->dayOfWeek);
            numDays = (7 + calComp + 1) % 7; 
            break;
        case WILDCARD:
            numDays = 1;  // Default number of days to add.
            break;
        } // Day of Week processing complete.
        
        daysInSchedMonth = daysInMonth(scheduled->tm_year, scheduled->tm_mon);

        newDay = scheduled->tm_mday + numDays;
        if (newDay > daysInSchedMonth) {
            return rollMonth(entry, scheduled);
        }
        else {
            scheduled->tm_mday = newDay;
        }
    }

    // Day rolled, set all finer grained variables to initial values
    initValues(entry, scheduled, CI_HOUR);
    return SUCCESS;
}
/**
 * Increment the hour if mutable.  If not, then attempt incrementing 
 * coarser grained calendar variables.  If date could not be incremented,
 * return ERROR.
 */
int rollHour(scheduleEntry * entry, struct tm * scheduled) {
    unsigned int newHour;
    int calComp;
    switch (entry->hour.type) {
    case SINGLE:
        // Unable to modify hour
        return rollDayOfMonth(entry, scheduled);
        
    case RANGE:
    case LIST:
        newHour = scheduled->tm_hour + 1; 
        calComp = compareCurrentToSchedule(newHour, &entry->hour);
        if (calComp < 0) {
            return rollDayOfMonth(entry, scheduled);
        } 
        else {
            scheduled->tm_hour = newHour + calComp;
            initValues(entry, scheduled, CI_MIN);
        }
        break;

    case WILDCARD:
        newHour = scheduled->tm_hour + 1; 
        if (newHour > 23) {
            return rollDayOfMonth(entry, scheduled);
        } 
        else {
            scheduled->tm_hour = newHour;
            initValues(entry, scheduled, CI_MIN);
        }
    }
    return SUCCESS;
}

/**
 * Increment the minute if mutable.  If not, attempt incrementing 
 * coarser grained calendar variables.  If date could not be incremented,
 * return ERROR.
 */
int rollMinute(scheduleEntry * entry, struct tm * scheduled) {
    unsigned int newMinute;
    int calComp;
    switch (entry->minute.type) {
    case SINGLE:
        // Unable to modify minute
        return rollHour(entry, scheduled);
        
    case RANGE:
    case LIST:
        newMinute = scheduled->tm_min + 1; 
        calComp = compareCurrentToSchedule(newMinute, &entry->minute);
        if (calComp < 0) {
            return rollHour(entry, scheduled);
        } 
        else {
            scheduled->tm_min = newMinute + calComp;
        }
        break;

    case WILDCARD:
        // ERROR:  Minute cannot be wild
        break;
    }
    return SUCCESS;
}


/**
 * Return the number of days in the month for the given year.
 */
int daysInMonth(int year, int month) {
    if ((year % 400 == 0) || (year % 4 == 0 && year % 100 != 0)) {
        return monthDaysLeap[month];
    } 
    else {
        return monthDaysNorm[month];
    }
}

/**
 * Returns the next time that the provided entry should be activated.
 * If in the past, TIME_IN_PAST, a negative value wil be returned.
 */
time_t calcNextTimeForTask(scheduleEntry *entry) {
    struct tm scheduled, *now;
    time_t currentTime = getCurrentTime();
    int calComp;
    // Populate now and default the scheduled to now
	now = localtime(&currentTime);
	memcpy(&scheduled, now, sizeof(struct tm));
    scheduled.tm_isdst = -1;    // Cause mktime to reevaluate dst for given time
    scheduled.tm_sec = 0;

    // Year check:

	if (entry->year.type != WILDCARD) {
        calComp = compareCurrentToSchedule(now->tm_year + 1900, &entry->year);
        if (calComp < 0) {
	        // No future entries for this task.
	        return TIME_IN_PAST;
	    }
	    else if (calComp > 0) {
	        // Task is next year, init lower variable and return time.
    	    scheduled.tm_year += calComp;
	        initValues(entry, &scheduled, CI_MOY);
	        return mktime(&scheduled);
	    }
	}
    /* 
        Month check:
        Assumption s.year = n.year
    */
    if (entry->monOfYear.type != WILDCARD)  {
        calComp = compareCurrentToSchedule(now->tm_mon, &entry->monOfYear);
        if (calComp < 0) {
	    	// task has passed.
	    	if (rollYear(entry, &scheduled) == ERROR) {
	    		return TIME_IN_PAST;
	    	}
            scheduled.tm_mon = returnFirstValue(entry->monOfYear);
            return mktime(&scheduled);
	    }
	    else if (calComp > 0) {
    	    scheduled.tm_mon += calComp;
	    	initValues(entry, &scheduled, CI_DOM);
	    	return mktime(&scheduled);
	    }
        else {
            scheduled.tm_mon = now->tm_mon;
        }
    }

    /*
        Day check:
        Assumption s.year = n.year
        Assumption s.mon = n.mon
    */
    if (entry->dayOfWeek.type != WILDCARD) {
        /*
            Day of Week check:
            The result of this will be an updated day of *Month* (dom)
            Assumption s.year = n.year
            Assumption s.mon = n.mon
            Assumption e.dom is wild
            Assumption e.dow is *not* wild
        */
        calComp = compareCurrentToSchedule(now->tm_wday, &entry->dayOfWeek);
        if (calComp != 0) {
            int tempMday = scheduled.tm_mday + ((7 + calComp) % 7);
            if (tempMday > daysInMonth(scheduled.tm_year, scheduled.tm_mon)) {
            	if (rollMonth(entry, &scheduled) == ERROR) {
            		// Cannot roll month
            		return TIME_IN_PAST;
            	}
            }
            else {
                scheduled.tm_mday = tempMday; 
            }
            initValues(entry, &scheduled, CI_HOUR);
	    	return mktime(&scheduled);
    	}
    }
    else {
        /* 
            Day of Month check:
            Assumption s.year = n.year
            Assumption s.mon = n.mon
            Assumption e.dow is wild
        */
    	if (entry->dayOfMonth.type != WILDCARD) {
            calComp = compareCurrentToSchedule(now->tm_mday, &entry->dayOfMonth);
    	    if (calComp < 0) {
    			if (rollMonth(entry, &scheduled) == ERROR) {
    				// Can't roll, task is in the past
    				return TIME_IN_PAST;
    			}
                scheduled.tm_mday = returnFirstValue(entry->dayOfMonth);
                return mktime(&scheduled);
    		}
            else if (calComp > 0) {
                scheduled.tm_mday += calComp;
	            initValues(entry, &scheduled, CI_HOUR);
		    	return mktime(&scheduled);
	    	}
            else {
                scheduled.tm_mday = now->tm_mday;
            }
    	}
    }

    /*
        Hour check:
            Assumption s.year = n.year
            Assumption s.mon = n.mon
            Assumption s.dom = n.mon
    */
    if (entry->hour.type != WILDCARD) {
    	// scheduled.tm_hour = entry->hour.value;
        calComp = compareCurrentToSchedule(now->tm_hour, &entry->hour);
    	if (calComp < 0) {
    		if (rollDayOfMonth(entry, &scheduled) == ERROR) {
    			// Cannot roll, task is in the past
    			return TIME_IN_PAST;
    		}
            scheduled.tm_hour = returnFirstValue(entry->hour);
    		return mktime(&scheduled);
    	}
    	else if (calComp > 0) {
            scheduled.tm_hour += calComp;
    		initValues(entry, &scheduled, CI_MIN);
    		return mktime(&scheduled);
    	}
        else {
            scheduled.tm_hour = now->tm_hour;
        }
    }
    /*
        Min check:
            Assumption s.year = n.year
            Assumption s.mon = n.mon
            Assumption s.dom = n.mon
            Assumption s.hour = n.hour
            Assumption e.min *cannot* be wild, but can be range or list
    */
    calComp = compareCurrentToSchedule(now->tm_min, &entry->minute);
    if (calComp < 0) {
        scheduled.tm_min = returnFirstValue(entry->minute);
    	if (rollHour(entry, &scheduled) == ERROR) {
    		// Cannot roll, task in past
    		return TIME_IN_PAST;
	    }
    } 
    else if (calComp > 0) {
        scheduled.tm_min += calComp;
    }
    else if (calComp == 0) {
        scheduled.tm_min = now->tm_min;
        if (scheduled.tm_sec <= now->tm_sec) {
            if (rollMinute(entry, &scheduled) == ERROR) {
                // Cannot roll, task in past
                return TIME_IN_PAST;
            }
        }
    }
    // Minute is either =, >, or the coarser grained values have been rolled
    return mktime(&scheduled);
}


/**
 * Initialize scheduled values from the provided level on down using values 
 * contained within the entry. For example: If the level is DOM, then year and 
 * month will remain intact and all finer grained time values will be initialized
 * to the lowest value if wild or the value from the entry.
 */
void initValues(scheduleEntry *entry, struct tm *scheduled, enum CalIndex level) {
    int dow;
    struct tm tempSchedule;

    switch(level) {
        case CI_YEAR:
            if (!entry->year.type == WILDCARD) {
                scheduled->tm_year = returnFirstValue(entry->year);
            }
        case CI_MOY: 
            scheduled->tm_mon = entry->monOfYear.type == WILDCARD ? 
                INIT_MON_VAL : returnFirstValue(entry->monOfYear);
        case CI_DOM: 
            scheduled->tm_mday = entry->dayOfMonth.type == WILDCARD ? 
                INIT_DOM_VAL : returnFirstValue(entry->dayOfMonth);
        case CI_DOW: 
            if (entry->dayOfWeek.type != WILDCARD) {
                memcpy(&tempSchedule, scheduled, sizeof(struct tm));
                mktime(&tempSchedule);
                dow = returnFirstValue(entry->dayOfWeek);
                if (dow != tempSchedule.tm_wday ) {
                    scheduled->tm_mday += 
                        (7 + entry->dayOfWeek.value - tempSchedule.tm_wday) % 7;
                }
            }
        case CI_HOUR: 
            scheduled->tm_hour = entry->hour.type == WILDCARD ? 
                INIT_HOUR_VAL : returnFirstValue(entry->hour);
        case CI_MIN: 
            scheduled->tm_min = entry->minute.type == WILDCARD ? 
                INIT_MIN_VAL : returnFirstValue(entry->minute);
            break;
        default:
    	    fprintf(stderr, "Invalid value passed to initValues\n");
    }
}

/**
 * Return the first value based on type of entry.
 */
int returnFirstValue(valueStruct values) {
    // Assumption.  Values are in ascending order
    switch(values.type) {
        case SINGLE:
            return values.value;
        case RANGE:
            return values.range[0];
        case LIST:
            // Assumption: List elements cannot be another list.
            return returnFirstValue(*(values.listNode.element));
        default:
            fprintf(stderr, "Err:  returnFirstValue invoked with invalid value\n");
            return ERROR;
    }
}

/**
 * Return "current" time.  During testing, current time will be overridden
 * to provide fixed value for deterministic results.
 */
time_t getCurrentTime() {
    return timeOverride == 0 ? time(NULL) : timeOverride;
}

/**
 * Set fixed test time.  Should be used for testing only.
 */
void setTestTime(time_t time) {
    timeOverride = time;
}

/**
 * Compare current calendar value to all values in provided ValueStruct to 
 * determine if next applicable schedule value is in past, present, or future.  
 * Return offset from current value.  For example:  
 * Current month = 9;  Values: 1,2,5-8;  Return -8 (1-9)
 * Current month = 3;  Values: 1,2,5-8;  Return  2 (5-3)
 * Current month = 5;  Values: 1,2,5-8;  Return  0 (5-5)
 */
int compareCurrentToSchedule(int current, valueStruct *values) {
    valueStruct * listElem = NULL;
    int diff;
    int nextVal;
    int minVal = INT_MAX;
    switch(values->type) {
        case SINGLE:
            return values->value - current;

        case RANGE:
            if (current < values->range[0]) {
                return values->range[0] - current;
            }
            else 
            if (current > values->range[1]) {
                return values->range[0] - current;
            }
            else {
                /* If within the range, return the difference between now 
                   and the next interval (range[2]), which may have a value 
                   of 1 for all values within the range. 
                   Start val +  Number of steps * Step value
                */
                nextVal = values->range[0]                                         
                        + (ceil((1.0 * current - values->range[0]) / values->range[2]) 
                        * values->range[2]);
                if (nextVal > values->range[1]) {
                    return values->range[0] - current;
                }
                else {
                    return nextVal - current;
                }
            }
            
        case LIST:
            // Assumption: List elements cannot be another list.
            listElem = values;
            do {
                diff = compareCurrentToSchedule(current, listElem->listNode.element);
                minVal = diff < minVal ? diff : minVal;
                listElem = listElem->listNode.next;
            }
            while ( diff < 0 && listElem != NULL);

            /* If the entry is in the past, return the difference of the 
               current from the first value in the list. */
            return (diff < 0) ? minVal : diff;

        case WILDCARD:
        default:
            return 0;  // wildcards are any value, so no difference.
    }
}

valueStruct * createSingleValue(int value) {
    valueStruct * container = malloc(sizeof(valueStruct));
    assert(container != NULL);
    memset(container, 0, sizeof(valueStruct));
    container->type = SINGLE;
    container->value = value;
    return container;
}
valueStruct * createRangeValue(int begin, int end, int step) {
    valueStruct * container = malloc(sizeof(valueStruct));
    assert(container != NULL);
    memset(container, 0, sizeof(valueStruct));
    container->type = RANGE;
    container->range[0] = begin;
    container->range[1] = end;
    container->range[2] = step;
    return container;
}

valueStruct * createWildcardValue() {
    valueStruct * container = malloc(sizeof(valueStruct));
    assert(container != NULL);
    memset(container, 0, sizeof(valueStruct));
    container->type = WILDCARD;
    container->value = -1;
    return container;
}

valueStruct * createListValue(valueStruct * value) {
    valueStruct * container = malloc(sizeof(valueStruct));
    assert(container != NULL);
    memset(container, 0, sizeof(valueStruct));
    container->type = LIST;
    container->listNode.element = value;
    return container;
}

void addListValue(valueStruct * list, valueStruct * value) {
    // find last entry;
    valueStruct * current, * container;
    for (current = list; 
         current->listNode.next != NULL; 
         current = current->listNode.next);

    // Create a new LIST element to hold the value
    container = malloc(sizeof(valueStruct));
    assert(container != NULL);
    memset(container, 0, sizeof(valueStruct));
    container->type = LIST;
    container->listNode.element = value;

    // Add the new container to the end of the list
    current->listNode.next = container;
}


/**
 * This will free the valueStruct and if it is a list, then it will recursively
 * free all list members.  If the top level List is allocated on the stack, then
 * use freeValueStructList instead as it will not attempt to free the top level
 * node.
 */
void freeValueStruct(valueStruct * value) {
    if (value->type == LIST) {
        free(value->listNode.element);
        if (value->listNode.next != NULL) {
            freeValueStruct(value->listNode.next);
        }
    }

    free(value);
}

/**
 * This will recursively free the members of the list, but not the top 
 * level item.  
 *
 * NOTE: If the top level List is allocated on the heap, then
 * use freeValueStruct instead.
 */
void freeValueStructList(valueStruct * value) {
    if (value->type == LIST) {
        free(value->listNode.element);
        if (value->listNode.next != NULL) {
            // Past top level node, can now call the standard free
            freeValueStruct(value->listNode.next);
        }
    }
}

/**
 * Free the node list, but not the schedule entries contained within the nodes
 * NOTE: Just testing idea of using recursion to free linked list.  Not sure
 * if it is any better than assiging next to a temp.
 */
void freeScheduleNodeList(scheduleNode * current) {
    if(current != NULL) {
        freeScheduleNodeList(current->next);
    }
    free(current);
}

/**
 * Perform deep copy of valueStruct items needed for LIST type
 * Args:
 *  dest  Allocated structure or null if to be allocated.  If null, then
 *        newly allocted and populated structure will be returned
 *  source Source to copy
 * Return
 *  Populated structure.  If dest arg was not null, then it will be the same 
 *  pointer.  Otherwise, it will be a newly allocated pointer.
 */ 
valueStruct * copyValueStruct(valueStruct * dest, valueStruct * source) {
    if (dest == NULL) {
        dest = malloc(sizeof(valueStruct));
        assert(dest != NULL);
	    memset(dest, 0, sizeof(valueStruct));
    }
    // Deep copy on list items, but a simple memcpy all on all others.
    if (source->type == LIST) {
        dest->type = LIST;
        dest->listNode.element = copyValueStruct(NULL, source->listNode.element);
        if (source->listNode.next != NULL) {
            dest->listNode.next = copyValueStruct(NULL, source->listNode.next);
        }
    } 
    else {
        memcpy(dest, source, sizeof(valueStruct));
    }

    return dest;
}

/**
 * Allocate and initialize a scheduleNode 
 */
scheduleNode * createScheduleNode() {
	scheduleNode * newNode = (scheduleNode *)malloc(sizeof(scheduleNode));
	assert(newNode != NULL);
	memset(newNode, 0, sizeof(scheduleNode));
    return newNode;
}
