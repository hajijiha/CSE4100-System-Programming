#include "csapp.h"

// Item structure with readers-writers synchronization
typedef struct{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    sem_t write_mutex;
}item;

// Connection pool for managing multiple clients
typedef struct{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
}pool;

// Binary search tree node
typedef struct node{
    item item;
    struct node* left;
    struct node* right;
}node;

// Function declarations
void init_pool(int, pool*);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
void load_stock();
void save_stock();
void print_stock(node* NODE, char* obuf);
void sell(int a, int b);
int buy(int a, int b);

// Readers-Writers synchronization functions
void read_lock(node* n);
void read_unlock(node* n);
void write_lock(node* n);
void write_unlock(node* n);

node* root = NULL;

// Signal handler for graceful shutdown
void sigint_handler(int sig){
    printf("\nServer shutting down... Saving stock data.\n");
    save_stock();
    exit(0);
}

// Acquire read lock
void read_lock(node *n) {
    if (!n) return;
    P(&n->item.mutex);
    n->item.readcnt++;
    if (n->item.readcnt == 1) {
        P(&n->item.write_mutex);
    }
    V(&n->item.mutex);
}

// Release read lock
void read_unlock(node *n) {
    if (!n) return;
    P(&n->item.mutex);
    n->item.readcnt--;
    if (n->item.readcnt == 0) {
        V(&n->item.write_mutex);
    }
    V(&n->item.mutex);
}

// Acquire write lock
void write_lock(node *n) {
    if (!n) return;
    P(&n->item.write_mutex);
}

// Release write lock
void write_unlock(node *n) {
    if (!n) return;
    V(&n->item.write_mutex);
}

int main(int argc, char **argv) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    static pool pool;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    Signal(SIGINT, sigint_handler);

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    load_stock();
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    printf("Stock server started on port %s\n", argv[1]);
    printf("Waiting for client connections...\n");

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        // Check for new connections
        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage); 
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);

            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                        client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        }
        check_clients(&pool);
    }
    exit(0);
}

// Initialize connection pool
void init_pool(int listenfd, pool *p){
    int i;
    p->maxi = -1;
    for(i=0; i<FD_SETSIZE; i++){
        p->clientfd[i] = -1;
    }

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

// Add new client to pool
void add_client(int connfd, pool *p) {
    int i;  
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)
        if (p->clientfd[i] < 0) { 
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            FD_SET(connfd, &p->read_set);

            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    if (i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}

// Handle client requests
void check_clients(pool *p) {
    int i, connfd, n;
    char buf[MAXLINE];
    char obuf[MAXLINE];
    rio_t *rp;

    for (i = 0; i <= p->maxi && p->nready > 0; i++) {
        connfd = p->clientfd[i];
        rp = &p->clientrio[i];

        if (connfd > 0 && FD_ISSET(connfd, &p->ready_set)) {
            p->nready--;
            if ((n = Rio_readlineb(rp, buf, MAXLINE)) > 0) {
                printf("Server received %d bytes on fd %d\n", n, connfd);
                obuf[0] = '\0';

                if (strncmp(buf, "show", 4) == 0) {
                    print_stock(root, obuf);
                    Rio_writen(connfd, obuf, MAXLINE);
                }
                else if (strncmp(buf, "buy ", 4) == 0) {
                    int id, num;
                    sscanf(buf + 4, "%d %d", &id, &num);
                    if (buy(id, num))
                        strcpy(obuf, "[buy] success\n");
                    else
                        strcpy(obuf, "Not enough left stock\n");
                    Rio_writen(connfd, obuf, MAXLINE);
                }
                else if (strncmp(buf, "sell ", 5) == 0) {
                    int id, num;
                    sscanf(buf + 5, "%d %d", &id, &num);
                    sell(id, num);
                    strcpy(obuf, "[sell] success\n");
                    Rio_writen(connfd, obuf, MAXLINE);
                }
                else if (strncmp(buf, "exit", 4) == 0) {
                    Close(connfd);
                    FD_CLR(connfd, &p->read_set);
                    p->clientfd[i] = -1;
                    save_stock();
                }
            } else {
                // Client disconnected
                printf("Client fd %d disconnected\n", connfd);
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
                save_stock();
            }
        }
    }
}

// Load stock data from file
void load_stock() {
    FILE* pFile = fopen("stock.txt", "r");
    if (pFile == NULL) {
        printf("Creating new stock.txt file\n");
        return;
    }
    
    int a, b, c;
    node* now;
    node* nn;
    node* pre;

    while (fscanf(pFile, "%d %d %d", &a, &b, &c) == 3) {
        now = malloc(sizeof(node));
        if (now == NULL) {
            perror("Failed to allocate memory");
            exit(EXIT_FAILURE);
        }

        now->item.ID = a;
        now->item.left_stock = b;
        now->item.price = c;
        now->item.readcnt = 0;
        Sem_init(&now->item.mutex, 0, 1);
        Sem_init(&now->item.write_mutex, 0, 1);
        now->left = now->right = NULL;

        // Insert into BST
        if (root == NULL) {
            root = now;
        } else {
            nn = root;
            pre = NULL;
            while (nn != NULL) {
                pre = nn;
                if (nn->item.ID < a) {
                    nn = nn->right;
                } else {
                    nn = nn->left;
                }
            }
            if (a < pre->item.ID) {
                pre->left = now;
            } else {
                pre->right = now;
            }
        }
    }
    fclose(pFile);
    printf("Stock data loaded successfully\n");
}

// Save stock data to file
void save_stock() {
    FILE* fp = fopen("stock.txt", "w");
    if (!fp) {
        perror("Failed to open stock.txt for writing");
        return;
    }
    char obuf[MAXLINE * 10] = "";
    print_stock(root, obuf);
    fputs(obuf, fp);
    fclose(fp);
    printf("Stock data saved successfully\n");
}

// Sell stock (increase inventory)
void sell(int id, int n){
    node* now = root;
    while(now != NULL && now->item.ID != id){
        if(now->item.ID < id){
            now = now->right;
        }
        else{
            now = now->left;
        }
    }
    if(now != NULL && now->item.ID == id){
        write_lock(now);
        now->item.left_stock += n;
        write_unlock(now);
    }
}  

// Buy stock (decrease inventory)
int buy(int id, int n){
    node* now = root;
    while(now != NULL && now->item.ID != id){
        if(now->item.ID < id){
            now = now->right;
        }
        else{
            now = now->left;
        }
    }
    if(now != NULL){
        write_lock(now);
        if(now->item.left_stock >= n){
            now->item.left_stock -= n;
            write_unlock(now);
            return 1;
        }
        write_unlock(now);
        return 0;
    }
    return 0;
}

// Print stock data using pre-order traversal
void print_stock(node* N, char* obuf) {
    if (!N) return;
    
    // Pre-order traversal (current node first)
    read_lock(N);
    char line[64];
    sprintf(line, "%d %d %d\n",
            N->item.ID,
            N->item.left_stock,
            N->item.price);
    strcat(obuf, line);
    read_unlock(N);
    
    print_stock(N->left, obuf);
    print_stock(N->right, obuf);
}