#ifndef SVNDUMP_H_
#define SVNDUMP_H_
#include "cache.h"

int svndump_init(const char *filename);
int svndump_init_fd(int in_fd, int back_fd);
void svndump_read(const char *url, const char *local_ref, const char *notes_ref);
void svndump_deinit(void);
void svndump_reset(void);

struct node_ctx_t {
	uint32_t action, srcRev, type;
	off_t prop_length, text_length;
	struct strbuf src, dst;
	uint32_t text_delta, prop_delta;
	const char *dataref;
	struct node_ctx_t *next;
};

struct rev_ctx_t {
	uint32_t revision;
	unsigned long timestamp;
	struct strbuf log, author, note;
};

struct dump_ctx_t {
	uint32_t version;
	struct strbuf uuid, url;
};

#endif
