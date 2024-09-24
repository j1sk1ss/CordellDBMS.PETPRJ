/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */


#ifndef LOGGING_H_
#define LOGGING_H_

#define LOGGING
#define ERRORS

#pragma region [Logging]

  #ifdef LOGGING
    #define print_log(message, ...)    printf("[LOG] (%s:%i) " message "\n", __FILE__, __LINE__, ##__VA_ARGS__)
  #else
    #define print_log(message, ...)
  #endif

  #ifdef WARNINGS
    #define print_warn(message, ...)   printf("[WARN] (%s:%i) " message "\n", __FILE__, __LINE__, ##__VA_ARGS__)
  #else
    #define print_warn(message, ...)
  #endif

  #ifdef ERRORS
    #define print_error(message, ...)  printf("[ERROR] (%s:%i) " message "\n", __FILE__, __LINE__, ##__VA_ARGS__)
  #else
    #define print_error(message, ...)
  #endif

  #ifdef INFORMING
    #define print_info(message, ...)   printf("[INFO] (%s:%i) " message "\n", __FILE__, __LINE__, ##__VA_ARGS__)
  #else
    #define print_info(message, ...)
  #endif

#pragma endregion

#endif