#include "../../include/dirman.h"


/*
 *  Directory destriptor table, is an a static array of directory indexes. Main idea in
 *  saving dirs temporary in static table somewhere in memory. Max size of this
 *  table equals (11 + 255 * 8) * 10 = 20.5Kb.
 *
 *  For working with table we have directory struct, that have index of table in DDT. If
 *  we access to dirs with full DRM_DDT, we also unload old dirs and load new dirs.
 *  (Stack)
 *
 *  Main problem in parallel work. If we have threads, they can try to access this
 *  table at one time. If you use OMP parallel libs, or something like this, please,
 *  define NO_DDT flag (For avoiding deadlocks), or use locks to directories.
 *
 *  Why we need DDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
static directory_t* DRM_DDT[DDT_SIZE] = { NULL };


#pragma region [DDT]

    int DRM_DDT_add_directory(directory_t* directory) {
        #ifndef NO_DDT
            int current = -1;
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) {
                    current = i;
                    break;
                }
            }

            if (current == -1) {
                for (int i = 0; i < DDT_SIZE; i++) {
                    if (DRM_DDT[i]->lock == LOCKED) continue;
                    else {
                        current = i;
                        break;
                    }
                }
            }

            if (current == -1) return -1;
            if (DRM_lock_directory(DRM_DDT[current], omp_get_thread_num()) != -1) {
                if (DRM_DDT[current] != NULL) {
                    if (memcmp(directory->header->name, DRM_DDT[current]->header->name, DIRECTORY_NAME_SIZE) != 0) {
                        DRM_DDT_flush_index(current);
                    }
                }

                print_log("Adding to DDT directory [%s] at index [%i]", directory->header->name, current);
                DRM_DDT[current] = directory;
            }
            else {
                print_error("Can't lock directory [%s] for flushing!", DRM_DDT[current]->header->name);
                return -1;
            }
        #endif

        return 1;
    }

    directory_t* DRM_DDT_find_directory(char* name) {
        #ifndef NO_DDT
            if (name == NULL) return NULL;
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) continue;
                if (memcmp(DRM_DDT[i]->header->name, name, DIRECTORY_NAME_SIZE) == 0) {
                    DRM_lock_directory(DRM_DDT[i], omp_get_thread_num());
                    return DRM_DDT[i];
                }
            }
        #endif

        return NULL;
    }

    int DRM_DDT_sync() {
        #ifndef NO_DDT
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) continue;
                if (DRM_lock_directory(DRM_DDT[i], omp_get_thread_num()) == 1) {
                    DRM_save_directory(DRM_DDT[i], NULL);
                    DRM_release_directory(DRM_DDT[i], omp_get_thread_num());
                } 
                else return -1;
            }
        #endif

        return 1;
    }

    int DRM_DDT_free() {
        #ifndef NO_DDT
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_lock_directory(DRM_DDT[i], omp_get_thread_num()) == 1) {
                    DRM_DDT_flush_index(i);
                }
                else {
                    return -1;
                }
            }
        #endif

        return 1;
    }

    int DRM_DDT_flush_directory(directory_t* directory) {
        #ifndef NO_DDT
            if (directory == NULL) return -1;

            int index = -1;
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) continue;
                if (directory == DRM_DDT[i]) {
                    index = i;
                    break;
                }
            }

            if (index != -1) DRM_DDT_flush_index(index);
            else DRM_free_directory(directory);
        #else
            DRM_free_directory(directory);
        #endif

        return 1;
    }

    int DRM_DDT_flush_index(int index) {
        print_log("Flushed DDT directory in [%i] index", index);

        #ifndef NO_DDT
            if (DRM_DDT[index] == NULL) return -1;
            DRM_free_directory(DRM_DDT[index]);
            DRM_DDT[index] = NULL;
        #endif

        return 1;
    }

#pragma endregion