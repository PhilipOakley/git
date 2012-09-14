#!/bin/sh

test_description='name-rev'
. ./test-lib.sh

# Prepare a history with this shape
#
# ---0--1--2--3--4--Y--5---7---Z
#     \   /               /
#      \ /               /
#       X---------------6
#

test_expect_success setup '
	test_tick &&
	git commit --allow-empty -m 0 &&
	git branch side &&
	git commit --allow-empty -m 1 &&
	git checkout side &&
	git commit --allow-empty -m X &&
	git branch X &&
	git commit --allow-empty -m 6 &&
	git checkout master &&
	git merge -m 2 X &&
	git commit --allow-empty -m 3 &&
	git commit --allow-empty -m 4 &&
	git commit --allow-empty -m Y &&
	git tag Y &&
	git commit --allow-empty -m 5 &&
	git merge -m 7 side &&
	git commit --allow-empty -m Z &&
	git tag Z
'

test_expect_success 'name-rev (plain)' '
	# We expect "X tags/Z~1^2~1" but it could
	# be written as "X tags/Z^^2^"; the only two
	# important things that matter are that it
	# is named after Z (not Y), and it correctly
	# names X.
	git name-rev --tags X >actual &&
	read X T <actual &&
	test "z$X" = zX &&
	expr "$T" : 'tags/Z[~^]' &&
	H1=$(git rev-parse --verify "$T") &&
	H2=$(git rev-parse --verify X) &&
	test "z$H1" = "z$H2"
'

test_expect_success 'name-rev --weight' '
	# Likewise; "X tags/Y~3^2" but we only care
	# that it is based on Y.
	git name-rev --weight --tags X >actual &&
	read X T <actual &&
	test "z$X" = zX &&
	expr "$T" : 'tags/Y[~^]' &&
	H1=$(git rev-parse --verify "$T") &&
	H2=$(git rev-parse --verify X) &&
	test "z$H1" = "z$H2"
'

test_done
