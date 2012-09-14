#include "builtin.h"
#include "cache.h"
#include "commit.h"
#include "tag.h"
#include "refs.h"
#include "parse-options.h"
#include "diff.h"
#include "revision.h"
#include "notes-cache.h"

#define CUTOFF_DATE_SLOP 86400 /* one day */

struct rev_name {
	const char *tip_name;
	int generation;
	int distance;
	int weight;
};

/*
 * Historically, "name-rev" named a rev based on the tip that is
 * topologically closest to it.
 *
 * It does not give a good answer to "what is the earliest tag that
 * contains the commit?", however, because you can build a new commit
 * on top of an ancient commit X, merge it to the tip and tag the
 * result, which would make X reachable from the new tag in two hops,
 * even though it appears in the part of the history that is contained
 * in other ancient tags.
 *
 * In order to answer that question, "name-rev" can be told to name a
 * rev based on the tip that has smallest number of commits behind it.
 */
static int use_weight;

/* To optimize revision traversal */
static struct commit *painted_commit;

static int compute_ref_weight(struct commit *commit)
{
	struct rev_info revs;
	int weight = 1; /* give root the weight of 1 */

	reset_revision_walk();
	init_revisions(&revs, NULL);
	add_pending_object(&revs, (struct object *)commit, NULL);
	prepare_revision_walk(&revs);
	while (get_revision(&revs))
		weight++;
	painted_commit = commit;
	return weight;
}

static struct notes_cache weight_cache;
static int weight_cache_updated;

static int get_ref_weight(struct commit *commit)
{
	struct strbuf buf = STRBUF_INIT;
	size_t sz;
	int weight;
	char *note;

	note = notes_cache_get(&weight_cache, commit->object.sha1, &sz);
	if (note && !strtol_i(note, 10, &weight)) {
		free(note);
		return weight;
	}
	free(note);

	weight = compute_ref_weight(commit);
	strbuf_addf(&buf, "%d", weight);
	notes_cache_put(&weight_cache, commit->object.sha1,
			buf.buf, buf.len);
	strbuf_release(&buf);
	weight_cache_updated = 1;
	return weight;
}

static struct commit *ref_commit(const char *refname, size_t reflen)
{
	struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];
	struct commit *commit;

	strbuf_add(&buf, refname, reflen);
	if (get_sha1(buf.buf, sha1))
		die("Internal error: cannot parse tip '%s'", buf.buf);
	strbuf_release(&buf);

	commit = lookup_commit_reference_gently(sha1, 0);
	if (!commit)
		die("Internal error: cannot look up commit '%s'", buf.buf);
	return commit;
}

static int ref_weight(struct commit *commit, const char *refname, size_t reflen)
{
	struct rev_name *name;

	name = commit->util;
	if (!name)
		die("Internal error: a tip without name '%.*s'", (int) reflen, refname);
	if (!name->weight)
		name->weight = get_ref_weight(commit);
	return name->weight;
}

static int tip_weight_cmp(const char *a, const char *b)
{
	size_t reflen_a, reflen_b;
	struct commit *commit_a, *commit_b;
	static const char traversal[] = "^~";

	/*
	 * A "tip" may look like <refname> followed by traversal
	 * instruction (e.g. ^2~74).  We only are interested in
	 * the weight of the ref part.
	 */
	reflen_a = strcspn(a, traversal);
	reflen_b = strcspn(b, traversal);

	if (reflen_a == reflen_b && !memcmp(a, b, reflen_a))
		return 0;

	commit_a = ref_commit(a, reflen_a);
	commit_b = ref_commit(b, reflen_b);

	/* Have we painted either one of these recently? */
	if (commit_a == painted_commit &&
	    (commit_b->object.flags & SHOWN)) {
		/*
		 * We know b can be reached from a, so b must be older
		 * (lighter, as it has fewer commits behind it) than
		 * a.
		 */
		return 1;
	} else if (commit_b == painted_commit &&
		   (commit_a->object.flags & SHOWN)) {
		/* Likewise */
		return -1;
	}

	return ref_weight(commit_a, a, reflen_a) - ref_weight(commit_b, b, reflen_b);
}

static long cutoff = LONG_MAX;

/* How many generations are maximally preferred over _one_ merge traversal? */
#define MERGE_TRAVERSAL_WEIGHT 65535

