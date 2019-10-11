#!/bin/sh

test_description='test large file handling on windows'
. ./test-lib.sh

test_expect_success SIZE_T_IS_64BIT,ZLIB_IS_LLP64 'setup >4GiB file' '

	git init &&
	dd if=/dev/zero of=file bs=1M count=4100 &&
	git config core.compression 0 &&
	git config core.looseCompression 0 &&
	# git config --get-all pack.packsizelimit >pack.psl.txt &&
	# git config --unset pack.packsizelimit 
	echo "test" .tst.txt
	# do not git config --unset core.bigFileThreshold we only want to avoid delta compression and keep zlib deflation at zero compression
'

test_expect_success SIZE_T_IS_64BIT,ZLIB_IS_LLP64 '>4GiB loose object' '

	: just check zlib deflation and not the large pack crc32
	git -c pack.packsizelimit = 2g add file &&
	git fsck --verbose --strict --full 
'

test_expect_success SIZE_T_IS_64BIT,ZLIB_IS_LLP64 '>4GiB pack file with commit' '

	# check the packing and crc32 over a large >4Gb file
	git repack -adf &&
	# need to ensure we get a pack... 
	git commit -m msg file &&
	# test here make sure it is packed correctly
	git verify-pack -s .git/objects/pack/*.pack &&
	git fsck --verbose --strict --full &&
	false
'

test_done
# core.repositoryformatversion ?
# core.compression
# core.looseCompression and pack.compression
# core.packedGitWindowSize
# core.bigFileThreshold ***
