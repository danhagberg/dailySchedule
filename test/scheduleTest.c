#include <time.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "CuTest.h"
#include "schedule.h"
#include "schedule.tab.h"

struct tm testTime;
time_t testTimeSeconds;
// Year as specified in tm struct
#define TEST_YEAR (2010 - 1900) 
// Mon is 0 - 11
#define TEST_MON 4
#define TEST_DAY_OF_MON 28
#define TEST_DAY_OF_WEEK 5
#define TEST_HOUR 10
#define TEST_MIN 15
#define TEST_SEC 23

#define ERROR 1
#define SUCCESS 0

struct testDate {
    int year;
    int mon;
    int dom;
    int dow;
    int hour;
    int min;
    int sec;
};

struct testSchedule {
    valueStruct *year;
    valueStruct *mon;
    valueStruct *dom;
    valueStruct *dow;
    valueStruct *hour;
    valueStruct *min;
    valueStruct *sec;
};

typedef struct _testArgs {
    struct testDate current;
    struct testSchedule schedule;
    struct testDate expectedCal;
    char testName[128];
    struct _testArgs *next;
} testArgs;

testArgs *head;
testArgs *tail;
testArgs *currTest;

/*----------------------------------------------------------------------
 Prototypes
 ----------------------------------------------------------------------*/
void compareTimeStructures(CuTest *tc, const char * testName, 
                           struct tm *expected, struct tm *actual, int expDow);
void createExpectedCal(CuTest *tc, struct tm *expected, int year, int mon, 
                       int dom, int hour, int min, int sec);
void runTestEntry(CuTest *tc, testArgs *test);
int parseDate(const char * buffer, struct testDate *testSched); 
testArgs * allocateTestArgs();
int loadTestArrayFromFile();
void AddTestsToSuite(CuSuite *suite);
valueStruct * parseValue(char * token, Bool normalize);
int parseSchedule(const char * buffer, struct testSchedule *testSched);
void cleanupTestSchedule(struct testSchedule *testSched);
/*----------------------------------------------------------------------*/


void InitTestEnv() {
    if (testTimeSeconds == 0) {
        // Set the "current time" to 2010/5/28 Fri(5) 10:15:23
        testTime.tm_year = TEST_YEAR;
        testTime.tm_mon = TEST_MON;
        testTime.tm_mday = TEST_DAY_OF_MON;
        testTime.tm_wday = TEST_DAY_OF_WEEK;
        testTime.tm_hour = TEST_HOUR;
        testTime.tm_min = TEST_MIN;
        testTime.tm_sec = TEST_SEC;
        testTime.tm_isdst = -1;  // Cause mktime to determine
        testTimeSeconds = mktime(&testTime);
        setTestTime(testTimeSeconds);
    }
}

int loadTestArrayFromFile() {
    #define MAX_SCHED_ENTRY_LEN 200
	char buffer[MAX_SCHED_ENTRY_LEN];
	char *bufferCheck;
	FILE * schedStream;
    int itemCount = 0;
	testArgs * entry;
	schedStream = fopen("test.dat", "r");
    if(schedStream == NULL) {
    	perror("Failed to open input file: ");
    	return ERROR;
    }

    memset(buffer, 0, sizeof(char) * MAX_SCHED_ENTRY_LEN);
    bufferCheck = fgets(buffer, MAX_SCHED_ENTRY_LEN, schedStream);
    while(!feof(schedStream) || bufferCheck != NULL) {
        if (buffer[0] != '/' && buffer[0] != '\n') {
            switch(itemCount%4) {
            case 0:  // Test Name
                entry = allocateTestArgs();
                strcat(entry->testName, buffer);
                break;
            case 1:  // "Current" Test Calendar
                parseDate(buffer, &(entry->current));
                break;
            case 2:  // Schedule
                parseSchedule(buffer, &(entry->schedule));
                break;
            case 3:  // Expected Calendar
                parseDate(buffer, &(entry->expectedCal));
                break;
            }
            itemCount++;
        }
        memset(buffer, 0, sizeof(char) * MAX_SCHED_ENTRY_LEN);
        bufferCheck = fgets(buffer, MAX_SCHED_ENTRY_LEN, schedStream);
    }

    fclose(schedStream);
    return 0;
}

testArgs * allocateTestArgs() {
    testArgs *entry;
    entry = malloc(sizeof(testArgs));
    memset( entry, 0, sizeof(testArgs));
    if (head == NULL) {
        head = tail = entry;
    }
    else {
        tail->next = entry;
        tail = entry;
    }
    return entry;
}

