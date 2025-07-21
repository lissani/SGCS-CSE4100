/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#include <semaphore.h>
#define MAX_STOCKS 1000
#define NTHREADS 4 // 서버 시작 시 미리 생성되는 thread 수
#define SBUFSIZE 100 // 공유 버퍼의 최대 크기, 최대로 연결할 수 있는 client 수
/*
트리 자료구조
*/
typedef struct item{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex; // readcnt 보호
    sem_t w; // writer 보호
} Item;

typedef struct TreeNode {
    Item data;
    struct TreeNode* left;
    struct TreeNode* right;
} TreeNode;

Item create_item(int, int, int);
TreeNode* insert(TreeNode*, TreeNode*);
void free_tree(TreeNode*);
TreeNode* search(TreeNode*, int);
TreeNode* load_stock_file(const char*);
void save_stock(char*);

/*
shared buffer 자료구조
*/
typedef struct{
    int* buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

void sbuf_init(sbuf_t* sp, int n);
void sbuf_deinit(sbuf_t* sp);
void sbuf_insert(sbuf_t* sp, int item);
int sbuf_remove(sbuf_t* sp);

void sigint_handler(int signo);
int handle_stock_command(int , char* , char*);

/*
global variables
*/
int stock_count = 0;
sbuf_t sbuf;
TreeNode* stock_root;
TreeNode* stock_array[MAX_STOCKS];

static int active_clients = 0;
static sem_t client_count_mutex;
/*
thread functions
*/
void *thread(void *vargp);
void echo_cnt(int connfd);
static void init_echo_cnt(void);
void increment_client_count(void);
void decrement_client_count(void);

static int byte_cnt;
static sem_t mutex_byte_cnt;

void echo(int connfd);

int main(int argc, char **argv) 
{
    Signal(SIGINT, sigint_handler);
    stock_root = load_stock_file("stock.txt");

    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    pthread_t tid;
    
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    } // 포트 전달하지 않으면 에러 메세지 출력 & 종료

    // client 접속 대기
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    Sem_init(&client_count_mutex, 0, 1);

    // create worker threads
    for (i=0; i<NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        // 클라이언트가 연결 요청 보내면, 수락
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        increment_client_count();
        sbuf_insert(&sbuf, connfd);
    }
    return 0;
}

/////////////////////////////////////////
// item 생성 & 초기화
// 서버 시작 시, txt 파일 -> 트리
// 클라이언트의 insert 요청 시
Item create_item(int ID, int left_stock, int price){
    Item newItem;
    newItem.ID = ID;
    newItem.left_stock = left_stock;
    newItem.price = price;
    newItem.readcnt = 0;
    Sem_init(&newItem.mutex, 0, 1);
    Sem_init(&newItem.w, 0, 1);
    return newItem;
}

// tree 삽입
TreeNode* insert(TreeNode* root, TreeNode* node){
    if (root == NULL){
        return node;
    }

    if (node->data.ID < root->data.ID)
        root->left = insert(root->left, node);
    else if (node->data.ID > root->data.ID)
        root->right = insert(root->right, node);
    return root;
}

// 메모리 해제 & 세마포어 파괴
void free_tree(TreeNode* root){
    if (root == NULL) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

// 이진 트리 검색
TreeNode* search(TreeNode* root, int ID){
    if (root == NULL) return NULL;

    if (ID == root->data.ID)
        return root;
    else if (ID < root->data.ID)
        return search(root->left, ID);
    else
        return search(root->right, ID);
}

// txt 파일 읽고, BST 구성
TreeNode* load_stock_file(const char* filename){
    FILE* fp = fopen(filename, "r");
    if (fp == NULL){
        perror("Failed to open file");
        exit(1);
    }

    TreeNode* root = NULL;
    int id, stock, price;

    while (fscanf(fp, "%d %d %d", &id, &stock, &price) != EOF){
        Item item = create_item(id, stock, price);

        TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
        node->data = item;
        node->left = node->right = NULL;

        stock_array[stock_count++] = node;
        root = insert(root, node);
    }
    fclose(fp);
    return root;
}

void sigint_handler(int signo)
{
    save_stock("stock.txt");
    free_tree(stock_root);
    exit(1);
}

void save_stock(char* file){
    FILE *fp = fopen(file, "w");
    if (fp != NULL){
        for (int i=0; i<stock_count; i++){
            TreeNode* node = stock_array[i];
            fprintf(fp, "%d %d %d\n", node->data.ID, node->data.left_stock, node->data.price);
        }
        fclose(fp);
    }
}

void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots); // 빈 슬롯 감소
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items); // 데이터 증가
}

