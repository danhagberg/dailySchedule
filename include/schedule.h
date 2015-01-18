#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Action definitions
/** 
 * Action Type - Attribute describing execution aspect
 * of Action:
 *  ON_DEMAND - Must be specified on task to execute.
 *  DEFAULT   - Will be executed if no other actions are specified for the task
 *  ALWAYS    - Will be executed for every task regardless of what other
 *              actions are specified.
 *  PRIVATE   - Non-global action.  All actions defined at the task level are
 *              unnamed and private.
 */
enum ActionType {ON_DEMAND, DEFAULT, ALWAYS, PRIVATE};

/**
 * Defines an action that is available for scheduling. 
 */
typedef struct _actionStruct {
	char * name;
	char * command;
    enum ActionType type;
} actionDef;

/**
 * Linked list node used to create a list of actions.
 */
typedef struct _actionNode {
    actionDef * action;
	struct _actionNode * next;
} actionNode;

/**
 *  Calendar entry value types.  
 *  - WILDCARD: Used to allow any value within the applicable range.
 *  - SINGLE: Value represents a single entry within the applicable range.
 *  - LIST: Value is a list of SINGLES and/or RANGES
 *  - RANGE: Value is a inclusive range from x to y with an optional step 
 *    value.
 */
enum ValType {WILDCARD, SINGLE, LIST, RANGE};

/**
 *  Defines each calendar value: year, day of month, hour, ...  Each may be a
 *  wildcard or a value.  Values include: 
 *  - single value (3) 
 *  - range (3 - 6) with optional interval (3 - 6/2) 
 *  - list of calendar values (3,5-7,9)  
 */
typedef struct _valStruct {
    enum ValType type;
    union {
        int value;
        int range[3];    // Start, stop, and interval
        struct { 
            struct _valStruct * element;
            struct _valStruct * next;
        } listNode;
    };
} valueStruct;

/**
 * Represents a complete schedule entry including:
 * - schedule at which action set will be performed
 * - expected length of task
 * - description of task
 * - set of actions to perform 
 */
typedef struct _scheduleStruct {
	valueStruct year;			// Year including century
	valueStruct monOfYear;		// 0 - 11
	valueStruct dayOfMonth;		// 1 - 31
	valueStruct dayOfWeek; 		// 0 - 6 - 0 is Sunday
	valueStruct hour; 			// 0 - 23
	valueStruct minute;			// 0 - 59
	int durationInMin;
	char * task;
	char * reminderMessage;
    actionNode * actionSet;
} scheduleEntry;

/**
 * List of scheduleEntry instances.  
 */
typedef struct _scheduleNode {
	struct _scheduleStruct * entry;
	struct _scheduleNode * next;
} scheduleNode;

/*
 * Represents the absolute time for an event based on the schedule and
 * the time it was created.  May be in the past.
 */
typedef struct _eventStruct {
    time_t nextTime;            // Next scheduled time for this event
	int durationInMin;
	char * task;
	char * reminderMessage;
    actionNode * actionSet;
} eventEntry;

/*
 * Represents the next set of tasks to execute providing the absolute time
 * and a list of tasks.
 */
typedef struct _scheduledExec {
    time_t absTime;            // Time to execute.  
	scheduleNode * taskHead;
} scheduledExec;


// Misc
enum BoolEnum {False, True};
typedef enum BoolEnum Bool;

/* -----------------------------------------------------------------------------
 *  Prototypes
 * ---------------------------------------------------------------------------*/
void displaySchedule(FILE * out);
void displayTodaysSchedule(FILE * out);
void runNotifications();

time_t calcNextTimeForTask(scheduleEntry *entry);

scheduledExec * calcNextTaskAlarm();

/**
 * Add a schedule entry to the list of entries using the 
 * values provided.  
 * Args:
 *  year        4 digit year or -1 for wildcard
 *  month       Valid values: 1 - 12 or -1 for wildcard
 *  dayOfMonth  Valid values: 1 - 31 or -1 for wildcard
 *  dayOfWeek   Valid values: 1 - 7 or -1 for wildcard
 *              1 is equal to Sunday
 *  hour        Valid values: 0 - 23 or -1 for wildcard
 *  minute      Valid values: 0 - 59
 *  duration    Duration of task in minutes
 *  task        Null terminated string containing task description
 *  reminder    Null terminated string containing reminder message 
 * Returns:
 *  SUCCESS if entry created.
 *  ERROR   if error occurred during add.  Entry was not created.
 */
