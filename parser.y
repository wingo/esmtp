%{
/**
 * \file parser.y
 * Parser for the rcfile.
 *
 * \author Adapted from fetchmail's rcfile_y.y by José Fonseca
 */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libesmtp.h>

#include "main.h"
#include "smtp.h"
#include "local.h"


/* parser reads these */
char *rcfile = NULL;		/* path name of dot file */

/* parser sets these */
int yydebug;			/* in case we didn't generate with -- debug */

static identity_t *identity = NULL;

/* using Bison, this arranges that yydebug messages will show actual tokens */
extern char * yytext;
#define YYPRINT(fp, type, val)	fprintf(fp, " = \"%s\"", yytext)

void yyerror (const char *s);
%}

%union {
    int number;
    char *sval;
}

%token IDENTITY HOSTNAME USERNAME PASSWORD STARTTLS CERTIFICATE_PASSPHRASE MDA

%token MAP

%token DISABLED ENABLED REQUIRED
%token <sval>  STRING
%token <number> NUMBER

%%

rcfile		: /* empty */
		| statement_list
		| statement_list identity_list
		| identity_list
		;

identity_list	: identity
		| identity statement_list
		| identity_list identity statement_list
		;


map		: /* empty */
		| MAP
		;

identity	: IDENTITY map STRING
			{
				identity = identity_new();
				identity_add(identity);
				identity->address = strdup($3);
			}
		;

statement_list	: statement
		| statement_list statement
		;

/* future global options should also have the form SET <name> optmap <value> */
statement	: HOSTNAME map STRING	{ identity->host = strdup($3); }
		| USERNAME map STRING	{ identity->user = strdup($3); }
		| PASSWORD map STRING	{ identity->pass = strdup($3); }
		| STARTTLS map DISABLED	{ identity->starttls = Starttls_DISABLED; }
		| STARTTLS map ENABLED	{ identity->starttls = Starttls_ENABLED; }
		| STARTTLS map REQUIRED	{ identity->starttls = Starttls_REQUIRED; }
		| CERTIFICATE_PASSPHRASE map STRING { identity->certificate_passphrase = strdup($3); }
		| MDA map STRING	{ mda = strdup($3); }
		;

%%

/* lexer interface */
extern int lineno;
extern char *yytext;
extern FILE *yyin;

void yyerror (const char *s)
/* report a syntax error */
{
	fprintf(stderr, "%s:%d: %s at %s\n", rcfile, lineno, s, 
		(yytext && yytext[0]) ? yytext : "end of input");
	exit(EX_CONFIG);
}

#define RCFILE ".esmtprc"

void parse_rcfile (void)
{
    if(!rcfile)
    {
	char *home;

	/* Setup the rcfile name. */
	if (!(home = getenv("HOME")))
	    return;
	    
	if (!(rcfile = malloc(strlen(home) + strlen(RCFILE) + 2)))
	    return;

	strcpy(rcfile, home);
	if (rcfile[strlen(rcfile) - 1] != '/')
	  strcat(rcfile, "/");
	strcat(rcfile, RCFILE);
    }
 
    identity = default_identity;

    /* Open the configuration file and feed it to the lexer. */
    if (!(yyin = fopen(rcfile, "r")))
    {
    	if (errno != ENOENT)
	{
	    fprintf(stderr, "open: %s: %s\n", rcfile, strerror(errno));
	}
    }
    else 
    {
	yyparse();		/* parse entire file */

	fclose(yyin);	/* not checking this should be safe, file mode was r */
    }
    
    free(rcfile);
}

/* easier to do this than cope with variations in where the library lives */
int yywrap(void) { return 1; }
