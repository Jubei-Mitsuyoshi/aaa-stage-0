/* ISC license. */

#include <errno.h>
#include "uint16.h"
#include "tai.h"
#include "iopause.h"
#include "s6lock.h"

int s6lock_wait_and (s6lock_t_ref a, uint16 const *idlist, unsigned int n, struct taia const *deadline, struct taia *stamp)
{
  iopause_fd x = { -1, IOPAUSE_READ, 0 } ;
  x.fd = s6lock_fd(a) ;
  for (; n ; n--, idlist++)
  {
    for (;;)
    {
      register int r = s6lock_check(a, *idlist) ;
      if (r < 0) return r ;
      else if (r) break ;
      r = iopause_stamp(&x, 1, deadline, stamp) ;
      if (r < 0) return r ;
      else if (!r) return (errno = ETIMEDOUT, -1) ;
      else if (s6lock_update(a) < 0) return -1 ;
    }
  }
  return 0 ;
}
