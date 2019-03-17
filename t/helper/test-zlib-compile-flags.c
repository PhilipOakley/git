#include "test-tool.h"
#include "git-compat-util.h"
#include <zlib.h>

int cmd__zlib_compile_flags(int argc, const char **argv)
{
	printf("%lu\n", zlibCompileFlags());
	return 0;
}
