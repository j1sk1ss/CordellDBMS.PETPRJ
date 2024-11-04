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
 *  Why we need TDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
static __thread table_t* TBM_TDT[TDT_SIZE] = { NULL };


int TBM_TDT_add_table(table_t* table) {
    if (table == NULL) return -2;

    int current = -1;
    int free_current = -1;
    int occup_current = -1;

    for (int i = 0; i < TDT_SIZE; i++) {
        if (TBM_TDT[i] != NULL) {
            if (TBM_TDT[i]->lock == UNLOCKED) {
                occup_current = i;
            }
        }
        else {
            free_current = i;
            break;
        }
    }

    if (free_current == -1 && occup_current == -1) return -1;
    else if (free_current != -1) current = free_current;
    else if (occup_current != -1) current = occup_current;

    if (TBM_TDT[current] != NULL) {
        if (THR_require_lock(&TBM_TDT[current]->lock, omp_get_thread_num()) != -1) {
            TBM_save_table(TBM_TDT[current], NULL);
            TBM_TDT_flush_index(current);
        }
        else return -1;
    }

    print_debug("Adding to TDT table [%.*s] at index [%i]", TABLE_NAME_SIZE, table->header->name, current);
    TBM_TDT[current] = table;

    return 1;
}

table_t* TBM_TDT_find_table(char* name) {
    if (name == NULL) return NULL;
    for (int i = 0; i < TDT_SIZE; i++) {
        if (TBM_TDT[i] == NULL) continue;
        if (strncmp(TBM_TDT[i]->header->name, name, TABLE_NAME_SIZE) == 0) return TBM_TDT[i];
    }

    return NULL;
}

int TBM_TDT_sync() {
    for (int i = 0; i < TDT_SIZE; i++) {
        if (TBM_TDT[i] == NULL) continue;
        if (THR_require_lock(&TBM_TDT[i]->lock, omp_get_thread_num()) == 1) {
            TBM_save_table(TBM_TDT[i], NULL);
            THR_release_lock(&TBM_TDT[i]->lock, omp_get_thread_num());
        }
        else return -1;
    }

    return 1;
}

int TBM_TDT_free() {
    for (int i = 0; i < TDT_SIZE; i++) {
        if (TBM_TDT[i] == NULL) continue;
        if (THR_require_lock(&TBM_TDT[i]->lock, omp_get_thread_num()) != -1) TBM_TDT_flush_index(i);
        else {
            print_error("Can't lock table [%.*s]", TABLE_NAME_SIZE, TBM_TDT[i]->header->name);
            return -1;
        }
    }

    return 1;
}

int TBM_TDT_flush_table(table_t* table) {
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

    return 1;
}

int TBM_TDT_flush_index(int index) {
    print_debug("Flushed TDT table in [%i] index", index);
    if (TBM_TDT[index] == NULL) return -1;
    TBM_free_table(TBM_TDT[index]);
    TBM_TDT[index] = NULL;

    return 1;
}
