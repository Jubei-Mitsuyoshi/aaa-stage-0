#if !defined(MY) || !defined(ROOT)
#error "You must define ROOT and MY when compiling this program."
#endif

#define PROGRAM "myinit"
#define VERSION_MAJOR "0"
#define VERSION_MINOR "4"
#define DEBUG 1

#define DEFAULT_HOME ROOT "etc/" MY "init"
#define INPUT_FIFO_EXTENSION ".in"

#define INIT_BOOT DEFAULT_HOME ".boot"  /* init.boot is only for system init */
