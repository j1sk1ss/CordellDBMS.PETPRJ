/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */


#ifndef THREADING_H_
#define THREADING_H_

#define LOCKED    1
#define UNLOCKED  0
#define NO_OWNER  0xFF

#ifndef _OPENMP
  #define omp_get_thread_num() 0
  #define omp_set_num_threads(num)
#else
  #include <omp.h>
#endif

#endif
