%{
/*
 * lexer.l -- lexer for the rcfile
 */

/*
 * Adapted from fetchmail's rcfile_l.l by José Fonseca
 */
 
#include <stdio.h>
#include <string.h>

#include "parser.h"

#define MSGBUFSIZE 8192

int lineno = 1;

void escapes(const char *tp, char *cp);

%}

%s NAME

%%

\"[^\"]*\"	{
			char buf[MSGBUFSIZE];

			yytext[strlen(yytext)-1] = '\0';
			escapes(yytext+1, buf);
			yylval.sval = (char *) strdup(buf);
                        BEGIN(0);
			return STRING;
		}
\'[^\']*\'	{
			char buf[MSGBUFSIZE];

			yytext[strlen(yytext)-1] = '\0';
			escapes(yytext+1, buf);
			yylval.sval = (char *) strdup(buf);
                        BEGIN(0);
			return STRING;
		}

<NAME>[^=;, \t\r\n]+	{
			char buf[MSGBUFSIZE];

			escapes(yytext, buf);
			yylval.sval = (char *) strdup(buf);
                        BEGIN(0);
			return STRING;
		}



host(name)?	{ BEGIN(NAME); return HOSTNAME; }
user(name)?	{ BEGIN(NAME); return USERNAME; }
pass(word)?	{ BEGIN(NAME); return PASSWORD; }
(start)?tls	{ return STARTTLS; }
(certificate_)?passphrase	{ return CERTIFICATE_PASSPHRASE; }

=		{ return MAP; }

disabled	{ return DISABLED; }
enabled		{ return ENABLED; }
required	{ return REQUIRED; }

(#.*)?\\?\n	{ lineno++; }   /* newline is ignored */

-?[0-9]+/[^a-zA-Z]	{ yylval.number = atoi(yytext); return NUMBER; }

[^=;:, \t\r\n]+	{
			char buf[MSGBUFSIZE];

			escapes(yytext, buf);
			yylval.sval = (char *) strdup(buf);
			return STRING;
		}

[ \t\r]+	;	/* whitespace */

%%

/* process standard C-style escape sequences in a string */
void escapes(
    const char *cp,	/* source string with escapes */
    char *tp		/* target buffer for digested string */
)
{
    while (*cp)
    {
	int	cval = 0;

	if (*cp == '\\' && strchr("0123456789xX", cp[1]))
	{
	    char *dp;
	    const char *hex = "00112233445566778899aAbBcCdDeEfF";
	    int dcount = 0;

	    if (*++cp == 'x' || *cp == 'X')
		for (++cp; (dp = strchr(hex, *cp)) && (dcount++ < 2); cp++)
		    cval = (cval * 16) + (dp - hex) / 2;
	    else if (*cp == '0')
		while (strchr("01234567",*cp) != (char*)NULL && (dcount++ < 3))
		    cval = (cval * 8) + (*cp++ - '0');
	    else
		while ((strchr("0123456789",*cp)!=(char*)NULL)&&(dcount++ < 3))
		    cval = (cval * 10) + (*cp++ - '0');
	}
	else if (*cp == '\\')		/* C-style character escapes */
	{
	    switch (*++cp)
	    {
		case '\\': cval = '\\'; break;
		case 'n': cval = '\n'; break;
		case 't': cval = '\t'; break;
		case 'b': cval = '\b'; break;
		case 'r': cval = '\r'; break;
		default: cval = *cp;
	    }
	    cp++;
	}
	else
	    cval = *cp++;
	*tp++ = cval;
    }
    *tp = '\0';
}

