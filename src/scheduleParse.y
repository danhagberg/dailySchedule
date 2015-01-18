/* A grammar parser for schedule files */
/* Current Schedule Def
// comment
defined_action = #<actionName> <type> <action>}
type = O | D | A {On-demand | Default | Always}
task = <year> <month> <dom> <dow> <hour> <min> <dur> <name> <reminder_msg> <taskAction>
dom = -1 | 1-31
dow = -1 | 0-6
hour = -1 | 0-23
min =  0-59
dur = 0-MAX INT
name = [a-zA-Z0-9-_ ]
reminder_msg = [a-zA-Z0-9-_ ]
taskAction = NULL | #actionName | "action"
*/
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schedule.h"

valueStruct * listVal;
actionNode * actionSetRoot;

%}
%debug

%union {
    int   intVal;
    char charVal;
    char *strVal;
    struct _valStruct *calVal;
    struct _actionNode *actSet;
    struct _actionStruct *actDef;
}
%token <intVal> NUM DUR ACT_EXEC_TYPE
%token <strVal> TEXT QTEXT ACTION COMMENT 
%token <charVal> ANY
%type <calVal> calEntry calValue single list range wildcard
%type <actDef> taskAction
%type <actSet> actionSet


%start schedule

%%

schedule : entries 
         ; 

entries : /* Blank */
        | entries entry
        ;

entry : comment 
      | defined_action 
      | task 
      ;

comment : COMMENT {printf("Comment: %s\n", $1);fflush(stdout);}
        ;

defined_action : ACTION ' ' QTEXT ' ' ACT_EXEC_TYPE 
    {
        addActionCommand($1, $3, $5); 
        free($1);
        free($3);
    }
    ;

task :   calEntry ' ' calEntry ' ' calEntry ' ' calEntry ' ' calEntry ' ' calEntry ' ' NUM ' ' QTEXT ' ' QTEXT
     { 
        addScheduleEntryNormalize($1, $3, $5, $7, $9, $11, $13, $15, $17, NULL); 
        freeValueStruct($1); 
        freeValueStruct($3); 
        freeValueStruct($5); 
        freeValueStruct($7); 
        freeValueStruct($9); 
        freeValueStruct($11); 
        free($15); 
        free($17);
     }
     |   calEntry ' ' calEntry ' ' calEntry ' ' calEntry ' ' calEntry ' ' calEntry ' ' NUM ' ' QTEXT ' ' QTEXT ' ' actionSet
     { 
        addScheduleEntryNormalize($1, $3, $5, $7, $9, $11, $13, $15, $17, $19); 
        freeValueStruct($1); 
        freeValueStruct($3); 
        freeValueStruct($5); 
        freeValueStruct($7); 
        freeValueStruct($9); 
        freeValueStruct($11); 
        free($15); 
        free($17);
        // $19 is not freed as it is still required and will be cleaned up with the schedule entry.
     }
     ;

calEntry : calValue
         | list
         ;

calValue : single
         | range
         | wildcard
         ;

single : NUM { $$ = createSingleValue($1); } 
       ;

range : NUM '-' NUM { $$ = createRangeValue($1,$3,1); } 
      | NUM '-' NUM '/' NUM { $$ = createRangeValue($1,$3,$5); } 
      ;

wildcard : ANY  { $$ = createWildcardValue(); } 
         ;

list : calValue ',' calValue { 
        listVal = createListValue($1); 
        addListValue(listVal, $3);
        $$ = listVal
     } 
     | list ',' calValue { addListValue(listVal, $3); } 
     ;

taskAction : ACTION 
           {    
                $$ = findActionCommand($1);
                if ($$ == NULL) {
                    yyerror("Action not found.  Must be defined prior to use.");
                }
                free($1);
           }
           | QTEXT 
           {
                $$ = createActionCommand("private", $1, PRIVATE); 
                free($1);
           }
           ;

actionSet : taskAction 
          {
            actionSetRoot = createActionSet($1); 
            $$ = actionSetRoot;
          }
          | actionSet ' ' taskAction
          {
            addActionToActionSet($1, $3)
          }
          ; 

%% 
#ifdef STANDALONE
int main() {
puts("Hello");
yydebug=1;
yyparse();

}


#endif

