//인터랙티브 명령어 처리

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bitmap.h"
#include "hash.h"
#include "list.h"

#define MAX_NUM 10
#define MAX_ARGS 10
#define MAX_BITMAP_SIZE 64

struct list* ListArr[MAX_NUM];
struct hash* HashArr[MAX_NUM];
struct bitmap* BitmapArr[MAX_NUM];

int ListNum=0, HashNum=0, BitmapNum=0;

typedef enum {
    LIST_PUSH_FRONT=1, LIST_PUSH_BACK, LIST_POP_FRONT,
    LIST_POP_BACK, LIST_REMOVE, LIST_REVERSE, LIST_SHUFFLE,
    LIST_SORT, LIST_SPLICE, LIST_SWAP, LIST_UNIQUE,
    LIST_FRONT, LIST_BACK, LIST_INSERT_ORDERED, LIST_INSERT,
    LIST_EMPTY, LIST_SIZE, LIST_MAX, LIST_MIN
} L_command;

typedef enum {
    HASH_INSERT=1, HASH_APPLY, HASH_DELETE, HASH_EMPTY,
    HASH_SIZE, HASH_CLEAR, HASH_FIND, HASH_REPLACE
} H_command;

typedef enum {
    BITMAP_MARK=1, BITMAP_ALL, BITMAP_ANY, BITMAP_SET,
    BITMAP_SET_ALL, BITMAP_SET_MULTIPLE, BITMAP_CONTAINS,
    BITMAP_COUNT, BITMAP_DUMP, BITMAP_EXPAND, BITMAP_FLIP,
    BITMAP_NONE, BITMAP_RESET, BITMAP_SCAN, BITMAP_SCAN_AND_FLIP,
    BITMAP_SIZE, BITMAP_TEST
} B_command;

/* 문자열을 열거형으로 변환 */
L_command get_list_cmd(const char* cmd){
    if (strcmp(cmd, "list_push_front")==0) return LIST_PUSH_FRONT;
    if (strcmp(cmd, "list_push_back")==0) return LIST_PUSH_BACK;
    if (strcmp(cmd, "list_pop_front")==0) return LIST_POP_FRONT;
    if (strcmp(cmd, "list_pop_back")==0) return LIST_POP_BACK;
    if (strcmp(cmd, "list_remove")==0) return LIST_REMOVE;
    if (strcmp(cmd, "list_reverse")==0) return LIST_REVERSE;
    if (strcmp(cmd, "list_shuffle")==0) return LIST_SHUFFLE;
    if (strcmp(cmd, "list_sort")==0) return LIST_SORT;
    if (strcmp(cmd, "list_splice")==0) return LIST_SPLICE;
    if (strcmp(cmd, "list_swap")==0) return LIST_SWAP;
    if (strcmp(cmd, "list_unique")==0) return LIST_UNIQUE;
    if (strcmp(cmd, "list_front")==0) return LIST_FRONT;
    if (strcmp(cmd, "list_back")==0) return LIST_BACK;
    if (strcmp(cmd, "list_insert_ordered")==0) return LIST_INSERT_ORDERED;
    if (strcmp(cmd, "list_insert")==0) return LIST_INSERT;
    if (strcmp(cmd, "list_empty")==0) return LIST_EMPTY;
    if (strcmp(cmd, "list_size")==0) return LIST_SIZE;
    if (strcmp(cmd, "list_max")==0) return LIST_MAX;
    if (strcmp(cmd, "list_min")==0) return LIST_MIN;
    return 0;
}

H_command get_hash_cmd(const char* cmd) {
    if (strcmp(cmd, "hash_insert") == 0) return HASH_INSERT;
    if (strcmp(cmd, "hash_apply") == 0) return HASH_APPLY;
    if (strcmp(cmd, "hash_delete") == 0) return HASH_DELETE;
    if (strcmp(cmd, "hash_empty") == 0) return HASH_EMPTY;
    if (strcmp(cmd, "hash_size") == 0) return HASH_SIZE;
    if (strcmp(cmd, "hash_clear") == 0) return HASH_CLEAR;
    if (strcmp(cmd, "hash_find") == 0) return HASH_FIND;
    if (strcmp(cmd, "hash_replace") == 0) return HASH_REPLACE;
    return 0;
}

