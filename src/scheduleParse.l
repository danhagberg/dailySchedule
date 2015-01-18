/* Lexical scanner for schedule files */

%{
#include <stdio.h>
#include <string.h>
#include "scheduleParse.tab.h"
#include "schedule.h"
void yyerror(char *s);
char * removeQuotes(char * fullStr, int strLen, Bool unescQuotes);
int convertToActionType(char typeChar);

#define VALUE_OR_ANY (yytext[0] == '*' ? -1 : atoi(yytext)) 

extern YYSTYPE yylval;
extern int yylineno;
%}
%option noyywrap

id          [a-zA-Z0-9][a-zA-Z0-9_-]*
digit       [0-9]
any         \*
dur         {digit}+
num         [-]?{digit}+
calNum      {digit}+
qtext       \"[^\"\n]*[\"\n]
text        [^\n]*
comment     ^\/\/.*
ws          [ \t]+
nl          {ws}?\n
actionInd   [ADO]

%x S_ACTION S_CAL

%%

{comment}     ;

^#{id} { 
    yylval.strVal = strdup(yytext); 
    BEGIN (S_ACTION);
    return ACTION;
};

^{calNum} { 
    yylval.intVal = atoi(yytext); 
    BEGIN (S_CAL);
    return NUM; 
};

^{any} { 
    BEGIN (S_CAL);
    return ANY; 
};

<S_CAL>{
{any}       {   return ANY; }
{calNum}    { 
                yylval.intVal = atoi(yytext); 
                return NUM; 
            }
{qtext}     { 
                /* if quote is escaped, continue loading the quoted text */
                if (yytext[yyleng-2] == '\\') {
                    yyless(yyleng-1);   // Place quote back in stream
                    yymore();           // Append next token to this token
                }
                else {
                    yylval.strVal = removeQuotes(yytext, yyleng, True); 
                    return QTEXT; 
                }
            };
#{id}       { 
                yylval.strVal = strdup(yytext); 
                return ACTION; 
            }
[- ,/]      {   return yytext[0];}
{nl}        {   
                yylineno++;
                BEGIN (INITIAL);
            };
.           {   yyerror("Invalid value for calendar task entry"); }
};

<S_ACTION>[ ] {return yytext[0];};

<S_ACTION>{actionInd} { 
                yylval.intVal = convertToActionType(yytext[0]); 
                BEGIN (INITIAL);
                return ACT_EXEC_TYPE;
            };

<S_ACTION>{qtext}  {
            /* if quote is escaped, continue loading the quoted text */
            if (yytext[yyleng-2] == '\\') {
                yyless(yyleng-1);   // Place quote back in stream
                yymore();           // Append next token to this token
            }
            else {
                yylval.strVal = removeQuotes(yytext, yyleng, True); 
                return QTEXT; 
            }
        };

{qtext}       { yylval.strVal = removeQuotes(yytext, yyleng, False); return QTEXT; };

{nl}          {yylineno++;};

.             {return yytext[0];};

%%
void yyerror(char *s) {
    printf("line: %d: %s at %s\n", yylineno, s, yytext);
}

/* Remove beginning and ending quotes from string.  Optionally convert internal escaped quotes to standard quotes */
char * removeQuotes(char * fullStr, int strLen, Bool unescQuotes) {
    int idx = 0, offset = 0;
    char * workStr;
    workStr = strdup(fullStr+1); /* Remove initial quote */
    if (workStr[strLen-2] != '"') {
        yyerror("Unterminated character string");
    }
    else {
        workStr[strLen-2] = 0; /* Null terminate at ending quote */
    }

    // Replace escaped quotes with plain quotes if requested
    if (unescQuotes == True) {
        for (offset = 0; workStr[offset] != 0; offset++, idx++) {
            if (workStr[offset] == '\\' && workStr[offset + 1] == '\"') {
                offset++;
            }
            workStr[idx] = workStr[offset];
        }
        // Fill in rest of string with NULL 
        for (; idx < strLen; idx++) {
            workStr[idx] = 0;
        }
    }
    return workStr;
}
int convertToActionType(char typeChar) {
    switch(typeChar) {
    case 'A': 
        return ALWAYS;
    case 'D': 
        return DEFAULT;
    case 'O': 
        return ON_DEMAND;
    default:
        yyerror("Invalid Action Type: Must be A|D|O");
        return -1;
    }

}