int addScheduleEntry(int year, int month, int dayOfMonth,
		int dayOfWeek, int hour, int minute,  int duration,
		const char * task, const char * reminder, 
        actionNode * actionSet);

/**
 * Add a schedule entry to the list of entries using the 
 * values provided.  
 * Args:
 *  year        4 digit year or -1 for wildcard
 *  month       Valid values: 1 - 12 or -1 for wildcard
 *  dayOfMonth  Valid values: 1 - 31 or -1 for wildcard
 *  dayOfWeek   Valid values: 1 - 7 or -1 for wildcard
 *              1 is equal to Sunday
 *  hour        Valid values: 0 - 23 or -1 for wildcard
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
		const char * task, const char * reminder);

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
        actionNode * actionSet);

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
		const char * task, const char * reminder);

/**
 * Create a schedule entry using valueStruct entries for each of the time 
 * values.  See valueStruct for more informaton regarding how values are 
 * specified.
 *
 * Must be freed using freeScheduleEntry(entry *)
 *
 * Args:
 *  year        4 digit year or wildcard
 *  month       Valid values: 0 - 11 or wildcard
 *  dayOfMonth  Valid values: 1 - 31 or wildcard
 *  dayOfWeek   Valid values: 0 - 6 or wildcard
 *              0 is equal to Sunday
 *  hour        Valid values: 0 - 23 or wildcard
 *  minute      Valid values: 0 - 59
 *  duration    Duration of task in minutes
 *  task        Null terminated string containing task description
 *  reminder    Null terminated string containing reminder message 
 * Returns:
 *  Newly allocated and popluated schedule entry
 *  NULL   if error occurred during creation
 */
scheduleEntry * createScheduleEntryAdv(valueStruct * year, valueStruct * month, 
        valueStruct * dayOfMonth, valueStruct * dayOfWeek, valueStruct * hour, 
        valueStruct * minute,  int duration,
		const char * task, const char * reminder);
/**
 * Free the schedule entry and all associated allocations.
 */
void freeScheduleEntry(scheduleEntry *entry);

/**
 * Set the current time for testing purposes.
 */
void setTestTime(time_t time);

/**
 * Creates a single value valueStruct instance.
 * Should be freed using freeValueStruct
 */
valueStruct * createSingleValue(int value);
/**
 * Creates a range value valueStruct instance.
 * Should be freed using freeValueStruct
 */
valueStruct * createRangeValue(int begin, int end, int step);
/**
 * Creates a wildcard value valueStruct instance.
 * Should be freed using freeValueStruct
 */
valueStruct * createWildcardValue();
/**
 * Creates a list value valueStruct instance.
 * Should be freed using freeValueStructList
 */
valueStruct * createListValue(valueStruct * value);
/**
 * Add a value to an the list provided.
 */
void addListValue(valueStruct * list, valueStruct * value);
/**
 * Free the memory allocated for the valueStruct.
 */
void freeValueStruct(valueStruct * value);
/**
 * Free the memory allocated for a list valueStruct. This will free
 * the list as well as all valueStruct instances within the list.
 */
void freeValueStructList(valueStruct * value);

/**
 * Create and return an instance of actionDef for the provided values.
 */
actionDef * createActionCommand(char * commandName, char * commandStr, 
        enum ActionType type);

/**
 * Find an exsiting actionDef given the command name.
 */
actionDef * findActionCommand(char * commandName);

/**
 * Create and add to the action set, a new action command.
 */
void addActionCommand(char * commandName, char * commandStr, 
        enum ActionType type);

/**
 * Create an action set and initialize using the provided action.
 */
actionNode * createActionSet(actionDef * action);

/**
 * Create and add to the action set, a new action command.
 */
void addActionToActionSet(actionNode * node, actionDef * action);

/**
 * Execute all actions associated with the scheduled task.
 */
void executeScheduledEntry(scheduledExec *task);
#endif //_SCHEDULE_H_