B_command get_bitmap_cmd(const char* cmd) {
    if (strcmp(cmd, "bitmap_mark") == 0) return BITMAP_MARK;
    if (strcmp(cmd, "bitmap_all") == 0) return BITMAP_ALL;
    if (strcmp(cmd, "bitmap_any") == 0) return BITMAP_ANY;
    if (strcmp(cmd, "bitmap_set") == 0) return BITMAP_SET;
    if (strcmp(cmd, "bitmap_set_all") == 0) return BITMAP_SET_ALL;
    if (strcmp(cmd, "bitmap_set_multiple") == 0) return BITMAP_SET_MULTIPLE;
    if (strcmp(cmd, "bitmap_contains") == 0) return BITMAP_CONTAINS;
    if (strcmp(cmd, "bitmap_count") == 0) return BITMAP_COUNT;
    if (strcmp(cmd, "bitmap_dump") == 0) return BITMAP_DUMP;
    if (strcmp(cmd, "bitmap_expand") == 0) return BITMAP_EXPAND;
    if (strcmp(cmd, "bitmap_flip") == 0) return BITMAP_FLIP;
    if (strcmp(cmd, "bitmap_none") == 0) return BITMAP_NONE;
    if (strcmp(cmd, "bitmap_reset") == 0) return BITMAP_RESET;
    if (strcmp(cmd, "bitmap_scan") == 0) return BITMAP_SCAN;
    if (strcmp(cmd, "bitmap_scan_and_flip") == 0) return BITMAP_SCAN_AND_FLIP;
    if (strcmp(cmd, "bitmap_size") == 0) return BITMAP_SIZE;
    if (strcmp(cmd, "bitmap_test") == 0) return BITMAP_TEST;
    return 0;
}

