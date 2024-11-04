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
 *  Why we need PDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
static __thread page_t* PGM_PDT[PDT_SIZE] = { NULL };


int PGM_PDT_add_page(page_t* page) {
    if (page == NULL) return -2;

    int current = -1;
    int free_current = -1;
    int occup_current = -1;

    for (int i = 0; i < PDT_SIZE; i++) {
        if (PGM_PDT[i] != NULL) {
            if (THR_test_lock(&PGM_PDT[i]->lock, omp_get_thread_num()) == UNLOCKED) {
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

    if (PGM_PDT[current] != NULL) {
        if (THR_require_lock(&PGM_PDT[current]->lock, omp_get_thread_num()) != -1) {
            PGM_save_page(PGM_PDT[current], NULL);
            PGM_PDT_flush_index(current);
        }
        else return -1;
    }

    print_debug("Adding to PDT page [%.*s] at index [%i]", PAGE_NAME_SIZE, page->header->name, current);
    PGM_PDT[current] = page;

    return 1;
}

page_t* PGM_PDT_find_page(char* name) {
    for (int i = 0; i < PDT_SIZE; i++) {
        if (PGM_PDT[i] == NULL) continue;
        if (strncmp(PGM_PDT[i]->header->name, name, PAGE_NAME_SIZE) == 0) return PGM_PDT[i];
    }

    return NULL;
}

int PGM_PDT_sync() {
    for (int i = 0; i < PDT_SIZE; i++) {
        if (PGM_PDT[i] == NULL) continue;
        if (THR_require_lock(&PGM_PDT[i]->lock, omp_get_thread_num()) == 1) {
            PGM_save_page(PGM_PDT[i], NULL);
            THR_release_lock(&PGM_PDT[i]->lock, omp_get_thread_num());
        }

        return -1;
    }

    return 1;
}

int PGM_PDT_free() {
    for (int i = 0; i < PDT_SIZE; i++) {
        if (PGM_PDT[i] == NULL) continue;
        if (THR_require_lock(&PGM_PDT[i]->lock, omp_get_thread_num()) != -1) PGM_PDT_flush_index(i);
        else {
            print_error("Can't lock page [%.*s]", PAGE_NAME_SIZE, PGM_PDT[i]->header->name);
            return -1;
        }
    }

    return 1;
}

int PGM_PDT_flush_page(page_t* page) {
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

    return 1;
}

int PGM_PDT_flush_index(int index) {
    print_debug("Flushed PDT page in [%i] index", index);
    if (PGM_PDT[index] == NULL) return -1;
    PGM_free_page(PGM_PDT[index]);
    PGM_PDT[index] = NULL;

    return 1;
}
