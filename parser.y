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
#include "xmalloc.h"


/** Path name of dot file */
static const char *rcfile = NULL;		

int yydebug;			/**< in case we didn't generate with -- debug */

static identity_t *identity = NULL;

/**
 * Utility macro to set the default identity, if one isn't set yet. 
 * 
 * It's necessary for compatability with older configurations files where the
 * default identity was defined in a global section, instead of defined by the
 * 'default' or the first defined identity, if no 'default' keyword is given.
 */
#define SET_DEFAULT_IDENTITY						\
do {									\
	if(!default_identity)						\
		default_identity = identity;				\
} while(0)

/* using Bison, this arranges that yydebug messages will show actual tokens */
extern char * yytext;
#define YYPRINT(fp, type, val)	fprintf(fp, " = \"%s\"", yytext)

void yyerror (const char *s);


%}

%union {
    int number;
    char *sval;
}

%token IDENTITY DEFAULT HOSTNAME USERNAME PASSWORD STARTTLS CERTIFICATE_PASSPHRASE PRECONNECT POSTCONNECT MDA

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
				identity->address = xstrdup($3);
			}
		;

statement_list	: statement
		| statement_list statement
		;

/* future global options should also have the form SET <name> optmap <value> */
statement	: HOSTNAME map STRING	{ identity->host = xstrdup($3); SET_DEFAULT_IDENTITY; }
		| USERNAME map STRING	{ identity->user = xstrdup($3); SET_DEFAULT_IDENTITY; }
		| PASSWORD map STRING	{ identity->pass = xstrdup($3); SET_DEFAULT_IDENTITY; }
		| STARTTLS map DISABLED	{ identity->starttls = Starttls_DISABLED; SET_DEFAULT_IDENTITY; }
		| STARTTLS map ENABLED	{ identity->starttls = Starttls_ENABLED; SET_DEFAULT_IDENTITY; }
		| STARTTLS map REQUIRED	{ identity->starttls = Starttls_REQUIRED; SET_DEFAULT_IDENTITY; }
		| CERTIFICATE_PASSPHRASE map STRING { identity->certificate_passphrase = xstrdup($3); SET_DEFAULT_IDENTITY; }
		| PRECONNECT map STRING	{ identity->preconnect = xstrdup($3); SET_DEFAULT_IDENTITY; }
		| PRECONNECT map STRING	{ identity->postconnect = xstrdup($3); SET_DEFAULT_IDENTITY; }
		| MDA map STRING	{ mda = xstrdup($3); }
		| DEFAULT		{ default_identity = identity; }
		;

%%

/* lexer interface */
extern int lineno;
extern char *yytext;
extern FILE *yyin;

/** 
 * Report a syntax error.
 */
void yyerror (const char *s)
{
	fprintf(stderr, "%s:%d: %s at %s\n", rcfile, lineno, s, 
		(yytext && yytext[0]) ? yytext : "end of input");
	exit(EX_CONFIG);
}

#define RCFILE "esmtprc"
#define DOT_RCFILE "." RCFILE
#define ETC_RCFILE "/etc/" RCFILE

void rcfile_parse(const char *_rcfile)
{
	char *temp = NULL;

	if(_rcfile)
	{
		/* Configuration file specified on the command line */
		if(!(yyin = fopen(_rcfile, "r")))
			goto failure;
			
		rcfile = _rcfile;
		goto success;
	}
	
	/* Search for the user configuration file */
	do 
	{
		char *home;

		if (!(home = getenv("HOME")))
			break;
			
		temp = xmalloc(strlen(home) + strlen(DOT_RCFILE) + 2);

		strcpy(temp, home);
		if (temp[strlen(temp) - 1] != '/')
			strcat(temp, "/");
		strcat(temp, DOT_RCFILE);

		if(!(yyin = fopen(temp, "r")))
		{
			if(errno == ENOENT)
				break;
			else
				goto failure;
		}
		
		rcfile = temp;
		goto success;
	} 
	while(0);
	
	/* Search for the global configuration file */
	do {
		if(!(yyin = fopen(ETC_RCFILE, "r")))
		{
			if(errno == ENOENT)
				break;
			else
				goto failure;
		}

		rcfile = ETC_RCFILE;
		goto success;
	}
	while(0);
		
	/* No configuration file found */
	return;

success:
	/* Configuration file opened */
	
	identity = identity_new();
	identity_add(identity);

	yyparse();	/* parse entire file */

	SET_DEFAULT_IDENTITY;

	fclose(yyin);	/* not checking this should be safe, file mode was r */

	rcfile = NULL;
	if(temp)
		free(temp);

	return;

failure:
	/* Failure to open the configuration file */
	fprintf(stderr, "open: %s: %s\n", rcfile, strerror(errno));
	exit(EX_CONFIG);
}

/* easier to do this than cope with variations in where the library lives */
int yywrap(void) { return 1; }