/* 자료구조별 명령어 처리 - list
    LIST_PUSH_FRONT, LIST_PUSH_BACK, LIST_POP_FRONT,
    LIST_POP_BACK, LIST_REMOVE, LIST_REVERSE, LIST_SHUFFLE,
    LIST_SORT, LIST_SPLICE, LIST_SWAP, LIST_UNIQUE,
    LIST_FRONT, LIST_BACK, LIST_INSERT_ORDERED, LIST_INSERT,
    LIST_EMPTY, LIST_SIZE, LIST_MAX, LIST_MIN
*/
void process_list_cmd(L_command cmd, char** args){
    struct list_item *i;
    struct list_elem *e;
    struct list *l;

    int idx = -1;
    //리스트 찾기
    for (int i=0; i<ListNum; i++){
        if (ListArr[i]!=NULL && strcmp(ListArr[i]->name, args[1])==0){
            idx = i;
            break;
        }
    }
    if (idx != -1) {
        l = ListArr[idx];
    }

    switch (cmd)
    {
    case LIST_PUSH_FRONT:
        {
            size_t val = atoi(args[2]);
            i = malloc(sizeof(struct list_item));
            if(i == NULL) return;
            i->data = val;
            list_push_front (l, &i->elem);
        }
        break;

    case LIST_PUSH_BACK:
        {
            size_t val = atoi(args[2]);
            i = malloc(sizeof(struct list_item));
            if(i == NULL) return;
            i->data = val;
            list_push_back (l, &i->elem);
        }
        break;
        
    case LIST_POP_FRONT:
        {
            e = list_pop_front (l);
        }
        break;

    case LIST_POP_BACK:
        {
            e = list_pop_back (l);
        }
        break;

    case LIST_REMOVE:
        {
            e = list_begin(l);
            for(int i=0; i<atoi(args[2]); i++) e = list_next(e);
            e = list_remove (e);
        }
        break;

    case LIST_REVERSE:
        {
            list_reverse (l);
        }
        break;

    case LIST_SHUFFLE:
        {
            list_shuffle(l);
        }
        break;

    case LIST_SORT:
        {
            list_sort(l, my_list_compare, NULL);
        }
        break;
    
    case LIST_SPLICE:
        {
            //set destination - e before
            e = list_begin(l);
            for(int i=0; i<atoi(args[2]); i++) e = list_next(e);
            //moving elem
            struct list *l_moving;
            idx = -1;
            //리스트 찾기
            for (int i=0; i<ListNum; i++){
                if (ListArr[i]!=NULL && strcmp(ListArr[i]->name, args[3])==0){
                    idx = i;
                    break;
                }
            }
            if (idx != -1) {
                l_moving = ListArr[idx];
            }

            struct list_elem *moving_first = list_begin(l_moving);
            for(int i=0; i<atoi(args[4]); i++) moving_first = list_next(moving_first);

            struct list_elem *moving_last = list_begin(l_moving);
            for(int i=0; i<atoi(args[5]); i++) moving_last = list_next(moving_last);
        
            list_splice (e, moving_first, moving_last);            
        }
        break;

    case LIST_SWAP:
        {
            //set first elem
            e = list_begin(l);
            for(int i=0; i<atoi(args[2]); i++) e = list_next(e);
            //set second elem
            struct list_elem* e2 = list_begin(l);
            for(int i=0; i<atoi(args[3]); i++) e2 = list_next(e2);

            list_swap(e, e2);         
        }
        break;

    case LIST_UNIQUE:
        {   
            if(args[2] == NULL || strcmp(args[2], "")==0)
            {
                list_unique (l, NULL, my_list_compare, NULL);
            }
            else{
                struct list* dupli_list = NULL;
                int D_idx = -1;
                //리스트 찾기
                for (int i=0; i<ListNum; i++){
                    if (ListArr[i]!=NULL && strcmp(ListArr[i]->name, args[2])==0){
                        D_idx = i;
                        break;
                    }
                }
                if (D_idx != -1) {
                    dupli_list = ListArr[D_idx];
                    list_unique (l, dupli_list, my_list_compare, NULL);
                }
            }
        }
        break;

    case LIST_FRONT:
        {
            e = list_front(l);
            i = list_entry(e, struct list_item, elem);
            printf("%d\n", i->data);            
        }
        break;

    case LIST_BACK:
        {
            e = list_back(l);
            i = list_entry(e, struct list_item, elem);
            printf("%d\n", i->data);            
        }
        break;

    case LIST_INSERT_ORDERED:
        {
            size_t val = atoi(args[2]);
            i = malloc(sizeof(struct list_item));
            if(i == NULL) return;
            i->data = val;
            list_insert_ordered (l, &i->elem, my_list_compare, NULL);
        }
        break;

    case LIST_INSERT:
        {
            //set position
            e = list_begin(l);
            for(int i=0; i<atoi(args[2]); i++) e = list_next(e);

            size_t val = atoi(args[3]);
            struct list_item * new_i = malloc(sizeof(struct list_item));
            if(new_i == NULL) return;
            new_i->data = val;
            list_insert (e, &new_i->elem);
        }
        break;

    case LIST_EMPTY:
        {
            if(list_empty(l)) printf("true\n");
            else printf("false\n");            
        }
        break;

    case LIST_SIZE:
        {
            printf("%zu\n", list_size(l));
        }
        break;

    case LIST_MAX:
        {
            e = list_max (l, my_list_compare, NULL);
            i = list_entry(e, struct list_item, elem);
            printf("%d\n", i->data);
        }
        break;

    case LIST_MIN:
        {
            e = list_min (l, my_list_compare, NULL);
            i = list_entry(e, struct list_item, elem);
            printf("%d\n", i->data);
        }
        break;

    default:
        break;
    }
}

