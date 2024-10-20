#include "../../include/pageman.h"


/*
 *  Page destriptor table, is an a static array of pages indexes. Main idea in
 *  saving pages temporary in static table somewhere in memory. Max size of this
 *  table equals 4096 * 10 = 41Kb.
 *
 *  For working with table we have PAGE struct, that have index of table in PDT. If
 *  we access to pages with full PGM_PDT, we also unload old page and load new page.
 *  (Stack)
 *
 *  Main problem in parallel work. If we have threads, they can try to access this
 *  table at one time. If you use OMP parallel libs, or something like this, please,
 *  define NO_PDT flag (For avoiding deadlocks), or use locks to pages.
 *
 *  Why we need PDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
static page_t* PGM_PDT[PDT_SIZE] = { NULL };


#pragma region [PDT]

    int PGM_PDT_add_page(page_t* page) {
        #ifndef NO_PDT
            int current = -1;
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] != NULL) {
                    if (PGM_get_checksum(page) == PGM_get_checksum(PGM_PDT[i]) || PGM_PDT[i]->lock == UNLOCKED) {
                        current = i;
                        break;
                    }
                }
                else {
                    current = i;
                    break;
                }
            }

            if (current == -1) return -1;
            if (PGM_lock_page(PGM_PDT[current], omp_get_thread_num()) != -1) {
                if (PGM_PDT[current] != NULL) {
                    PGM_save_page(PGM_PDT[current], NULL);
                    PGM_PDT_flush_index(current);
                }
                
                print_log("Adding to PDT page [%s] at index [%i]", page->header->name, current);
                PGM_PDT[current] = page;
            } else {
                print_error("Can't lock page [%s] for flushing!", PGM_PDT[current]->header->name);
                return -1;
            }
        #endif

        return 1;
    }

    page_t* PGM_PDT_find_page(char* name) {
        #ifndef NO_PDT
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] == NULL) continue;
                if (memcmp(PGM_PDT[i]->header->name, name, PAGE_NAME_SIZE) == 0) {
                    PGM_lock_page(PGM_PDT[i], omp_get_thread_num());
                    return PGM_PDT[i];
                }
            }
        #endif

        return NULL;
    }

    int PGM_PDT_sync() {
        #ifndef NO_PDT
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] == NULL) continue;
                if (PGM_lock_page(PGM_PDT[i], omp_get_thread_num()) == 1) {
                    PGM_save_page(PGM_PDT[i], NULL);
                    PGM_release_page(PGM_PDT[i], omp_get_thread_num());
                } 
                else return -1;
            }
        #endif

        return 1;
    }

    int PGM_PDT_free() {
        #ifndef NO_PDT
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_lock_page(PGM_PDT[i], omp_get_thread_num()) != -1) PGM_PDT_flush_index(i);
                else return -1;
            }
        #endif

        return 1;
    }

    int PGM_PDT_flush_page(page_t* page) {
        #ifndef NO_PDT
            if (page == NULL) return -1;

            int index = -1;
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] == NULL) continue;
                if (page == PGM_PDT[i]) {
                    index = i;
                    break;
                }
            }

            if (index != -1) PGM_PDT_flush_index(index);
            else PGM_free_page(page);
        #else
            PGM_free_page(page);
        #endif

        return 1;
    }

    int PGM_PDT_flush_index(int index) {
        print_log("Flushed PDT page in [%i] index", index);

        #ifndef NO_PDT
            if (PGM_PDT[index] == NULL) return -1;
            PGM_free_page(PGM_PDT[index]);
            PGM_PDT[index] = NULL;
        #endif

        return 1;
    }

#pragma endregion