int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items); // 데이터 감소
    P(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)];
    V(&sp->mutex);
    V(&sp->slots); // 빈 슬롯 증가
    return item;
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        echo_cnt(connfd);
        Close(connfd);
        decrement_client_count();
    }
}

static void init_echo_cnt(void){
    Sem_init(&mutex_byte_cnt, 0, 1);
    byte_cnt = 0;
}

// thread-safe client handler
void echo_cnt(int connfd){
    int n;
    char buf[MAXLINE];
    char response[MAXLINE] = "";

    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    Pthread_once(&once, init_echo_cnt);
    Rio_readinitb(&rio, connfd);

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        // client의 요청 한 줄씩 읽음
        P(&mutex_byte_cnt);
        byte_cnt += n;
        printf("server recieved %d bytes\n", n);
        fflush(stdout);
        V(&mutex_byte_cnt);

        //Rio_writen(connfd, buf, MAXLINE); // echo 동작
        if (handle_stock_command(connfd, buf, response) == 1){
            break;
        }
    }
}

int handle_stock_command(int connfd, char* buf, char* response){
    char command[MAXLINE];
    int id, count;

    response[0] = '\0'; // 응답 버퍼 초기화
    buf[strcspn(buf, "\n")] = 0;

    if (sscanf(buf, "%s", command) != 1)
        return 0;

    // show - 전체 주식 목록 출력
    if (strcmp(command, "show") == 0){

        for (int i = 0; i < stock_count; i++){
            TreeNode* node = stock_array[i];

            // reader entry
            P(&node->data.mutex);
            node->data.readcnt++;
            if (node->data.readcnt == 1){
                P(&node->data.w);
            }
            V(&node->data.mutex);

            // read - critical
            char line[100];
            sprintf(line, "%d %d %d\n", node->data.ID, node->data.left_stock, node->data.price);
            strcat(response, line);

            // reader exit
            P(&node->data.mutex);
            node->data.readcnt--;
            if (node->data.readcnt == 0){
                V(&node->data.w);
            }
            V(&node->data.mutex);
        }
        Rio_writen(connfd, response, MAXLINE);
    }
    
    else if (strncmp(command, "buy", 3) == 0){
        if (sscanf(buf, "buy %d %d", &id, &count) == 2){
            TreeNode* node = search(stock_root, id);
            if (node){
                P(&node->data.w);
                // write - critical
                if (node->data.left_stock >= count){
                    node->data.left_stock -= count;
                    strcpy(response, "[buy] success\n");
                } else{
                    strcpy(response, "Not enough left stocks\n");
                }
                V(&node->data.w);
            } else{
                strcpy(response, "Invalid stock ID\n");
            }
        }
        Rio_writen(connfd, response, MAXLINE);
    }
    
    else if (strncmp(command, "sell", 4) == 0){
        if (sscanf(buf, "sell %d %d", &id, &count) == 2){
            TreeNode* node = search(stock_root, id);
            if (node){
                P(&node->data.w);
                // wirte - critical
                node->data.left_stock += count;
                strcpy(response, "[sell] success\n");
                V(&node->data.w);
            } else{
                strcpy(response, "Invalid stock ID\n");
            }
        }
        Rio_writen(connfd, response, MAXLINE);
    }
    else if (strcmp(command, "exit") == 0) {
        return 1;
    }
    return 0;
}

void increment_client_count(void){
    P(&client_count_mutex);
    active_clients++;
    V(&client_count_mutex);
}

void decrement_client_count(void){
    P(&client_count_mutex);
    active_clients--;
    V(&client_count_mutex);

    if(active_clients == 0){
        save_stock("stock.txt");
    }
}
/* $end echoserverimain */