/* 자료구조별 명령어 처리 - hash
    HASH_INSERT, HASH_APPLY, HASH_DELETE, HASH_EMPTY,
    HASH_SIZE, HASH_CLEAR, HASH_FIND, HASH_REPLACE
*/
void process_hash_cmd(H_command cmd, char** args){
    struct hash_item *i;
    struct hash_elem *e = NULL;
    struct hash *h;
    struct hash_iterator iter;

    int idx = -1;
    //해시 찾기
    for (int i=0; i<HashNum; i++){
        if (HashArr[i]!=NULL && strcmp(HashArr[i]->name, args[1])==0){
            idx = i;
            break;
        }
    }
    if (idx != -1) {
        h = HashArr[idx];
    } else{
        return;
    }

    switch (cmd)
    {
    case HASH_INSERT:
        {
            size_t val = atoi(args[2]);
            i = malloc(sizeof(struct hash_item));
            if(i == NULL) {
                break;
            }
            i->data = val;
            struct hash_elem * old = hash_insert (h, &i->elem);
        }
        break; 

    case HASH_APPLY:
        {
            hash_first (&iter, h);

            if(strcmp(args[2],"triple")==0) {
                while (hash_next (&iter)){   
                    triple(hash_cur(&iter),NULL);
                }
            }
            else if(strcmp(args[2],"square")==0){
                while ((e=hash_next (&iter))!=NULL){   
                    square(hash_cur(&iter),NULL);
                }
            }
            else printf("지원하지 않는 apply 명령어\n");
        }
        break; 

    case HASH_DELETE:
        {
            int key = atoi(args[2]);
            struct hash_item temp_item;
            temp_item.data = key;

            struct hash_elem *found = hash_find(h, &temp_item.elem);
            if(found == NULL){
                return;
            }
            e = hash_delete(h,found);
            delete_hash_elem(e, NULL);
        }
        break;   

    case HASH_EMPTY:
        {
            if(hash_empty(h)) printf("true\n");
            else printf("false\n");  
        }
        break; 

    case HASH_SIZE:
        {
            printf("%zu\n", hash_size(h));
        }
        break; 

    case HASH_CLEAR:
        {
            hash_clear (h, delete_hash_elem); 
        }
        break;

    case HASH_FIND:
        {
            hash_first (&iter, h);
            while ((e = hash_next(&iter)) != NULL) {
                i = hash_entry(e, struct hash_item, elem);
                if(i->data == atoi(args[2])) {
                    printf("%d\n", atoi(args[2]));
                    break;
                }
            }
        }
        break;

    case HASH_REPLACE:
        {   
            int key = atoi(args[2]);
            struct hash_item temp_item;
            temp_item.data = key;

            struct hash_elem *found = hash_find(h, &temp_item.elem);

            struct hash_item *new_hash_item = malloc(sizeof(struct hash_item));
            new_hash_item->data = key;

            if(found != NULL){
                struct hash_elem *old_item = hash_entry(found, struct hash_item, elem);
                e = hash_delete(h,found);
                delete_hash_elem(e,NULL);
            }
            e = hash_insert(h, &new_hash_item->elem);
        }
        break;

    default:
        break;
    }
}

