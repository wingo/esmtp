/**
 * \file message.c
 * Simple message handling.
 * 
 * \author José Fonseca
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"
#include "local.h"
#include "rfc822.h"


message_t *message_new(void)
{
	message_t *message;

	if(!(message = (message_t *)malloc(sizeof(message_t))))
		return NULL;

	memset(message, 0, sizeof(message_t));

	INIT_LIST_HEAD(&message->remote_recipients);
	INIT_LIST_HEAD(&message->local_recipients);
	
	message->notify = Notify_NOTSET;
		
	return message;
}

void message_free(message_t *message)
{
	struct list_head *ptr, *tmp;
	recipient_t *recipient;
	
	if(message->reverse_path)
		free(message->reverse_path);

	if(!list_empty(&message->remote_recipients))
		list_for_each_safe(ptr, tmp, &message->remote_recipients)
		{
			recipient = list_entry(ptr, recipient_t, list);
			list_del(ptr);
			
			if(recipient->address)
				free(recipient->address);
					
			free(ptr);
		}

	if(!list_empty(&message->local_recipients))
		list_for_each_safe(ptr, tmp, &message->local_recipients)
		{
			recipient = list_entry(ptr, recipient_t, list);
			list_del(ptr);
			
			if(recipient->address)
				free(recipient->address);
					
			free(ptr);
		}

	if(message->fp)
		fclose(message->fp);
	
	free(message);
}
	
int message_set_reverse_path(message_t *message, const char *address)
{
	if(message->reverse_path)
		free(message->reverse_path);

	if(!(message->reverse_path = strdup(address)))
		return 0;

	return 1;
}

int message_add_recipient(message_t *message, const char *address)
{
	recipient_t *recipient;

	if(!(recipient = (recipient_t *)malloc(sizeof(recipient_t))))
		return 0;

	if(!(recipient->address = strdup(address)))
	{
		free(recipient);
		return 0;
	}

	if(local_address(address))
		list_add(&recipient->list, &message->local_recipients);
	else
		list_add(&recipient->list, &message->remote_recipients);

	return 1;
}

static int message_buffer_alloc(message_t *message)
{
	char *buffer;
	size_t buffer_size;

	if(!message->buffer_size)
		buffer_size = BUFSIZ;
	else
		buffer_size = message->buffer_size << 1;
	
	if(!(buffer = (char *)realloc(message->buffer, buffer_size)))
		return 0;

	message->buffer = buffer;
	message->buffer_size = buffer_size;

	return 1;
}

static char *message_buffer_readline(message_t *message)
{
	FILE *fp = message->fp ? message->fp : stdin;
	size_t ret = message->buffer_stop, n;
	
	while(1)
	{
		if(message->buffer_stop >= message->buffer_size - 1 && !message_buffer_alloc(message))
			return NULL;

		if(!fgets(message->buffer + message->buffer_stop, message->buffer_size - message->buffer_stop, fp))
			return NULL;
		
		n = strlen(message->buffer + message->buffer_stop);
		message->buffer_stop += n;
		
		if(*(message->buffer + message->buffer_stop - 1) == '\n')
			return message->buffer + ret;
	}
}

static void message_buffer_fill(message_t *message)
{
	FILE *fp = message->fp ? message->fp : stdin;

	message->buffer_stop += fread(message->buffer, 1, message->buffer_size - message->buffer_stop, fp);
}

static size_t message_buffer_flush(message_t *message, char *ptr, size_t size)
{
	size_t count, n;
	char *p, *q;
	
	p = message->buffer + message->buffer_start;
	count = 0;
	while(count < size && message->buffer_start < message->buffer_stop)
	{
		q = memchr(p, '\n', message->buffer_stop - message->buffer_start);
		
		if(q)
			/* read up to the newline */
			n = q - p;
		else
			/* read up to the end of the buffer */
			n = message->buffer_stop - message->buffer_start;

		if(n)
		{
			if(n > (size - count))
				n = size - count;
			
			memcpy(ptr, p, n);

			p += n;
			message->buffer_start += n;
			ptr += n;
			count += n;

			message->buffer_r = *(p - 1) == '\r';
		}

		if(count == size)
			return count;
		
		if(q)
		{
			if(!message->buffer_r)
			{
				*ptr++ = '\r';
				count++;
				
				if(count == size)
				{
					message->buffer_r = 1;
					return count;
				}
			}
			else
				message->buffer_r = 0;
			
			*ptr++ = *p++;	/* '\n' */
			message->buffer_start++;
			count++;
		}
	}

	if(message->buffer_start == message->buffer_stop)
		message->buffer_start = message->buffer_stop = 0;
	
	return count;
}
	
