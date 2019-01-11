#!/bin/sh
#
# Copyright (c) 2018 Jiang Xin
#

test_description='git redundant test'

. ./test-lib.sh

create_commits()
{
	set -e
	parent=
	for name in A B C D E F G H I J K L M
	do
		test_tick
		T=$(git write-tree)
		if test -z "$parent"
		then
			sha1=$(echo $name | git commit-tree $T)
		else
			sha1=$(echo $name | git commit-tree -p $parent $T)
		fi
		eval $name=$sha1
		parent=$sha1
	done
	git update-ref refs/heads/master $M
}

create_redundant_packs()
{
	set -e
	cd .git/objects/pack
	P1=$(printf "$T\n$A\n" | git pack-objects pack 2>/dev/null)
	P2=$(printf "$T\n$A\n$B\n$C\n$D\n$E\n" | git pack-objects pack 2>/dev/null)
	P3=$(printf "$C\n$D\n$F\n$G\n$I\n$J\n" | git pack-objects pack 2>/dev/null)
	P4=$(printf "$D\n$E\n$G\n$H\n$J\n$K\n" | git pack-objects pack 2>/dev/null)
	P5=$(printf "$F\n$G\n$H\n" | git pack-objects pack 2>/dev/null)
	P6=$(printf "$F\n$I\n$L\n" | git pack-objects pack 2>/dev/null)
	P7=$(printf "$H\n$K\n$M\n" | git pack-objects pack 2>/dev/null)
	P8=$(printf "$L\n$M\n" | git pack-objects pack 2>/dev/null)
	cd -
	eval P$P1=P1:$P1
	eval P$P2=P2:$P2
	eval P$P3=P3:$P3
	eval P$P4=P4:$P4
	eval P$P5=P5:$P5
	eval P$P6=P6:$P6
	eval P$P7=P7:$P7
	eval P$P8=P8:$P8
}

# Create commits and packs
create_commits
create_redundant_packs

test_expect_success 'clear loose objects' '
	git prune-packed &&
	test $(find .git/objects -type f | grep -v pack | wc -l) -eq 0
'

cat >expected <<EOF
P1:$P1
P4:$P4
P5:$P5
P6:$P6
EOF

test_expect_success 'git pack-redundant --all' '
	git pack-redundant --all | \
		sed -e "s#^.*/pack-\(.*\)\.\(idx\|pack\)#\1#g" | \
		sort -u | \
		while read p; do eval echo "\${P$p}"; done | \
		sort > actual && \
	test_cmp expected actual
'

test_expect_success 'remove redundant packs' '
	git pack-redundant --all | xargs rm &&
	git fsck &&
	test $(git pack-redundant --all | wc -l) -eq 0
'

test_done