void process_bitmap_cmd(B_command cmd, char** args){
    struct bitmap *b;
    size_t bit_idx;

    int idx = -1;
    //비트맵 찾기
    for (int i=0; i<BitmapNum; i++){
        if (BitmapArr[i]!=NULL && strcmp(BitmapArr[i]->name, args[1])==0){
            idx = i;
            break;
        }
    }
    if (idx != -1){
        b = BitmapArr[idx];
    }
    size_t bit_cnt = bitmap_size(b);

    switch (cmd)
    {
    case BITMAP_MARK:
        {  
            bit_idx = strtoul(args[2], NULL, 10);
            bitmap_mark (b, bit_idx);
        }
        break;

    case BITMAP_ALL:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t bit_end_idx = strtoul(args[3], NULL, 10);
            if(bitmap_all (b, bit_idx, bit_end_idx)) printf("true\n");
            else printf("false\n");
        }
        break;

    case BITMAP_ANY:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t bit_end_idx = strtoul(args[3], NULL, 10);
            if(bitmap_any (b, bit_idx, bit_end_idx)) printf("true\n");
            else printf("false\n");
        }
        break;

    case BITMAP_SET:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            bitmap_set (b, bit_idx, string_to_bool(args[3])); 
        }
        break;

    case BITMAP_SET_ALL:
        {
            bitmap_set_all (b, string_to_bool(args[2]));
        }
        break;

    case BITMAP_SET_MULTIPLE:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t bit_end_idx = strtoul(args[3], NULL, 10);
            bitmap_set_multiple (b, bit_idx, bit_end_idx, string_to_bool(args[4]));
        }
        break;

    case BITMAP_CONTAINS:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t cnt = strtoul(args[3], NULL, 10);
            if(bitmap_contains (b, bit_idx, cnt, string_to_bool(args[4]))){
                printf("true\n");
            }
            else printf("false\n");
        }
        break;

    case BITMAP_COUNT:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t cnt = strtoul(args[3], NULL, 10);
            size_t num = bitmap_count(b, bit_idx, cnt, string_to_bool(args[4]));
            printf("%zu\n", num);
        }
        break;

    case BITMAP_DUMP:
        {
            bitmap_dump(b);
        }
        break;

    case BITMAP_EXPAND:
        {
            struct bitmap* expanded_bitmap = bitmap_expand(b, atoi(args[2]));
            if(expanded_bitmap != NULL){
                bitmap_destroy(b);
                BitmapArr[idx] = expanded_bitmap;
                b = expanded_bitmap;
            }
        }
        break; // 비트맵 배열에서 바꿔줘야 함

    case BITMAP_FLIP:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            bitmap_flip (b, bit_idx);
        }
        break;

    case BITMAP_NONE:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t cnt = strtoul(args[3], NULL, 10);
            if(bitmap_none (b, bit_idx, cnt)) printf("true\n");
            else printf("false\n");
        }
        break;

    case BITMAP_RESET:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            bitmap_reset (b, bit_idx);
        }
        break;

    case BITMAP_SCAN:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t cnt = strtoul(args[3], NULL, 10);
            size_t find_idx=bitmap_scan (b, bit_idx, cnt, string_to_bool(args[4]));
            printf("%zu\n", find_idx);
        }
        break;

    case BITMAP_SCAN_AND_FLIP:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            size_t cnt = strtoul(args[3], NULL, 10);
            size_t find_idx=bitmap_scan_and_flip (b, bit_idx, cnt, string_to_bool(args[4]));
            printf("%zu\n", find_idx);
        }
        break;

    case BITMAP_SIZE:
        {
            size_t num = bitmap_size (b);
            printf("%zu\n",num);
        }
        break;

    case BITMAP_TEST:
        {
            bit_idx = strtoul(args[2], NULL, 10);
            if(bitmap_test (b, bit_idx)) printf("true\n");
            else printf("false\n");
        }
        break;

    default:
        break;
    }
}

