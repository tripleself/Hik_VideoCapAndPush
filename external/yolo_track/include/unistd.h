/**Many C programs developed under Linux need the header file unistd.h, but VC does not have this header file,
 * so it always reports an error when compiled with VC. Save the following content as unistd.h, and you can solve this problem.
 * so it always reports an error when compiled with VC. Save the following content as unistd.h, and you can solve this problem.
 This file is part of the Mingw32 package.
 * unistd.h maps (roughly) to io.h
 */

#ifndef _UNISTD_H
#define _UNISTD_H
#include <io.h>
#include <process.h>
#endif /* _UNISTD_H */
