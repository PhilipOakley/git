#!/bin/sh

echo "/* Automatically generated by $0 */
struct cmdname_help {
    char name[20];
    char help[80];
};

static struct cmdname_help common_cmds[] = {"

sed -n -e 's/^git-\([^ 	]*\)[ 	].* common.*/\1/p' command-list.txt |
sort |
while read cmd
do
     sed -n '
     /^NAME/,/git-'"$cmd"'/H
     ${
	    x
	    s/.*git-'"$cmd"' - \(.*\)/  {"'"$cmd"'", N_("\1")},/
	    p
     }' "Documentation/git-$cmd.txt"
done
echo "};"