void process_command(char *command){
    char *args[MAX_ARGS];
    memset(args, 0, sizeof(args));
    int count =0;

    char *token = strtok(command, " ");
    while (token != NULL && count < MAX_ARGS){
        args[count++]=token;
        token = strtok(NULL, " ");
    }
    /* 기본 명령어 create, delete, dumpdata, quit */
    if(strcmp(args[0], "create")==0){

        if(strcmp(args[1], "list")==0){
            struct list * new_list = malloc(sizeof(struct list));
            if (new_list != NULL){
                list_init(new_list);
                ListArr[ListNum++] = new_list;
                new_list->name = strdup(args[2]);
            }
        }
        else if(strcmp(args[1], "hashtable")==0){
            struct hash * new_hash = malloc(sizeof(struct hash));
            if (new_hash != NULL){
                hash_init(new_hash, my_hash_func, my_hash_compare, NULL);
                HashArr[HashNum++] = new_hash;
                new_hash->name = strdup(args[2]);
            }
        }

        else if(strcmp(args[1], "bitmap")==0){
            if (args[2] == NULL || args[3] == NULL) {
                return;
            }
            int size = atoi(args[3]);
            // 크기 검사
            if (size <= 0) {
                return;
            }
            // 배열 범위 검사
            if (BitmapNum >= MAX_BITMAP_SIZE) {
                return;
            }    
            struct bitmap * new_bitmap = bitmap_create(size);
            if (new_bitmap != NULL){
                BitmapArr[BitmapNum++] = new_bitmap;
                new_bitmap->name = strdup(args[2]);
            }
        }
    }

    else if(strcmp(args[0], "delete")==0){
        int idx = -1;
        //리스트 찾기
        for (int i=0; i<ListNum; i++){
            if (ListArr[i]!=NULL && strcmp(ListArr[i]->name, args[1])==0){
                idx = i;
                break;
            }
        }
        if (idx != -1){
            //리스트 삭제
            struct list *delete_list = ListArr[idx];
            while (!list_empty(delete_list)){
                struct list_elem *e = list_pop_front(delete_list);
                delete_list_elem(e);
            }
            free(delete_list->name);
            free(delete_list);
            ListArr[idx]=NULL;
            return;
        }
        idx = -1;
        //해시 찾기
        for (int i=0; i<HashNum; i++){
            if (HashArr[i]!=NULL && strcmp(HashArr[i]->name, args[1])==0){
                idx = i;
                break;
            }
        }
        if (idx != -1){
            //해시 삭제
            struct hash *delete_hash = HashArr[idx];
            hash_clear(delete_hash, delete_hash_elem);
            free(delete_hash->name);
            free(delete_hash);
            HashArr[idx]=NULL;
            return;
        }
        idx = -1;
        //비트맵 찾기
        for (int i=0; i<BitmapNum; i++){
            if (BitmapArr[i]!=NULL && strcmp(BitmapArr[i]->name, args[1])==0){
                idx = i;
                break;
            }
        }
        if (idx != -1){
            //비트맵 삭제
            struct bitmap *delete_bitmap = BitmapArr[idx];
            bitmap_destroy(delete_bitmap);
            BitmapArr[idx]=NULL;
            return;
        }
    }
    else if(strcmp(args[0], "dumpdata")==0){
        int idx = -1;
        //리스트 찾기
        for (int i=0; i<ListNum; i++){
            if (ListArr[i]!=NULL && strcmp(ListArr[i]->name, args[1])==0){
                idx = i;
                break;
            }
        }
        if (idx != -1){
            //리스트 dump
            struct list *dump_list = ListArr[idx];
            struct list_elem *e;

            for(e = list_begin(dump_list); e!= list_end(dump_list); e=list_next(e)){
                struct list_item * i = list_entry(e, struct list_item, elem);
                printf("%d ", i->data);
            }
            if(!list_empty(dump_list)) printf("\n");
            return;
        }
        idx=-1;
        //해시 찾기
        for (int i=0; i<HashNum; i++){
            if (HashArr[i]!=NULL && strcmp(HashArr[i]->name, args[1])==0){
                idx = i;
                break;
            }
        }
        if (idx != -1){
            //해시 dump
            struct hash *dump_hash = HashArr[idx];
            struct hash_elem *e;
            struct hash_iterator iter;

            hash_first(&iter, dump_hash);

            while((e=hash_next(&iter))!=NULL){
                struct hash_item * i = hash_entry(e, struct hash_item, elem);
                printf("%d ", i->data);
            }
            if(!hash_empty(dump_hash)) printf("\n");
            return;
        }
        idx = -1;
        //비트맵 찾기
        for (int i=0; i<BitmapNum; i++){
            if (BitmapArr[i]!=NULL && strcmp(BitmapArr[i]->name, args[1])==0){
                idx = i;
                break;
            }
        }
        if (idx != -1){
            //비트맵 dump
            struct bitmap *dump_bitmap = BitmapArr[idx];
            
            for(size_t i=0; i< dump_bitmap->bit_cnt; i++){
                bool bit_val = bitmap_test(dump_bitmap, i);
                printf("%d", bit_val);
            }
            printf("\n");
            return;
        }
    }

    else if(strcmp(args[0], "quit")==0){
        exit(0);
    }

    /* 추가 자료구조 명령어 처리 */
    else{
        if(get_list_cmd(args[0])){
            L_command cmd = get_list_cmd(args[0]);
            process_list_cmd(cmd, args);
        }
        else if(get_hash_cmd(args[0])){
            H_command cmd = get_hash_cmd(args[0]);
            process_hash_cmd(cmd, args);
        }
        else if(get_bitmap_cmd(args[0])){
            B_command cmd = get_bitmap_cmd(args[0]);
            process_bitmap_cmd(cmd, args);
        }
    }
}

int main(){
    char command[256];

    while (1){
        if (scanf("%[^\n]%*c", command)==EOF) break;
        process_command(command);
    }
    return 0;
}