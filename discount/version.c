#include "config.h"

char markdown_version[] = "3.0.0c"//BRANCH VERSION
#if 4 != 4
		" TAB=4"
#endif
#if USE_AMALLOC
		" DEBUG"
#endif
#if CHECKBOX_AS_INPUT
		" GHC=INPUT"
#else
		" GHC=ENTITY"
#endif
		;