static void name_rev(struct commit *commit,
		     const char *tip_name, int generation, int distance,
		     int deref)
{
	struct rev_name *name = (struct rev_name *)commit->util;
	struct commit_list *parents;
	int parent_number;
	int use_this_tip = 0;

	if (!commit->object.parsed)
		parse_commit(commit);

	if (commit->date < cutoff)
		return;

	if (deref) {
		char *new_name = xmalloc(strlen(tip_name)+3);
		strcpy(new_name, tip_name);
		strcat(new_name, "^0");
		tip_name = new_name;

		if (generation)
			die("generation: %d, but deref?", generation);
	}

	if (!name) {
		name = xcalloc(1, sizeof(struct rev_name));
		commit->util = name;
		use_this_tip = 1;
	}

	if (!use_weight) {
		if (distance < name->distance)
			use_this_tip = 1;
	} else {
		if (!name->tip_name)
			use_this_tip = 1;
		else {
			/*
			 * Pick a name based on the ref that is older,
			 * i.e. having smaller number of commits
			 * behind it.  Break the tie by picking the
			 * path with smaller numer of steps to reach
			 * that ref from the commit.
			 */
			int cmp = tip_weight_cmp(name->tip_name, tip_name);
			if (0 < cmp)
				use_this_tip = 1;
			else if (!cmp && distance < name->distance)
				use_this_tip = 1;
		}
	}

	if (!use_this_tip)
		return;

	name->tip_name = tip_name;
	name->generation = generation;
	name->distance = distance;

	/* Propagate our name to our parents */
	for (parents = commit->parents, parent_number = 1;
	     parents;
	     parents = parents->next, parent_number++) {
		if (parent_number > 1) {
			int len = strlen(tip_name);
			char *new_name = xmalloc(len +
				1 + decimal_length(generation) +  /* ~<n> */
				1 + 2 +				  /* ^NN */
				1);

			if (len > 2 && !strcmp(tip_name + len - 2, "^0"))
				len -= 2;
			if (generation > 0)
				sprintf(new_name, "%.*s~%d^%d", len, tip_name,
					generation, parent_number);
			else
				sprintf(new_name, "%.*s^%d", len, tip_name,
					parent_number);
			name_rev(parents->item, new_name, 0,
				 distance + MERGE_TRAVERSAL_WEIGHT, 0);
		} else {
			name_rev(parents->item, tip_name, generation + 1,
				 distance + 1, 0);
		}
	}
}

struct name_ref_data {
	int tags_only;
	int name_only;
	const char *ref_filter;
};

static int name_ref(const char *path, const unsigned char *sha1, int flags, void *cb_data)
{
	struct object *o = parse_object(sha1);
	struct name_ref_data *data = cb_data;
	int deref = 0;

	if (data->tags_only && prefixcmp(path, "refs/tags/"))
		return 0;

	if (data->ref_filter && fnmatch(data->ref_filter, path, 0))
		return 0;

	while (o && o->type == OBJ_TAG) {
		struct tag *t = (struct tag *) o;
		if (!t->tagged)
			break; /* broken repository */
		o = parse_object(t->tagged->sha1);
		deref = 1;
	}
	if (o && o->type == OBJ_COMMIT) {
		struct commit *commit = (struct commit *)o;

		if (!prefixcmp(path, "refs/heads/"))
			path = path + 11;
		else if (data->tags_only
		    && data->name_only
		    && !prefixcmp(path, "refs/tags/"))
			path = path + 10;
		else if (!prefixcmp(path, "refs/"))
			path = path + 5;

		name_rev(commit, xstrdup(path), 0, 0, deref);
	}
	return 0;
}

/* returns a static buffer */
static const char *get_rev_name(const struct object *o)
{
	static char buffer[1024];
	struct rev_name *n;
	struct commit *c;

	if (o->type != OBJ_COMMIT)
		return NULL;
	c = (struct commit *) o;
	n = c->util;
	if (!n)
		return NULL;

	if (!n->generation)
		return n->tip_name;
	else {
		int len = strlen(n->tip_name);
		if (len > 2 && !strcmp(n->tip_name + len - 2, "^0"))
			len -= 2;
		snprintf(buffer, sizeof(buffer), "%.*s~%d", len, n->tip_name,
				n->generation);

		return buffer;
	}
}

static void show_name(const struct object *obj,
		      const char *caller_name,
		      int always, int allow_undefined, int name_only)
{
	const char *name;
	const unsigned char *sha1 = obj->sha1;

	if (!name_only)
		printf("%s ", caller_name ? caller_name : sha1_to_hex(sha1));
	name = get_rev_name(obj);
	if (name)
		printf("%s\n", name);
	else if (allow_undefined)
		printf("undefined\n");
	else if (always)
		printf("%s\n", find_unique_abbrev(sha1, DEFAULT_ABBREV));
	else
		die("cannot describe '%s'", sha1_to_hex(sha1));
}

static char const * const name_rev_usage[] = {
	N_("git name-rev [options] <commit>..."),
	N_("git name-rev [options] --all"),
	N_("git name-rev [options] --stdin"),
	NULL
};

