#include "../include/threading.h"


int THR_create_thread(void* entry, void* args) {
    #ifdef _WIN32
        HANDLE client_thread = CreateThread(NULL, 0, entry, args, 0, NULL);
        if (client_thread == NULL) {
            free(args);
            return -1;
        }

        CloseHandle(client_thread);
    #else
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, entry, args) != 0) {
            free(args);
            return -1;
        }

        pthread_detach(client_thread);
    #endif

    return 1;
}