int parseDate(const char * buffer, struct testDate *testSched) {
    #define NUMBER_OF_TOKENS 7
	int tempVal;
	char *strTokens[NUMBER_OF_TOKENS+1]; 
	int tokenCnt = 0;
    int ti = 0; // token index

	strTokens[tokenCnt] = strtok((char*)buffer, " \n\r"); // Not thread safe
	while (strTokens[tokenCnt] != NULL) {
		if (tokenCnt == NUMBER_OF_TOKENS) {
			fprintf(stderr, "parseDate: Too many tokens in entry\n\t%s\n", buffer);
			return ERROR;
		}
		tokenCnt++;
    	strTokens[tokenCnt] = strtok(NULL, "\r\n ");
	}

	if(tokenCnt < NUMBER_OF_TOKENS - 1) {
		fprintf(stderr, "parseDate: Problem with entry\n\t%s\n", buffer);
		return ERROR;
	}

	testSched->year = atoi(strTokens[ti++]);
	testSched->mon = atoi(strTokens[ti++]) - 1;
	testSched->dom = atoi(strTokens[ti++]);
	testSched->dow = atoi(strTokens[ti++]) - 1;
	testSched->hour = atoi(strTokens[ti++]);
	testSched->min = atoi(strTokens[ti++]);
	testSched->sec = atoi(strTokens[ti++]);
}

int parseSchedule(const char * buffer, struct testSchedule *testSched) {
    #define NUMBER_OF_TOKENS 7
	int tempVal;
	char *strTokens[NUMBER_OF_TOKENS+1]; 
	int tokenCnt = 0;
    int ti = 0; // token index

	strTokens[tokenCnt] = strtok((char*)buffer, " \r\n"); // Not thread safe
	while (strTokens[tokenCnt] != NULL) {
		if (tokenCnt == NUMBER_OF_TOKENS) {
			fprintf(stderr, "parseSchedule: Too many tokens in entry\n\t%s\n", buffer);
			return ERROR;
		}
		tokenCnt++;
		strTokens[tokenCnt] = strtok(NULL, " \r\n");
	}

	if(tokenCnt < NUMBER_OF_TOKENS - 1) {
		fprintf(stderr, "parseSchedule: Problem with entry\n\t%s\n", buffer);
		return ERROR;
	}

	testSched->year = parseValue(strTokens[ti++], False);
	testSched->mon = parseValue(strTokens[ti++], True); 
	testSched->dom = parseValue(strTokens[ti++], False);
	testSched->dow = parseValue(strTokens[ti++], True);
	testSched->hour = parseValue(strTokens[ti++], False);
	testSched->min = parseValue(strTokens[ti++], False);
	testSched->sec = parseValue(strTokens[ti++], False);
}

valueStruct * parseValue(char * token, Bool normalize) {
    int begin, end, step;
    valueStruct *base;  // Used for lists
    char * listElem;
    // lists have ,
    // ranges have - and optional /
    // else wild card or value
    if (strchr(token, ',') > 0) {
        // List -- Have to recursively build the valueStructs
        // Note -- strtok is not thread safe or re-entrant.  
        //         Assumption: lists cannot have lists
        listElem = strtok(token, ",\n"); 
        assert(listElem != NULL);
        base = createListValue(parseValue(listElem, normalize)); 
        for (listElem = strtok(NULL, ",\n"); listElem != NULL; 
            listElem = strtok(NULL, ",\n")) {
            addListValue(base, parseValue(listElem, normalize));
        }
        return base;
    } 
    else if (strchr(token, '-') > 0) {
        // Range value
        step = 1; // Default value
        sscanf(token, "%d-%d/%d", &begin, &end, &step);
        if (normalize) {
            return createRangeValue(begin - 1, end - 1, step);
        }
        else {
            return createRangeValue(begin, end, step);
        }
    } 
    else if (token[0] == '*') {
        // Wild card
        return createWildcardValue();
    } 
    else {
        // regular value
        if (normalize) {
            return createSingleValue(atoi(token) - 1);
        }
        else {
            return createSingleValue(atoi(token));
        }
    }
}


void TestCurrentFileEntry(CuTest *tc) {
    if (head != NULL) {
        if (currTest == NULL) {
            currTest = head;
        } else {
            // Set up for next entry into this function
            currTest = currTest->next;
        }
        
        runTestEntry(tc, currTest);
    }
}

