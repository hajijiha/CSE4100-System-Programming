#include "csapp.h"

#define NTHREADS 20
#define SBUFSIZE 100

// Item structure with readers-writers synchronization
typedef struct {
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    sem_t write_mutex;
} item;

// Binary search tree node
typedef struct node {
    item data;
    struct node* left;
    struct node* right;
} node;

// Bounded buffer for producer-consumer pattern
typedef struct {
    int *buf;          
    int n;             
    int front;         
    int rear;          
    sem_t mutex;       
    sem_t slots;       
    sem_t items;       
} sbuf_t;

// Global variables
node* root = NULL;
sbuf_t sbuf;
static int byte_cnt;
static sem_t cnt_mutex;
static sem_t save_mutex;

// Function prototypes
void *thread(void *vargp);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
void handle_client(int connfd);
void load_stock();
void save_stock();
void print_stock(node* n, char* obuf);
int buy_stock(int id, int num);
void sell_stock(int id, int num);
node* find_node(node* n, int id);
node* insert_node(node* root, int id, int stock, int price);
void free_tree(node* n);

// Acquire read lock
void read_lock(node* n) {
    if (!n) return;
    P(&n->data.mutex);
    n->data.readcnt++;
    if (n->data.readcnt == 1) {
        P(&n->data.write_mutex);
    }
    V(&n->data.mutex);
}

// Release read lock
void read_unlock(node* n) {
    if (!n) return;
    P(&n->data.mutex);
    n->data.readcnt--;
    if (n->data.readcnt == 0) {
        V(&n->data.write_mutex);
    }
    V(&n->data.mutex);
}

// Acquire write lock
void write_lock(node* n) {
    if (!n) return;
    P(&n->data.write_mutex);
}

// Release write lock
void write_unlock(node* n) {
    if (!n) return;
    V(&n->data.write_mutex);
}

// Signal handler for graceful shutdown
void sigint_handler(int sig) {
    printf("\nServer shutting down... Saving stock data.\n");
    
    // Print current state before saving
    char obuf[MAXLINE * 10];
    obuf[0] = '\0';
    print_stock(root, obuf);
    printf("Final stock state:\n%s", obuf);
    
    save_stock();
    free_tree(root);
    exit(0);
}