size_t message_read(message_t *message, char *ptr, size_t size)
{
	size_t count = 0, n;

	if(!message->buffer && !message_buffer_alloc(message))
		return 0;

	n = message_buffer_flush(message, ptr, size);
	count += n;
	ptr += n;
	
	while(count != size)
	{
		message_buffer_fill(message);
		
		if(!(n = message_buffer_flush(message, ptr, size - count)))
			break;
		count += n;
		ptr += n;

	};
		
	return count;
}

void message_rewind(message_t *message)
{
	FILE *fp = message->fp ? message->fp : stdin;
	
	message->buffer_start = message->buffer_stop = 0;
	message->buffer_r = 0;
	
	rewind(fp);
}

int message_eof(message_t *message)
{
	FILE *fp = message->fp ? message->fp : stdin;
	
	if(message->buffer_start != message->buffer_stop)
		return 0;

	return feof(fp);
}

static int message_parse_header(message_t *message, size_t start, size_t stop)
{
	const char *address;
	char *header, *next, c;
	
	header = message->buffer + start;
	next = message->buffer + stop;
	
	c = *next;
	*next = '\0';
	
	if(!strncasecmp("From: ", header, 6))
	{
		if((address = next_address(header)))
			if(!message_set_reverse_path(message, address))
				return 0;
	}
	else if(!strncasecmp("To: ", header, 4) || 
			!strncasecmp("Cc: ", header, 4) || 
			!strncasecmp("Bcc: ", header, 5))
	{
		address = next_address(header);
		while(address)
		{
			if(!message_add_recipient(message, address))
				return 0;
			address = next_address(NULL);
		}

	}
	
	*next = c;
	
	if(!strncasecmp("Bcc: ", header, 5))
	{
		size_t n = message->buffer_stop - stop;

		memcpy(header, next, n);

		message->buffer_stop = start + n;
	}

	return 1;
}

int message_parse_headers(message_t *message)
{
	FILE *fp = message->fp ? message->fp : stdin;
	char *line, *header;
	size_t start, stop;

	assert(!message->buffer);

	if(!message_buffer_alloc(message))
		return 0;

	start = 0;
	while((line = message_buffer_readline(message)))
	{
		if(line[0] == ' ' || line[0] == '\t')
		{
			/* append line */
		}
		else
		{
			stop = line - message->buffer;
			if(stop && !message_parse_header(message, start, stop))
				return 0;

			start = stop;

			if(line[0] == '\n')
				return 1;
		}
	}
	
	return 0;
}

#ifdef TEST
int local_address(const char *address)
{
	return !strchr(address, '@');
}

int main(int argc, char *argv[])
{
	message_t *message = message_new();
	const size_t len = 8192;
	size_t n;
	char buf[len];
	unsigned i;
	FILE *fpin, *fpout;
	struct list_head *ptr;
	int ret;

	fpin = fopen("test.in", "r");
	fpout = fopen("test.out", "w");
	message->fp = fpin;
	ret = message_parse_headers(message);
	do {
		n = message_read(message, buf, len);
		fwrite(buf, 1, n, fpout);
	} while(n == len);
	
	printf("%d %s\n", ret, message->reverse_path);
	list_for_each(ptr, &message->local_recipients)
	{
		recipient_t *recipient = list_entry(ptr, recipient_t, list);
		
		if(recipient->address)
			printf("%s\n",recipient->address);
	}
	list_for_each(ptr, &message->remote_recipients)
	{
		recipient_t *recipient = list_entry(ptr, recipient_t, list);
		
		if(recipient->address)
			printf("%s\n",recipient->address);
	}
	
	message_free(message);
}

#endif
