

/* This is auto-generated code. Edit at your own peril. */

#include "CuTest.h"
#include <stdlib.h>


extern void TestCurrentFileEntry(CuTest*);
extern void AddTestsToSuite(CuSuite *suite);


void RunAllTests(void) 
{
    CuString *output = CuStringNew();
    CuSuite* suite = CuSuiteNew();


    AddTestsToSuite(suite);

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
    free(output);
    free(suite);
}


int main(void)
{
    RunAllTests();
    return 0;
}

