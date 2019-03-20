#!/bin/sh

test_description='test large file handling on windows'
. ./test-lib.sh

test_expect_success SIZE_T_IS_64BIT 'blah blubb' '

	test-tool zlib-compile-flags >zlibFlags.txt &&
	dd if=/dev/zero of=file bs=1M count=5100 &&
	git config core.compression 0 &&
	git config core.looseCompression 0 &&
	git add file &&
	git verify-pack -s .git/objects/pack/*.pack &&
	git fsck --verbose --strict --full &&
	git commit -m msg file &&
	git verify-pack -s .git/objects/pack/*.pack &&
	git log --stat &&
	git fsck --verbose --strict --full &&
	git repack -a -f &&
	git verify-pack -s .git/objects/pack/*.pack &&
	git verify-pack -v .git/objects/pack/*.pack &&
	git gc
'

test_done
