#include "cache.h"
#include "commit.h"
#include "diff.h"
#include "revision.h"


struct gdata {
	int generation;
	struct commit *youngest_ancestor;
};

static struct gdata *util_gd(struct commit *commit)
{
	return commit->util;
}

static void show_commit(struct commit *commit, struct gdata *gd)
{
	printf("%s %d",
	       find_unique_abbrev(commit->object.sha1, DEFAULT_ABBREV),
	       gd->generation);
	if (gd->youngest_ancestor != commit) {
		struct commit *ancestor = gd->youngest_ancestor;
		const char *abbrev;

		abbrev = find_unique_abbrev(ancestor->object.sha1,
					    DEFAULT_ABBREV);
		printf(" %s ", show_date(commit->date, 0, DATE_ISO8601));
		printf("(%lu) ", ancestor->date - commit->date);
		printf("%d", util_gd(ancestor)->generation);
		printf(" %s", abbrev);
	}
	putchar('\n');
}

int main(int ac, const char **av)
{
	struct rev_info revs;
	struct setup_revision_opt opt;
	struct commit_list *list;
	struct commit_list *stuck = NULL;

	memset(&opt, 0, sizeof(opt));
	opt.def = "HEAD";
	init_revisions(&revs, NULL);
	setup_revisions(ac, av, &revs, &opt);
	prepare_revision_walk(&revs);

	list = revs.commits;
	while (list || stuck) {
		struct commit_list *parent, *next;
		struct commit *commit;
		struct gdata *gd;
		int ready = 1;
		int parent_generation;
		struct commit *youngest_ancestor;

		if (!list) {
			list = stuck;
			stuck = NULL;
		}
		commit = list->item;
		youngest_ancestor = commit;
		parent_generation = 0;
		parse_commit(commit);
		if (!commit->util)
			commit->util = xcalloc(1, sizeof(*gd));
		gd = commit->util;
		if (gd->generation) {
			/* we have handled this already */
			next = list->next;
			free(list);
			list = next;
			continue;
		}

		for (parent = commit->parents; parent; parent = parent->next) {
			struct commit *p = parent->item;
			struct gdata *pgd = p->util;

			/* queue to the front */
			commit_list_insert(p, &list);
			if (!pgd || !pgd->generation) {
				ready = 0;
				continue;
			}
			if (parent_generation < pgd->generation)
				parent_generation = pgd->generation;
			if (youngest_ancestor->date < pgd->youngest_ancestor->date)
				youngest_ancestor = pgd->youngest_ancestor;
		}
		if (!ready) {
			commit_list_insert(commit, &stuck);
			continue;
		}
		gd->generation = parent_generation + 1;
		gd->youngest_ancestor = youngest_ancestor;

		next = list->next;
		free(list);
		list = next;

		show_commit(commit, gd);
	}
}