// Initialize bounded buffer
void sbuf_init(sbuf_t *sp, int n) {
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

// Clean up bounded buffer
void sbuf_deinit(sbuf_t *sp) {
    Free(sp->buf);
}

// Insert item into bounded buffer (Producer)
void sbuf_insert(sbuf_t *sp, int item) {
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

// Remove item from bounded buffer (Consumer)
int sbuf_remove(sbuf_t *sp) {
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

int main(int argc, char **argv) {
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // Install signal handler
    Signal(SIGINT, sigint_handler);
    
    // Initialize mutexes
    Sem_init(&cnt_mutex, 0, 1);
    Sem_init(&save_mutex, 0, 1);
    byte_cnt = 0;
    
    // Load stock data
    load_stock();

    listenfd = Open_listenfd(argv[1]);
    
    // Initialize bounded buffer
    sbuf_init(&sbuf, SBUFSIZE);
    
    // Create worker thread pool
    for (i = 0; i < NTHREADS; i++) {
        Pthread_create(&tid, NULL, thread, NULL);
    }
    
    printf("Stock server started on port %s\n", argv[1]);
    printf("Thread pool with %d worker threads created\n", NTHREADS);
    printf("Waiting for client connections...\n");

    // Master thread: Accept connections and insert to buffer
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                   client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        
        // Insert connection to buffer for worker threads
        sbuf_insert(&sbuf, connfd);
    }
    
    return 0;
}

// Worker thread routine
void *thread(void *vargp) {
    Pthread_detach(pthread_self());
    
    while (1) {
        // Get connection from buffer
        int connfd = sbuf_remove(&sbuf);
        
        // Handle client requests
        handle_client(connfd);
        
        // Close connection
        Close(connfd);
        
        // Save stock data after each client disconnects
        save_stock();
        
        // Log disconnection
        printf("Thread %ld: Client on fd %d disconnected, stock saved\n", pthread_self(), connfd);
    }
    
    return NULL;
}

// Handle client requests
void handle_client(int connfd) {
    rio_t rio;
    char buf[MAXLINE];
    char obuf[MAXLINE];
    int n;
    
    Rio_readinitb(&rio, connfd);
    
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("Server received %d bytes on fd %d\n", n, connfd);
        obuf[0] = '\0';
        
        if (strncmp(buf, "show", 4) == 0) {
            print_stock(root, obuf);
            Rio_writen(connfd, obuf, MAXLINE);
        }
        else if (strncmp(buf, "buy ", 4) == 0) {
            int id, num;
            sscanf(buf + 4, "%d %d", &id, &num);
            if (buy_stock(id, num)) {
                strcpy(obuf, "[buy] success\n");
                printf("Thread %ld: buy %d %d - success\n", pthread_self(), id, num);
            } else {
                strcpy(obuf, "Not enough left stock\n");
                printf("Thread %ld: buy %d %d - failed\n", pthread_self(), id, num);
            }
            Rio_writen(connfd, obuf, MAXLINE);
        }
        else if (strncmp(buf, "sell ", 5) == 0) {
            int id, num;
            sscanf(buf + 5, "%d %d", &id, &num);
            sell_stock(id, num);
            strcpy(obuf, "[sell] success\n");
            printf("Thread %ld: sell %d %d - success\n", pthread_self(), id, num);
            Rio_writen(connfd, obuf, MAXLINE);
        }
        else if (strncmp(buf, "exit", 4) == 0) {
            save_stock();
            break;
        }
    }
}

// Load stock data from file
void load_stock() {
    FILE* fp = fopen("stock.txt", "r");
    if (fp == NULL) {
        printf("stock.txt not found, creating new file\n");
        return;
    }
    
    int id, stock, price;
    while (fscanf(fp, "%d %d %d", &id, &stock, &price) == 3) {
        root = insert_node(root, id, stock, price);
    }
    
    fclose(fp);
    printf("Stock data loaded successfully\n");
}

// Save stock data to file
void save_stock() {
    P(&save_mutex);
    
    FILE* fp = fopen("stock.txt", "w");
    if (fp == NULL) {
        printf("Error opening stock.txt for writing\n");
        V(&save_mutex);
        return;
    }
    
    char obuf[MAXLINE * 10];
    obuf[0] = '\0';
    print_stock(root, obuf);
    fputs(obuf, fp);
    fclose(fp);
    
    V(&save_mutex);
}

// Insert node into BST
node* insert_node(node* root, int id, int stock, int price) {
    if (root == NULL) {
        node* new_node = (node*)malloc(sizeof(node));
        new_node->data.ID = id;
        new_node->data.left_stock = stock;
        new_node->data.price = price;
        new_node->data.readcnt = 0;
        Sem_init(&new_node->data.mutex, 0, 1);
        Sem_init(&new_node->data.write_mutex, 0, 1);
        new_node->left = NULL;
        new_node->right = NULL;
        return new_node;
    }
    
    if (id < root->data.ID) {
        root->left = insert_node(root->left, id, stock, price);
    } else if (id > root->data.ID) {
        root->right = insert_node(root->right, id, stock, price);
    } else {
        // Update existing node
        root->data.left_stock = stock;
        root->data.price = price;
    }
    
    return root;
}

// Find node in BST
node* find_node(node* n, int id) {
    if (n == NULL) return NULL;
    
    if (id == n->data.ID) {
        return n;
    } else if (id < n->data.ID) {
        return find_node(n->left, id);
    } else {
        return find_node(n->right, id);
    }
}

// Print stock using pre-order traversal
void print_stock(node* n, char* obuf) {
    if (n == NULL) return;
    
    // Current node first (pre-order)
    read_lock(n);
    char line[64];
    sprintf(line, "%d %d %d\n", n->data.ID, n->data.left_stock, n->data.price);
    strcat(obuf, line);
    read_unlock(n);
    
    // Then left and right subtrees
    if (n->left != NULL) {
        print_stock(n->left, obuf);
    }
    if (n->right != NULL) {
        print_stock(n->right, obuf);
    }
}

// Buy stock (decrease inventory)
int buy_stock(int id, int num) {
    node* n = find_node(root, id);
    if (n == NULL) return 0;
    
    write_lock(n);
    int success = 0;
    if (n->data.left_stock >= num) {
        n->data.left_stock -= num;
        success = 1;
    }
    write_unlock(n);
    
    return success;
}

// Sell stock (increase inventory)
void sell_stock(int id, int num) {
    node* n = find_node(root, id);
    if (n == NULL) return;
    
    write_lock(n);
    n->data.left_stock += num;
    write_unlock(n);
}

// Free the BST
void free_tree(node* n) {
    if (n == NULL) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}