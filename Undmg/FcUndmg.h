#pragma once

/* Disable C4200 warnings because of old C-style flex sized structures. */
#pragma warning( push )
#pragma warning( disable : 4200 )

#include "dmg.h"
#include "hfslib.h"
#include "hfscompress.h"

/* Enable warnings again. */
#pragma warning( pop )