void runTestEntry(CuTest *tc, testArgs *test) {
    time_t taskTime, currentTimeSeconds;
    struct tm * taskTimeStructure, expectedTaskTime, currentTime;

    InitTestEnv();

    // Set the "current" time.
    currentTime.tm_year = test->current.year - 1900;
    currentTime.tm_mon = test->current.mon;
    currentTime.tm_mday = test->current.dom;
    currentTime.tm_wday = test->current.dow;
    currentTime.tm_hour = test->current.hour;
    currentTime.tm_min = test->current.min;
    currentTime.tm_sec = test->current.sec;
    currentTime.tm_isdst = -1;  // Cause mktime to determine
    currentTimeSeconds = mktime(&currentTime);
    setTestTime(currentTimeSeconds);

    testTimeSeconds = 0;// Will cause init to reset to base time if invoked

    // Set the schedule to test
    scheduleEntry *testEntry = 
        createScheduleEntryAdv(test->schedule.year, test->schedule.mon, 
            test->schedule.dom, test->schedule.dow, test->schedule.hour, 
            test->schedule.min, 0, "task", "reminder");
    CuAssertPtrNotNull(tc, testEntry);

    // Calculate the next time for this task 
    taskTime = calcNextTimeForTask(testEntry);

    taskTimeStructure = localtime(&taskTime);

    createExpectedCal(tc, &expectedTaskTime, test->expectedCal.year - 1900, 
        test->expectedCal.mon, test->expectedCal.dom, 
        test->expectedCal.hour, test->expectedCal.min, test->expectedCal.sec); 

    compareTimeStructures(tc, test->testName, &expectedTaskTime, taskTimeStructure, test->expectedCal.dow);
    
    cleanupTestSchedule(&test->schedule);
    freeScheduleEntry(testEntry);
}

void createExpectedCal(CuTest *tc, struct tm *expected, int year, int mon, 
                       int dom, int hour, int min, int sec) {

    memset(expected, 0, sizeof(struct tm));
    expected->tm_year = year;
    expected->tm_mon = mon;
    expected->tm_mday = dom;
    expected->tm_hour = hour;
    expected->tm_min = min;
    expected->tm_sec = sec;
    expected->tm_isdst = -1;  // Cause mktime to determine

    mktime(expected);

}
void compareTimeStructures(CuTest *tc, const char * testName, 
    struct tm *expected, struct tm *actual, int expDow) {
    char msgBuff[256];
    int testNameLen = strnlen(testName, 200);
    memset(msgBuff, 0, sizeof(msgBuff));
    strcpy(msgBuff, testName);

    #ifdef DEBUG
    puts(testName);
    printf("Expected: %s", asctime(expected));
    printf("Actual  : %s\n", asctime(actual));
    #endif // DEBUG

    memset(msgBuff+testNameLen, 0, 256-testNameLen);
    strcat(msgBuff, ": Year");
    CuAssertIntEquals_Msg(tc, msgBuff, expected->tm_year, actual->tm_year);

    memset(msgBuff+testNameLen, 0, 256-testNameLen);
    strcat(msgBuff, ": Month");
    CuAssertIntEquals_Msg(tc, msgBuff, expected->tm_mon, actual->tm_mon);

    memset(msgBuff+testNameLen, 0, 256-testNameLen);
    strcat(msgBuff, ": DoM");
    CuAssertIntEquals_Msg(tc, msgBuff, expected->tm_mday, actual->tm_mday);

    memset(msgBuff+testNameLen, 0, 256-testNameLen);
    strcat(msgBuff, ": DoW");
    CuAssertIntEquals_Msg(tc, msgBuff, expDow, actual->tm_wday);

    memset(msgBuff+testNameLen, 0, 256-testNameLen);
    strcat(msgBuff, ": Hour");
    CuAssertIntEquals_Msg(tc, msgBuff, expected->tm_hour, actual->tm_hour);

    memset(msgBuff+testNameLen, 0, 256-testNameLen);
    strcat(msgBuff, ": Min");
    CuAssertIntEquals_Msg(tc, msgBuff, expected->tm_min, actual->tm_min);

    memset(msgBuff+testNameLen, 0, 256-testNameLen);
    strcat(msgBuff, ": Sec");
    CuAssertIntEquals_Msg(tc, msgBuff, expected->tm_sec, actual->tm_sec);
}