static void name_rev_line(char *p, struct name_ref_data *data)
{
	int forty = 0;
	char *p_start;
	for (p_start = p; *p; p++) {
#define ishex(x) (isdigit((x)) || ((x) >= 'a' && (x) <= 'f'))
		if (!ishex(*p))
			forty = 0;
		else if (++forty == 40 &&
			 !ishex(*(p+1))) {
			unsigned char sha1[40];
			const char *name = NULL;
			char c = *(p+1);
			int p_len = p - p_start + 1;

			forty = 0;

			*(p+1) = 0;
			if (!get_sha1(p - 39, sha1)) {
				struct object *o =
					lookup_object(sha1);
				if (o)
					name = get_rev_name(o);
			}
			*(p+1) = c;

			if (!name)
				continue;

			if (data->name_only)
				printf("%.*s%s", p_len - 40, p_start, name);
			else
				printf("%.*s (%s)", p_len, p_start, name);
			p_start = p + 1;
		}
	}

	/* flush */
	if (p_start != p)
		fwrite(p_start, p - p_start, 1, stdout);
}

static const char *get_validity_token(void)
{
	/*
	 * In future versions, we may want to automatically invalidate
	 * the cached weight data whenever grafts and replacement
	 * changes.  We could do so by (1) reading the contents of the
	 * grafts file, (2) enumerating the replacement data (original
	 * object name and replacement object name) and sorting the
	 * result, and (3) concatenating (1) and (2) and hashing it,
	 * to come up with "dynamic validity: [0-9a-f]{40}" or something.
	 *
	 * In this verison, we simply do not bother ;-).
	 */
	return "static validity token";
}

int cmd_name_rev(int argc, const char **argv, const char *prefix)
{
	struct object_array revs = OBJECT_ARRAY_INIT;
	int all = 0, transform_stdin = 0, allow_undefined = 1, always = 0;
	struct name_ref_data data = { 0, 0, NULL };
	struct option opts[] = {
		OPT_BOOLEAN(0, "name-only", &data.name_only, N_("print only names (no SHA-1)")),
		OPT_BOOLEAN(0, "tags", &data.tags_only, N_("only use tags to name the commits")),
		OPT_STRING(0, "refs", &data.ref_filter, N_("pattern"),
				   N_("only use refs matching <pattern>")),
		OPT_GROUP(""),
		OPT_BOOLEAN(0, "all", &all, N_("list all commits reachable from all refs")),
		OPT_BOOLEAN(0, "stdin", &transform_stdin, N_("read from stdin")),
		OPT_BOOLEAN(0, "undefined", &allow_undefined, N_("allow to print `undefined` names")),
		OPT_BOOLEAN(0, "always",     &always,
			   N_("show abbreviated commit object as fallback")),
		OPT_BOOLEAN(0, "weight", &use_weight,
			    N_("name revs based on the oldest tip that contain them")),
		OPT_END(),
	};

	git_config(git_default_config, NULL);
	argc = parse_options(argc, argv, prefix, opts, name_rev_usage, 0);
	if (!!all + !!transform_stdin + !!argc > 1) {
		error("Specify either a list, or --all, not both!");
		usage_with_options(name_rev_usage, opts);
	}
	if (all || transform_stdin)
		cutoff = 0;

	if (use_weight)
		notes_cache_init(&weight_cache, "name-rev-weight",
				 get_validity_token());

	for (; argc; argc--, argv++) {
		unsigned char sha1[20];
		struct object *o;
		struct commit *commit;

		if (get_sha1(*argv, sha1)) {
			fprintf(stderr, "Could not get sha1 for %s. Skipping.\n",
					*argv);
			continue;
		}

		o = deref_tag(parse_object(sha1), *argv, 0);
		if (!o || o->type != OBJ_COMMIT) {
			fprintf(stderr, "Could not get commit for %s. Skipping.\n",
					*argv);
			continue;
		}

		commit = (struct commit *)o;
		if (cutoff > commit->date)
			cutoff = commit->date;
		add_object_array((struct object *)commit, *argv, &revs);
	}

	if (cutoff)
		cutoff = cutoff - CUTOFF_DATE_SLOP;
	for_each_ref(name_ref, &data);

	if (transform_stdin) {
		char buffer[2048];

		while (!feof(stdin)) {
			char *p = fgets(buffer, sizeof(buffer), stdin);
			if (!p)
				break;
			name_rev_line(p, &data);
		}
	} else if (all) {
		int i, max;

		max = get_max_object_index();
		for (i = 0; i < max; i++) {
			struct object *obj = get_indexed_object(i);
			if (!obj || obj->type != OBJ_COMMIT)
				continue;
			show_name(obj, NULL,
				  always, allow_undefined, data.name_only);
		}
	} else {
		int i;
		for (i = 0; i < revs.nr; i++)
			show_name(revs.objects[i].item, revs.objects[i].name,
				  always, allow_undefined, data.name_only);
	}

	if (use_weight && weight_cache_updated)
		notes_cache_write(&weight_cache);

	return 0;
}
