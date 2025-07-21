/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#include <semaphore.h>
#define MAX_STOCKS 1000
/*
트리 자료구조
*/
typedef struct item{
    int ID;
    int left_stock;
    int price;
    int readcnt; // task2 사용
    sem_t mutex; // task2 사용
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
client 다루기 위한 자료구조
*/
typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
}pool;

void init_pool(int, pool*);
void add_client(int, pool*);
void check_clients(pool*);
int handle_client_command(int, char*, TreeNode*, pool*);
void sigint_handler(int signo);
int zero_client(pool*);

int byte_cnt = 0;
int stock_count = 0;
TreeNode* stock_root;
TreeNode* stock_array[MAX_STOCKS];

void echo(int connfd);

int main(int argc, char **argv) 
{
    Signal(SIGINT, sigint_handler);
    stock_root = load_stock_file("stock.txt");

    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    static pool pool;
    
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    } // 포트 전달하지 않으면 에러 메세지 출력 & 종료

    // client 접속 대기
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {

        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage);
            // 클라이언트가 연결 요청 보내면, 수락
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                        client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        }

        check_clients(&pool);

        if (zero_client(&pool)){
            save_stock("stock.txt");
        }
    }
    exit(0);
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

/////////////////////////////////////////
void init_pool(int listenfd, pool *p){
    int i;
    p->maxi = -1;
    for (i=0; i<FD_SETSIZE; i++)
        p->clientfd[i] = -1;
    
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p){
    int i;
    p->nready--;
    for (i=0; i<FD_SETSIZE; i++){
        if (p->clientfd[i] < 0){
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            FD_SET(connfd, &p->read_set);

            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if(i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}

void check_clients(pool *p){
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for(i=0; (i <= p->maxi) && (p->nready > 0); i++){
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if (connfd <= 0) continue;

        if (FD_ISSET(connfd, &p->ready_set)){
            p->nready--;
            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                byte_cnt += n;
                printf("server recieved %d bytes\n", n);
                fflush(stdout);
                // echo 동작
                //Rio_writen(connfd, buf, n);
                if (handle_client_command(connfd, buf, stock_root, p) == 1){
                    break;
                }
                p->clientrio[i] = rio;
            }
            else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}

int handle_client_command(int connfd, char* buf, TreeNode* root, pool* p){
    char command[MAXLINE];
    int id, count;
    char response[MAXLINE] = "";

    response[0] = '\0'; // 응답 버퍼 초기화
    buf[strcspn(buf, "\n")] = 0;
    
    if (sscanf(buf, "%s", command) != 1)
        return 0;

    // show - 전체 주식 목록 출력
    if (strcmp(command, "show") == 0){

        for (int i = 0; i < stock_count; i++){
            TreeNode* node = stock_array[i];
            char line[100];
            sprintf(line, "%d %d %d\n", node->data.ID, node->data.left_stock, node->data.price);
            strcat(response, line);
        }
        Rio_writen(connfd, response, MAXLINE);
    }
    
    else if (strncmp(command, "buy", 3) == 0){
        if (sscanf(buf, "buy %d %d", &id, &count) == 2){
            TreeNode* node = search(root, id);
            if (node){
                if (node->data.left_stock >= count){
                    node->data.left_stock -= count;
                    strcpy(response, "[buy] success\n");
                } else{
                    strcpy(response, "Not enough left stocks\n");
                }
            } else{
                strcpy(response, "Invalid stock ID\n");
            }
        }
        Rio_writen(connfd, response, MAXLINE);
    }
    
    else if (strncmp(command, "sell", 4) == 0){
        if (sscanf(buf, "sell %d %d", &id, &count) == 2){
            TreeNode* node = search(root, id);
            if (node){
                node->data.left_stock += count;
                strcpy(response, "[sell] success\n");
            } else{
                strcpy(response, "Invalid stock ID\n");
            }
        }
        Rio_writen(connfd, response, MAXLINE);
    }
    else if (strcmp(command, "exit") == 0) {
        Close(connfd);
        FD_CLR(connfd, &p->read_set);
        for (int i=0; i <= p->maxi; i++){
            if (p->clientfd[i] == connfd){
                p->clientfd[i] = -1;
                break;
            }
        }
        return 1; // 연결 종료
    }
    return 0;
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

int zero_client(pool* p){
    for(int i=0; i <= p->maxi; i++){
        if(p->clientfd[i] != -1){
            return 0;
        }
    }
    return 1;
}
/* $end echoserverimain */