void TestValueParse(CuTest *tc) {
    valueStruct * testVal, *base;
    char testListVal_2[4] = "1,3";
    char testListVal_3[6] = "1,3,5";
    char testListVal_2R[9] = "1,3-10/2";
    testVal = parseValue("3", False);
    CuAssertTrue(tc, testVal->type == SINGLE && testVal->value == 3);
    freeValueStruct(testVal);

    testVal = parseValue("3", True);
    CuAssertTrue(tc, testVal->type == SINGLE && testVal->value == 2);
    freeValueStruct(testVal);

    testVal = parseValue("3-5", False);
    CuAssertTrue(tc, testVal->type == RANGE && testVal->range[0] == 3 
                     && testVal->range[1] == 5 && testVal->range[2] == 1);
    freeValueStruct(testVal);

    testVal = parseValue("3-5", True);
    CuAssertTrue(tc, testVal->type == RANGE && testVal->range[0] == 2 
                     && testVal->range[1] == 4 && testVal->range[2] == 1);
    freeValueStruct(testVal);

    testVal = parseValue("3-5/2", False);
    CuAssertTrue(tc, testVal->type == RANGE && testVal->range[0] == 3 
                     && testVal->range[1] == 5 && testVal->range[2] == 2);
    freeValueStruct(testVal);

    testVal = parseValue("3-5/2", True);
    CuAssertTrue(tc, testVal->type == RANGE && testVal->range[0] == 2 
                     && testVal->range[1] == 4 && testVal->range[2] == 2);
    freeValueStruct(testVal);

    testVal = parseValue("*", False);
    CuAssertTrue(tc, testVal->type == WILDCARD && testVal->value == -1);
    freeValueStruct(testVal);

    testVal = parseValue("*", True);
    CuAssertTrue(tc, testVal->type == WILDCARD && testVal->value == -1);
    freeValueStruct(testVal);

    // Test 2 element simple list
    testVal = parseValue(testListVal_2, False);
    base = testVal;
    CuAssertTrue(tc, testVal->type == LIST);
    CuAssertIntEquals(tc, testVal->listNode.element->value, 1); 

    testVal = testVal->listNode.next;
    CuAssertIntEquals(tc, testVal->listNode.element->value, 3); 
    freeValueStruct(base);

    // Test 3 element simple list
    testVal = parseValue(testListVal_3, False);
    base = testVal;
    CuAssertTrue(tc, testVal->type == LIST);
    CuAssertIntEquals(tc, testVal->listNode.element->value, 1); 

    testVal = testVal->listNode.next;
    CuAssertIntEquals(tc, testVal->listNode.element->value, 3); 

    testVal = testVal->listNode.next;
    CuAssertIntEquals(tc, testVal->listNode.element->value, 5); 
    freeValueStruct(base);

    // Test 2 element list with range
    testVal = parseValue(testListVal_2R, False);
    base = testVal;
    CuAssertTrue(tc, testVal->type == LIST);
    CuAssertIntEquals(tc, testVal->listNode.element->type, SINGLE); 
    CuAssertIntEquals(tc, testVal->listNode.element->value, 1); 

    testVal = testVal->listNode.next;
    CuAssertIntEquals(tc, testVal->listNode.element->type, RANGE); 
    CuAssertIntEquals(tc, testVal->listNode.element->range[0], 3); 
    CuAssertIntEquals(tc, testVal->listNode.element->range[1], 10); 
    CuAssertIntEquals(tc, testVal->listNode.element->range[2], 2); 
    freeValueStruct(base);

}
/**
 * Test File Parsing
 */
void TestFileParse(CuTest *tc) {
    extern FILE *yyin;
	yyin = fopen("testSched.dat", "r");
    CuAssertPtrNotNullMsg(tc, "Failed to open input file: testSched.dat", yyin);

    CuAssertIntEquals(tc, 0, yyparse());

    fclose(yyin);
}

void AddTestsToSuite(CuSuite *suite) {
    testArgs *test;
    SUITE_ADD_TEST(suite, TestValueParse);
    SUITE_ADD_TEST(suite, TestFileParse);
    loadTestArrayFromFile();
    for (test = head; test != NULL; test = test->next) {
        SUITE_ADD_TEST(suite, TestCurrentFileEntry);
    }
}

void cleanupTestSchedule(struct testSchedule *testSched) {
    freeValueStruct(testSched->year);
    freeValueStruct(testSched->mon);
    freeValueStruct(testSched->dom);
    freeValueStruct(testSched->dow);
    freeValueStruct(testSched->hour);
    freeValueStruct(testSched->min);
}

