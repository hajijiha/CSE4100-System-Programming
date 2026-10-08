#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // memset, strcmp, strtok, sscanf, atoi 등 사용
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include "list.h"
#include "hash.h"
#include "bitmap.h"

/* 
   list.h의 기존 list_entry 매크로는 내부에서 MEMBER.next를 참조하여
   해시 테이블 등에서 올바른 결과를 주지 못하므로, 여기서 재정의합니다.
*/
#undef list_entry
#define list_entry(ptr, type, member) ((type *) ((uint8_t *)(ptr) - offsetof(type, member)))
#define hash_entry(H, STRUCT, MEMBER) list_entry((H), STRUCT, MEMBER)
#define BoolToStr(x) ((x) ? "true" : "false")

/* 자료구조 타입 식별을 위한 열거형 */
enum data_type {
    DATA_LIST = 1,
    DATA_HASH = 2,
    DATA_BITMAP = 3,
};

/* "list1", "hash1", "bm1" 등의 문자열에서 타입과 인덱스를 추출 */
struct object {
    enum data_type type;
    int idx;
};

struct object get_object_info(char *s) {
    struct object obj = {0};
    if (strstr(s, "list") != NULL) {
        obj.type = DATA_LIST;
        obj.idx = s[4] - '0';  // 예: "list1" → index 1
    } else if (strstr(s, "hash") != NULL) {
        obj.type = DATA_HASH;
        obj.idx = s[4] - '0';  // 예: "hash1" → index 1
    } else if (strstr(s, "bm") != NULL) {
        obj.type = DATA_BITMAP;
        obj.idx = s[2] - '0';  // 예: "bm1" → index 1
    }
    return obj;
}

/* 리스트의 0 기반 인덱스 위치의 요소를 반환 */
struct list_elem *list_get(struct list *l, int index) {
    struct list_elem *e = list_begin(l);
    for (int i = 0; i < index && e != list_end(l); i++)
        e = list_next(e);
    return e;
}

/* 해시 테이블에 저장할 데이터 구조체 */
struct hash_item {
    struct hash_elem hash_elem;
    int data;
};

typedef struct hash_item hash_item;
/* list_item은 list.h에 정의되어 있음 */

/* 리스트 비교 함수: list_item의 data 필드 비교 */
bool less_func_l(const struct list_elem *a, const struct list_elem *b, void *aux) {
    (void)aux;
    struct list_item *item_a = list_entry(a, struct list_item, elem);
    struct list_item *item_b = list_entry(b, struct list_item, elem);
    return item_a->data < item_b->data;
}

/* 해시 비교 함수: hash_item의 data 필드 비교 */
bool less_func_h(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    (void)aux;
    hash_item *item_a = hash_entry(a, hash_item, hash_elem);
    hash_item *item_b = hash_entry(b, hash_item, hash_elem);
    return item_a->data < item_b->data;
}

void my_hash_clear(struct hash *h) {
    while (!hash_empty(h)) {
        struct hash_iterator iter;
        hash_first(&iter, h);
        if (hash_next(&iter)) {
            struct hash_elem *he = hash_cur(&iter);
            hash_delete(h, he);
        }
    }
}

/* 해시 함수 래퍼: hash_item의 data 값을 해싱 (hash_int 사용) */
unsigned my_hash_int(const struct hash_elem *e, void *aux) {
    (void)aux;
    hash_item *item = hash_entry(e, hash_item, hash_elem);
    return hash_int(item->data);
}

/* 해시 액션 함수들 */
void square(struct hash_elem *e, void *aux) {
    (void)aux;
    hash_item *item = hash_entry(e, hash_item, hash_elem);
    item->data = item->data * item->data;
}

void triple(struct hash_elem *e, void *aux) {
    (void)aux;
    hash_item *item = hash_entry(e, hash_item, hash_elem);
    item->data = item->data * item->data * item->data;
}

