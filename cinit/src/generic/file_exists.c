/*******************************************************************************
 *
 * 2006-2009 Nico Schottelius (nico-cinit at schottelius.org)
 *
 * This file is part of cinit.

 * cinit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cinit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cinit.  If not, see <http://www.gnu.org/licenses/>.

 *
 *    check whether file exists and is a file
 *
 */

#include <errno.h>              /* errno */
#include <sys/stat.h>           /* stat */
#include <stdio.h>              /* NULL */

#include "intern.h"             /* print_errno() */
#include "svc-intern.h"         /* svc_report_status() */
#include "messages.h"           /* messages..  */

int file_exists(char *filename)
{
   struct stat buf;

   /*
    * check: - is it a link? if so, is it broken? report! - is it non existent? 
    * -> return non-existent - is it existent, but not a regular file? report! 
    */
   if(lstat(filename, &buf) == -1) {    /* lstat fails? */
      if(errno == ENOENT) {
         return FE_NOT;
      } else {
         print_errno(filename);
         return FE_ERR;
      }
   } else {
      if(S_ISLNK(buf.st_mode)) {             /* is a link, check destination */
         if(stat(filename, &buf) == -1) {    /* do real stat(): */
            if(errno == ENOENT) {
               svc_report_status(filename, MSG_BROKENLINK, NULL);
               return FE_NOLINK;
            } else {
               /*
                * FIXME: MSG_*, ?? 
                */
               mini_printf("anderer fehler.\n", 1);
               print_errno(filename);
               return FE_ERR;
            }
         }
      }
   }                            /* caught all stat() errors */

   if(!S_ISREG(buf.st_mode)) {
      svc_report_status(filename, MSG_NONREGULAR, NULL);
      return FE_OTHER;
   }

   return FE_FILE;
}
