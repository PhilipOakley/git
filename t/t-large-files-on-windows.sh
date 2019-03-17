#!/bin/sh

test_description='test large file handling on windows'
. ./test-lib.sh

test_expect_success SIZE_T_IS_64BIT 'require 64bit size_t' '

	test-tool zlib-compile-flags >zlibFlags.txt &&
	dd if=/dev/zero of=file bs=1M count=4100 &&
	git config core.compression 0 &&
	git config core.looseCompression 0 &&
	git add file &&
	git verify-pack -s .git/objects/pack/*.pack &&
	git fsck --verbose --strict --full &&
	git commit -m msg file &&
	git log --stat &&
	git gc &&
	git fsck --verbose --strict --full &&
	git index-pack -v -o test.idx .git/objects/pack/*.pack &&
	git gc &&
	git fsck
'

test_done