/* 전역 배열: 최대 10개의 자료구조 (인덱스 0~9 사용) */
#define MAX_DS 10
struct list *LIST[MAX_DS] = {0};
struct hash *HASH[MAX_DS] = {0};
struct bitmap *BITMAP[MAX_DS] = {0};

int main(void) {
    char line[256];
    char *token;
    srand((unsigned)time(NULL));

    /* 각 명령어는 한 줄씩 입력됩니다. */
    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* 공백 및 개행문자 제거 */
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;
        token = strtok(line, " ");
        if (token == NULL) continue;

        if (strcmp(token, "quit") == 0) {
            break;
        } else if (strcmp(token, "create") == 0) {
            char *typeStr = strtok(NULL, " ");
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            if (strcmp(typeStr, "list") == 0) {
                struct list *l = malloc(sizeof(struct list));
                if (l == NULL) continue;
                list_init(l);
                LIST[obj.idx] = l;
            } else if (strcmp(typeStr, "hashtable") == 0) {
                struct hash *h = malloc(sizeof(struct hash));
                if (h == NULL) continue;
                hash_init(h, my_hash_int, less_func_h, NULL);
                HASH[obj.idx] = h;
            } else if (strcmp(typeStr, "bitmap") == 0) {
                int bit_cnt = atoi(strtok(NULL, " "));
                BITMAP[obj.idx] = bitmap_create((size_t)bit_cnt);
            }
        } else if (strcmp(token, "delete") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            if (obj.type == DATA_LIST) {
                free(LIST[obj.idx]);
                LIST[obj.idx] = NULL;
            } else if (obj.type == DATA_HASH) {
                hash_destroy(HASH[obj.idx], NULL);
                free(HASH[obj.idx]);
                HASH[obj.idx] = NULL;
            } else if (obj.type == DATA_BITMAP) {
                bitmap_destroy(BITMAP[obj.idx]);
                BITMAP[obj.idx] = NULL;
            }
        } else if (strcmp(token, "dumpdata") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            if (obj.type == DATA_LIST) {
                struct list *l = LIST[obj.idx];
                if(!list_empty(l)){
                struct list_elem *e;
                for (e = list_begin(l); e != list_end(l); e = list_next(e))
                    printf("%d ", list_entry(e, struct list_item, elem)->data);
                printf("\n");
                }
            } else if (obj.type == DATA_HASH) {
                struct hash *h = HASH[obj.idx];
                if (!hash_empty(h)) {
                    struct hash_iterator iter;
                    hash_first(&iter, h);
                    while (hash_next(&iter)) {
                        printf("%d ", hash_entry(hash_cur(&iter), hash_item, hash_elem)->data);
                    }
                    printf("\n");
                }
            } else if (obj.type == DATA_BITMAP) {
                struct bitmap *bm = BITMAP[obj.idx];
                for (int i = 0; i < (int)bitmap_size(bm); i++)
                    printf("%d", bitmap_test(bm, i) ? 1 : 0);
                printf("\n");
            }
        }
        /* 리스트 관련 명령어 */
        else if (strcmp(token, "list_push_front") == 0) {
            char *objStr = strtok(NULL, " ");
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            struct list_item *item = calloc(1, sizeof(struct list_item));
            item->data = data;
            list_push_front(l, &item->elem);
        } else if (strcmp(token, "list_push_back") == 0) {
            char *objStr = strtok(NULL, " ");
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            struct list_item *item = malloc(sizeof(struct list_item));
            item->data = data;
            list_push_back(l, &item->elem);
        } else if (strcmp(token, "list_pop_front") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            if (!list_empty(l))
                (void)list_pop_front(l);
        } else if (strcmp(token, "list_pop_back") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            if (!list_empty(l))
                (void)list_pop_back(l);
        } else if (strcmp(token, "list_insert") == 0) {
            char *objStr = strtok(NULL, " ");
            int index = atoi(strtok(NULL, " "));
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            struct list_item *item = malloc(sizeof(struct list_item));
            item->data = data;
            list_insert(list_get(l, index), &item->elem);
        } else if(strcmp(token, "list_empty") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            printf("%s\n", BoolToStr(list_empty(l)));
        }else if (strcmp(token, "list_insert_ordered") == 0) {
            char *objStr = strtok(NULL, " ");
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            struct list_item *item = malloc(sizeof(struct list_item));
            item->data = data;
            list_sort(l, less_func_l, NULL);
            list_insert_ordered(l, &item->elem, less_func_l, NULL);
        } else if (strcmp(token, "list_front") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            if (list_empty(l))
                printf("list empty\n");
            else
                printf("%d\n", list_entry(list_front(l), struct list_item, elem)->data);
        } else if (strcmp(token, "list_back") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            if (list_empty(l))
                printf("list empty\n");
            else
                printf("%d\n", list_entry(list_back(l), struct list_item, elem)->data);
        } else if (strcmp(token, "list_swap") == 0) {
            char *objStr = strtok(NULL, " ");
            int idx1 = atoi(strtok(NULL, " "));
            int idx2 = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            list_swap(list_get(l, idx1), list_get(l, idx2));
        } else if (strcmp(token, "list_shuffle") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            list_shuffle(LIST[obj.idx]);
        } else if (strcmp(token, "list_max") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            struct list_elem *maxElem = list_max(l, less_func_l, NULL);
            if (maxElem == list_end(l))
                printf("list empty\n");
            else
                printf("%d\n", list_entry(maxElem, struct list_item, elem)->data);
        } else if (strcmp(token, "list_min") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            struct list_elem *minElem = list_min(l, less_func_l, NULL);
            if (minElem == list_end(l))
                printf("list empty\n");
            else
                printf("%d\n", list_entry(minElem, struct list_item, elem)->data);
        } else if (strcmp(token, "list_maxminsizeempty") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            printf("%s\n", BoolToStr(list_empty(l)));
            if (!list_empty(l)) {
                int maxVal = list_entry(list_max(l, less_func_l, NULL), struct list_item, elem)->data;
                int minVal = list_entry(list_min(l, less_func_l, NULL), struct list_item, elem)->data;
                printf("%d %d %zu\n", maxVal, minVal, list_size(l));
            } else {
                printf("0 0 %zu\n", list_size(l));
            }
        }
         else if (strcmp(token, "list_remove") == 0) {
            char *objStr = strtok(NULL, " ");
            int index = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            if (!list_empty(l))
                list_remove(list_get(l, index));
        } else if (strcmp(token, "list_size") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            printf("%zu\n", list_size(l));
        }else if (strcmp(token, "list_reverse") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            list_reverse(l);
        } else if (strcmp(token, "list_sort") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct list *l = LIST[obj.idx];
            list_sort(l, less_func_l, NULL);
        } else if (strcmp(token, "list_splice") == 0) {
            char *destStr = strtok(NULL, " ");
            int destIndex = atoi(strtok(NULL, " "));
            char *srcStr = strtok(NULL, " ");
            int startIndex = atoi(strtok(NULL, " "));
            int endIndex = atoi(strtok(NULL, " "));
            struct object destObj = get_object_info(destStr);
            struct object srcObj = get_object_info(srcStr);
            list_splice(list_get(LIST[destObj.idx], destIndex),
                        list_get(LIST[srcObj.idx], startIndex),
                        list_get(LIST[srcObj.idx], endIndex));
        } else if (strcmp(token, "list_unique") == 0) {
            char *objStr = strtok(NULL, " ");
            char *dupStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            if (dupStr != NULL) {
                struct object dupObj = get_object_info(dupStr);
                list_unique(LIST[obj.idx], LIST[dupObj.idx], less_func_l, NULL);
            } else {
                list_unique(LIST[obj.idx], NULL, less_func_l, NULL);
            }
        }
        /* 해시 테이블 관련 명령어 */
        else if (strcmp(token, "hash_insert") == 0) {
            char *objStr = strtok(NULL, " ");
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            hash_item *item = malloc(sizeof(hash_item));
            item->data = data;
            hash_insert(h, &item->hash_elem);
        } else if (strcmp(token, "hash_find") == 0) {
            char *objStr = strtok(NULL, " ");
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            hash_item key;
            key.data = data;
            struct hash_elem *he = hash_find(h, &key.hash_elem);
            if (he)
                printf("%d\n", hash_entry(he, hash_item, hash_elem)->data);
        } else if (strcmp(token, "hash_delete") == 0) {
            char *objStr = strtok(NULL, " ");
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            hash_item key;
            key.data = data;
            (void)hash_delete(h, &key.hash_elem);
        } else if (strcmp(token, "hash_replace") == 0) {
            char *objStr = strtok(NULL, " ");
            int data = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            hash_item *item = malloc(sizeof(hash_item));
            item->data = data;
            (void) hash_replace(h, &item->hash_elem);
            /* 아무 출력도 하지 않음 */
        }
        else if (strcmp(token, "hash_apply") == 0) {
            char *objStr = strtok(NULL, " ");
            char *actStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            hash_action_func *act = NULL;
            if (strcmp(actStr, "square") == 0)
                act = square;
            else if (strcmp(actStr, "triple") == 0)
                act = triple;
            if (act)
                hash_apply(h, act);
        } else if (strcmp(token, "hash_plain") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            for (size_t i = 0; i < h->bucket_cnt; i++) {
                struct list *bucket = &h->buckets[i];
                struct list_elem *e;
                for (e = list_begin(bucket); e != list_end(bucket); e = list_next(e))
                    printf("%d ", hash_entry(e, hash_item, hash_elem)->data);
            }
            printf("\n");
        } else if (strcmp(token, "hash_empty") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            printf("%s\n", BoolToStr(hash_empty(h)));
        } else if (strcmp(token, "hash_size") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            printf("%zu\n", hash_size(h));
        } else if (strcmp(token, "hash_clear") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            for (size_t i = 0; i < h->bucket_cnt; i++) {
                struct list *bucket = &h->buckets[i];
                while (!list_empty(bucket)) {
                    struct list_elem *e = list_front(bucket);
                    hash_delete(h, &hash_entry(e, hash_item, hash_elem)->hash_elem);
                }
            }
        } else if (strcmp(token, "hash_etc") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            struct hash *h = HASH[obj.idx];
            for (size_t i = 0; i < h->bucket_cnt; i++) {
                struct list *bucket = &h->buckets[i];
                for (struct list_elem *e = list_begin(bucket); e != list_end(bucket); e = list_next(e)) {
                    printf("%d ", hash_entry(e, hash_item, hash_elem)->data);
                }
            }     
            for (size_t i = 0; i < h->bucket_cnt; i++) {
                struct list *bucket = &h->buckets[i];
                while (!list_empty(bucket)) {
                    struct list_elem *e = list_front(bucket);
                    hash_delete(h, &hash_entry(e, hash_item, hash_elem)->hash_elem);
                }
            }
        }
        /* 비트맵 관련 명령어 */
        else if (strcmp(token, "bitmap_set") == 0) {
            char *objStr = strtok(NULL, " ");
            int pos = atoi(strtok(NULL, " "));
            char *valStr = strtok(NULL, " ");
            bool val = (strcmp(valStr, "true") == 0);
            struct object obj = get_object_info(objStr);
            bitmap_set(BITMAP[obj.idx], pos, val);
        } else if (strcmp(token, "bitmap_test") == 0) {
            char *objStr = strtok(NULL, " ");
            int pos = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            printf("%s\n", BoolToStr(bitmap_test(BITMAP[obj.idx], pos)));
        } else if (strcmp(token, "bitmap_mark") == 0) {
            char *objStr = strtok(NULL, " ");
            int pos = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            bitmap_mark(BITMAP[obj.idx], pos);
        } else if (strcmp(token, "bitmap_reset") == 0) {
            char *objStr = strtok(NULL, " ");
            int pos = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            bitmap_reset(BITMAP[obj.idx], pos);
        } else if (strcmp(token, "bitmap_flip") == 0) {
            char *objStr = strtok(NULL, " ");
            int pos = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            bitmap_flip(BITMAP[obj.idx], pos);
        } else if (strcmp(token, "bitmap_all") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            printf("%s\n", BoolToStr(bitmap_all(BITMAP[obj.idx], start, cnt)));
        } else if (strcmp(token, "bitmap_set_all") == 0) {
            char *objStr = strtok(NULL, " ");
            char *valStr = strtok(NULL, " ");
            bool val = (strcmp(valStr, "true") == 0);
            struct object obj = get_object_info(objStr);
            bitmap_set_all(BITMAP[obj.idx], val);
        } else if (strcmp(token, "bitmap_set_multiple") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            char *valStr = strtok(NULL, " ");
            bool val = (strcmp(valStr, "true") == 0);
            struct object obj = get_object_info(objStr);
            bitmap_set_multiple(BITMAP[obj.idx], start, cnt, val);
        } else if (strcmp(token, "bitmap_any") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            printf("%s\n", BoolToStr(bitmap_any(BITMAP[obj.idx], start, cnt)));
        } else if (strcmp(token, "bitmap_contains") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            char *valStr = strtok(NULL, " ");
            bool val = (strcmp(valStr, "true") == 0);
            struct object obj = get_object_info(objStr);
            printf("%s\n", BoolToStr(bitmap_contains(BITMAP[obj.idx], start, cnt, val)));
        } else if (strcmp(token, "bitmap_count") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            char *valStr = strtok(NULL, " ");
            bool val = (strcmp(valStr, "true") == 0);
            struct object obj = get_object_info(objStr);
            printf("%zu\n", bitmap_count(BITMAP[obj.idx], start, cnt, val));
        } else if (strcmp(token, "bitmap_none") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            printf("%s\n", BoolToStr(bitmap_none(BITMAP[obj.idx], start, cnt)));
        } else if (strcmp(token, "bitmap_plain") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            for (int i = 0; i < (int)bitmap_size(BITMAP[obj.idx]); i++)
                printf("%d", bitmap_test(BITMAP[obj.idx], i) ? 1 : 0);
            printf("\n");
        } else if (strcmp(token, "bitmap_size") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            printf("%zu\n", bitmap_size(BITMAP[obj.idx]));
        } else if (strcmp(token, "bitmap_scan") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            char *valStr = strtok(NULL, " ");
            bool val = (strcmp(valStr, "true") == 0);
            struct object obj = get_object_info(objStr);
            size_t pos = bitmap_scan(BITMAP[obj.idx], start, cnt, val);
            printf("%zu\n", pos);
        } else if (strcmp(token, "bitmap_scan_and_flip") == 0) {
            char *objStr = strtok(NULL, " ");
            int start = atoi(strtok(NULL, " "));
            int cnt = atoi(strtok(NULL, " "));
            char *valStr = strtok(NULL, " ");
            bool val = (strcmp(valStr, "true") == 0);
            struct object obj = get_object_info(objStr);
            size_t pos = bitmap_scan_and_flip(BITMAP[obj.idx], start, cnt, val);
            printf("%zu\n", pos);
        } else if (strcmp(token, "bitmap_dump") == 0) {
            char *objStr = strtok(NULL, " ");
            struct object obj = get_object_info(objStr);
            bitmap_dump(BITMAP[obj.idx]);
        } else if (strcmp(token, "bitmap_expand") == 0) {
            char *objStr = strtok(NULL, " ");
            int extra = atoi(strtok(NULL, " "));
            struct object obj = get_object_info(objStr);
            if (bitmap_expand(BITMAP[obj.idx], extra) == NULL)
                printf("expand fail\n");
        }
    }
    return 0;
}
