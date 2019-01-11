#!/bin/sh

test_description='git show'

. ./test-lib.sh

test_expect_success setup '
	echo hello world >foo &&
	H=$(git hash-object -w foo) &&
	git tag -a foo-tag -m "Tags $H" $H &&
	HH=$(expr "$H" : "\(..\)") &&
	H38=$(expr "$H" : "..\(.*\)") &&
	rm -f .git/objects/$HH/$H38
'

test_expect_success 'showing a tag that point at a missing object' '
	test_must_fail git --no-pager show foo-tag
'

test_expect_success 'set up a bit of history' '
	test_commit main1 &&
	test_commit main2 &&
	test_commit main3 &&
	git tag -m "annotated tag" annotated &&
	git checkout -b side HEAD^^ &&
	test_commit side2 &&
	test_commit side3 &&
	test_merge merge main3
'

test_expect_success 'showing two commits' '
	cat >expect <<-EOF &&
	commit $(git rev-parse main2)
	commit $(git rev-parse main3)
	EOF
	git show main2 main3 >actual &&
	grep ^commit actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success 'showing a range walks (linear)' '
	cat >expect <<-EOF &&
	commit $(git rev-parse main3)
	commit $(git rev-parse main2)
	EOF
	git show main1..main3 >actual &&
	grep ^commit actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success 'showing a range walks (Y shape, ^ first)' '
	cat >expect <<-EOF &&
	commit $(git rev-parse main3)
	commit $(git rev-parse main2)
	EOF
	git show ^side3 main3 >actual &&
	grep ^commit actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success 'showing a range walks (Y shape, ^ last)' '
	cat >expect <<-EOF &&
	commit $(git rev-parse main3)
	commit $(git rev-parse main2)
	EOF
	git show main3 ^side3 >actual &&
	grep ^commit actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success 'showing with -N walks' '
	cat >expect <<-EOF &&
	commit $(git rev-parse main3)
	commit $(git rev-parse main2)
	EOF
	git show -2 main3 >actual &&
	grep ^commit actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success 'showing annotated tag' '
	cat >expect <<-EOF &&
	tag annotated
	commit $(git rev-parse annotated^{commit})
	EOF
	git show annotated >actual &&
	grep -E "^(commit|tag)" actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success 'showing annotated tag plus commit' '
	cat >expect <<-EOF &&
	tag annotated
	commit $(git rev-parse annotated^{commit})
	commit $(git rev-parse side3)
	EOF
	git show annotated side3 >actual &&
	grep -E "^(commit|tag)" actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success 'showing range' '
	cat >expect <<-EOF &&
	commit $(git rev-parse main3)
	commit $(git rev-parse main2)
	EOF
	git show ^side3 annotated >actual &&
	grep -E "^(commit|tag)" actual >actual.filtered &&
	test_cmp expect actual.filtered
'

test_expect_success '-s suppresses diff' '
	cat >expect <<-\EOF &&
	merge
	main3
	EOF
	git show -s --format=%s merge main3 >actual &&
	test_cmp expect actual
'

test_expect_success '--quiet suppresses diff' '
	echo main3 >expect &&
	git show --quiet --format=%s main3 >actual &&
	test_cmp expect actual
'

test_expect_success 'show --graph is forbidden' '
  test_must_fail git show --graph HEAD
'

check_human_date() {
	commit_date=$1
	expect=$2
	test_expect_success "$commit_date" "
		echo $expect $commit_date >dates &&
		git add dates &&
		git commit -m 'Expect String' --date=\"$commit_date\" dates &&
		git show --date=human | grep \"^Date:\" >actual &&
		grep \"$expect\" actual
"
}

TODAY_REGEX='[A-Z][a-z][a-z] [012][0-9]:[0-6][0-9] .0200'
THIS_YEAR_REGEX='[A-Z][a-z][a-z] [A-Z][a-z][a-z] [0-9]* [012][0-9]:[0-6][0-9]'
MORE_THAN_A_YEAR_REGEX='[A-Z][a-z][a-z] [A-Z][a-z][a-z] [0-9]* [0-9][0-9][0-9][0-9]'
check_human_date "$(($(date +%s)-18000)) +0200" $TODAY_REGEX # 5 hours ago
check_human_date "$(($(date +%s)-432000)) +0200" $THIS_YEAR_REGEX  # 5 days ago
check_human_date "$(($(date +%s)-1728000)) +0200" $THIS_YEAR_REGEX # 3 weeks ago
check_human_date "$(($(date +%s)-13000000)) +0200" $THIS_YEAR_REGEX # 5 months ago
check_human_date "$(($(date +%s)-31449600)) +0200" $THIS_YEAR_REGEX # 12 months ago
check_human_date "$(($(date +%s)-37500000)) +0200" $MORE_THAN_A_YEAR_REGEX # 1 year, 2 months ago
check_human_date "$(($(date +%s)-55188000)) +0200" $MORE_THAN_A_YEAR_REGEX # 1 year, 9 months ago
check_human_date "$(($(date +%s)-630000000)) +0200" $MORE_THAN_A_YEAR_REGEX # 20 years ago


test_done
