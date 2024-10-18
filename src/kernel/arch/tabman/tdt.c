#include "../../include/tabman.h"


/*
 *  Table destriptor table, is an a static array of table indexes. Main idea in
 *  saving tables temporary in static table somewhere in memory. Max size of this
 *  table equals 2072 * 10 = 20.72Kb.
 *
 *  For working with table we have table struct, that have index of table in TDT. If
 *  we access to tables with full TBM_TDT, we also unload old tables and load new tables.
 *  (Stack)
 *
 *  Main problem in parallel work. If we have threads, they can try to access this
 *  table at one time. If you use OMP parallel libs, or something like this, please,
 *  define NO_TDT flag (For avoiding deadlocks), or use locks to tables.
 *
 *  Why we need TDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
static table_t* TBM_TDT[TDT_SIZE] = { NULL };


#pragma region [TDT]

    int TBM_TDT_add_table(table_t* table) {
        #ifndef NO_TDT
            int current = -1;
            for (int i = 0; i < TDT_SIZE; i++) {
                if (TBM_TDT[i] != NULL) {
                    if (TBM_get_checksum(table) == TBM_get_checksum(TBM_TDT[i]) || TBM_TDT[i]->lock == UNLOCKED) {
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
            if (TBM_lock_table(TBM_TDT[current], omp_get_thread_num()) != -1) {
                if (TBM_TDT[current] != NULL) {
                    TBM_save_table(TBM_TDT[current], NULL);
                    TBM_TDT_flush_index(current);
                }

                print_log("Adding to TDT table [%s] at index [%i]", table->header->name, current);
                TBM_TDT[current] = table;
            }
        #endif

        return 1;
    }

    table_t* TBM_TDT_find_table(char* name) {
        #ifndef NO_TDT
            if (name == NULL) return NULL;
            for (int i = 0; i < TDT_SIZE; i++) {
                if (TBM_TDT[i] == NULL) continue;
                if (memcmp(TBM_TDT[i]->header->name, name, TABLE_NAME_SIZE) == 0) {
                    TBM_lock_table(TBM_TDT[i], omp_get_thread_num());
                    return TBM_TDT[i];
                }
            }
        #endif

        return NULL;
    }

    int TBM_TDT_sync() {
        #ifndef NO_TDT
            for (int i = 0; i < TDT_SIZE; i++) {
                if (TBM_TDT[i] == NULL) continue;
                if (TBM_lock_table(TBM_TDT[i], omp_get_thread_num()) == 1) {
                    TBM_save_table(TBM_TDT[i], NULL);
                    TBM_release_table(TBM_TDT[i], omp_get_thread_num());
                } 
                else return -1;
            }
        #endif

        return 1;
    }

    int TBM_TDT_free() {
        #ifndef NO_TDT
            for (int i = 0; i < PDT_SIZE; i++) {
                if (TBM_lock_table(TBM_TDT[i], omp_get_thread_num()) == 1) TBM_TDT_flush_index(i);
                else return -1;
            }
        #endif

        return 1;
    }

    int TBM_TDT_flush_table(table_t* table) {
        #ifndef NO_TDT
            if (table == NULL) return -1;

            int index = -1;
            for (int i = 0; i < TDT_SIZE; i++) {
                if (TBM_TDT[i] == NULL) continue;
                if (table == TBM_TDT[i]) {
                    index = i;
                    break;
                }
            }

            if (index != -1) TBM_TDT_flush_index(index);
            else TBM_free_table(table);
        #else
            TBM_free_table(directory);
        #endif

        return 1;
    }

    int TBM_TDT_flush_index(int index) {
        print_log("Flushed TDT table in [%i] index", index);

        #ifndef NO_TDT
            if (TBM_TDT[index] == NULL) return -1;
            TBM_free_table(TBM_TDT[index]);
            TBM_TDT[index] = NULL;
        #endif

        return 1;
    }

#pragma endregion