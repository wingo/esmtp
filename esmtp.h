/*
 * esmtp.h - global declarations
 */

typedef struct {
	char *identity;
	char *host;
	char *user;
	char *pass;
	enum starttls_option starttls; /* it should default to Starttls_DISABLED */
	char *certificate_passphrase;
} identity_t;

extern identity_t default_identity;

typedef struct identity_list_rec identity_list_t;

struct identity_list_rec {
	identity_list_t *next;
	identity_t identity;
} ;

extern identity_list_t *identities_head, **identities_tail;


extern char *rcfile;


extern void parse_rcfile(void);

