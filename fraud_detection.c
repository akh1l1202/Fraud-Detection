#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// T (minimum degree) controls the size of the nodes.
// Max keys per node: 2*T - 1
// Max children per node: 2*T
#define T 3
#define MAX_TRANSACTIONS (2 * T - 1)
#define MAX_CHILDREN (2 * T)

#define HASH_MAP_SIZE 100
#define MAX_CUSTOMER_NAME 50

// --- NEW GLOBAL FRAUD CONSTANTS ---
#define SECONDS_IN_HOUR 3600L
#define TXN_LIMIT_PER_HOUR 25
#define TXN_WARNING_THRESHOLD 15
// ----------------------------------

// --- Data Structures ---

typedef struct {
    // Primary key for B-Tree sorting (time-based)
    long long time_key;
    int id; // Unique ID for record-keeping (checked for uniqueness)
    float amount;
    time_t date_time;
    char type; // 'D' = Debit, 'C' = Credit
    int counterparty_id;
    char channel[10]; // ATM, WEB, APP
    int terminal_id;
} Transaction;

typedef struct BTreeNode {
    Transaction transactions[MAX_TRANSACTIONS];
    struct BTreeNode *children[MAX_CHILDREN];
    int n; // Current number of transactions
    bool is_leaf;
} BTreeNode;

typedef struct Customer {
    int id;
    char name[MAX_CUSTOMER_NAME];
    BTreeNode *b_tree_root;
    float debit_threshold;
    float credit_threshold;
    struct Customer *next;  // For Hash Map Chaining
} Customer;

typedef struct HashMap {
    Customer *table[HASH_MAP_SIZE];
} HashMap;


// --- Memory Management Functions ---

void freeBTree(BTreeNode *x) {
    if (x == NULL) return;
    for (int i = 0; i < x->n + 1; i++) {
        freeBTree(x->children[i]);
    }
    free(x);
}

void freeHashMap(HashMap *map) {
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        Customer *current = map->table[i];
        Customer *temp;
        while (current != NULL) {
            temp = current;
            current = current->next;
            freeBTree(temp->b_tree_root);
            free(temp);
        }
        map->table[i] = NULL;
    }
    printf("\n[INFO] All system memory (Customers and Transactions) freed successfully.\n");
}


// --- A. B-Tree Operations ---

BTreeNode* createBTreeNode(bool leaf) {
    BTreeNode *newNode = (BTreeNode*)malloc(sizeof(BTreeNode));
    if (!newNode) {
        perror("Memory allocation failed for BTreeNode");
        exit(EXIT_FAILURE);
    }
    newNode->is_leaf = leaf;
    newNode->n = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        newNode->children[i] = NULL;
    }
    return newNode;
}

Transaction* findTransactionByID(BTreeNode *x, int transactionId) {
    if (x == NULL) return NULL;

    for (int i = 0; i < x->n; i++) {
        if (x->transactions[i].id == transactionId) {
            return &x->transactions[i];
        }
        // Safe even if children[i] is NULL because the callee checks for NULL
        Transaction *found_in_child = findTransactionByID(x->children[i], transactionId);
        if (found_in_child != NULL) {
            return found_in_child;
        }
    }
    return findTransactionByID(x->children[x->n], transactionId);
}

// Function to split a full child node y of a non-full parent node x
void BTreeSplitChild(BTreeNode *x, int i, BTreeNode *y) {
    // y is full. Create z to hold y's [T..2T-2] keys
    BTreeNode *z = createBTreeNode(y->is_leaf);
    z->n = T - 1;

    for (int j = 0; j < T - 1; j++) {
        z->transactions[j] = y->transactions[j + T];
    }
    if (!y->is_leaf) {
        for (int j = 0; j < T; j++) {
            z->children[j] = y->children[j + T];
            y->children[j + T] = NULL;
        }
    }

    y->n = T - 1;

    for (int j = x->n; j >= i + 1; j--) {
        x->children[j + 1] = x->children[j];
    }
    x->children[i + 1] = z;

    for (int j = x->n - 1; j >= i; j--) {
        x->transactions[j + 1] = x->transactions[j];
    }

    x->transactions[i] = y->transactions[T - 1];

    x->n = x->n + 1;
}

// Insert into a non-full node x
void BTreeInsertNonFull(BTreeNode *x, Transaction t) {
    int i = x->n - 1;
    long long key = t.time_key;

    if (x->is_leaf) {
        while (i >= 0 && x->transactions[i].time_key > key) {
            x->transactions[i + 1] = x->transactions[i];
            i--;
        }
        x->transactions[i + 1] = t;
        x->n++;
    } else {
        while (i >= 0 && x->transactions[i].time_key > key) {
            i--;
        }
        i++;

        // Guard in case of unexpected NULL (should not happen in a valid B-Tree, but safe)
        if (x->children[i] == NULL) {
            x->children[i] = createBTreeNode(true);
        }

        if (x->children[i]->n == MAX_TRANSACTIONS) {
            BTreeSplitChild(x, i, x->children[i]);
            if (x->transactions[i].time_key < key) {
                i++;
            }
        }
        BTreeInsertNonFull(x->children[i], t);
    }
}

// Public-facing insert function
void insertTransaction(BTreeNode **root, Transaction t) {
    if (*root == NULL) {
        *root = createBTreeNode(true);
    }

    BTreeNode *r = *root;

    if (r->n == MAX_TRANSACTIONS) {
        BTreeNode *s = createBTreeNode(false);
        s->children[0] = r;

        BTreeSplitChild(s, 0, r);

        BTreeInsertNonFull(s, t);
        *root = s;
        printf("[INFO] B-Tree root split executed. Height increased.\n");
    } else {
        BTreeInsertNonFull(r, t);
    }
}

// In-order traversal to print all transactions
void printBTreeTransactions(BTreeNode *x) {
    if (x == NULL) return;

    int i;
    for (i = 0; i < x->n; i++) {
        printBTreeTransactions(x->children[i]);

        char time_buffer[30];
        struct tm *lt = localtime(&x->transactions[i].date_time);
        if (lt) {
            strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", lt);
        } else {
            snprintf(time_buffer, sizeof(time_buffer), "N/A");
        }

        // Removed Time Key printout for cleaner history view
        printf(" - ID: %d, Type: %c, Amount: Rs.%.2f, Date: %s | Counterparty: %d, Channel: %s, Terminal: %d\n",
               x->transactions[i].id,
               x->transactions[i].type,
               x->transactions[i].amount,
               time_buffer,
               x->transactions[i].counterparty_id,
               x->transactions[i].channel,
               x->transactions[i].terminal_id);
    }
    printBTreeTransactions(x->children[i]);
}

// --- B. Hash Map Operations ---

int hashFunction(int customerId) {
    if (customerId < 0) customerId = -customerId;
    return customerId % HASH_MAP_SIZE;
}

void insertCustomer(HashMap *map, Customer *newCustomer) {
    int index = hashFunction(newCustomer->id);
    newCustomer->next = map->table[index];
    map->table[index] = newCustomer;
}

Customer* findCustomer(HashMap *map, int customerId) {
    int index = hashFunction(customerId);
    Customer *current = map->table[index];
    while (current != NULL) {
        if (current->id == customerId) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// --- C. Core Fraud Detection Logic ---

// NEW: Function to check transaction velocity (transactions per hour)
int checkVelocitySpike(BTreeNode *x, time_t cutoff_time) {
    if (x == NULL) return 0;

    int count = 0;

    for (int i = 0; i < x->n; i++) {
        // Since B-tree stores keys in ascending order (left-to-right),
        // and we use the full traversal (printBTreeTransactions style),
        // we must check the left child first.
        count += checkVelocitySpike(x->children[i], cutoff_time);

        // Check if the transaction is recent enough
        if (x->transactions[i].date_time >= cutoff_time) {
            count++;
        } else {
             /* Optimization: Since the keys are sorted (oldest to newest),
             if this transaction is older than the cutoff, all remaining
             transactions in this node and all remaining children (i+1 to n)
             will also be older. We can stop searching the rest of this branch.
             This relies on the long long time_key sorting being temporally accurate.
             We return here to stop the recursion. */
             return count;
        }
    }

    // Check the last child
    count += checkVelocitySpike(x->children[x->n], cutoff_time);

    return count;
}


void checkTransactionSpike(BTreeNode *x, float debit_threshold, float credit_threshold, int *debit_fraud_count, int *credit_fraud_count) {
    if (x == NULL) return;

    for (int i = 0; i < x->n; i++) {
        checkTransactionSpike(x->children[i], debit_threshold, credit_threshold, debit_fraud_count, credit_fraud_count);

        if (x->transactions[i].type == 'D' && x->transactions[i].amount > debit_threshold) {
            printf("        !!! FRAUD ALERT: High-Value Debit Transaction Detected (Above Rs.%.2f) !!!\n", debit_threshold);
            printf("        -> Transaction ID: %d, Amount: Rs.%.2f, Channel: %s, Terminal: %d\n",
                   x->transactions[i].id,
                   x->transactions[i].amount,
                   x->transactions[i].channel,
                   x->transactions[i].terminal_id);
            (*debit_fraud_count)++;
        } else if (x->transactions[i].type == 'C' && x->transactions[i].amount > credit_threshold) {
            printf("        !!! SUSPICIOUS CREDIT: High-Value Credit Transaction Detected (Above Rs.%.2f) !!!\n", credit_threshold);
            printf("        -> Transaction ID: %d, Amount: Rs.%.2f, Counterparty: %d\n",
                   x->transactions[i].id,
                   x->transactions[i].amount,
                   x->transactions[i].counterparty_id);
            (*credit_fraud_count)++;
        }
    }
    checkTransactionSpike(x->children[x->n], debit_threshold, credit_threshold, debit_fraud_count, credit_fraud_count);
}

void analyzeCustomerForFraud(HashMap *map, int customerId) {
    Customer *customer = findCustomer(map, customerId);

    if (customer == NULL) {
        printf("\n[ERROR] Customer ID %d not found in the system.\n", customerId);
        return;
    }

    printf("\n--- Real-time Fraud Analysis for %s (ID: %d) ---\n", customer->name, customer->id);
    if (customer->b_tree_root == NULL || customer->b_tree_root->n == 0) {
        printf("No transactions to analyze.\n");
        return;
    }

    int debit_fraud_count = 0;
    int credit_fraud_count = 0;

    time_t current_time = time(NULL);
    time_t cutoff_time = current_time - SECONDS_IN_HOUR;

    float debit_thr = customer->debit_threshold;
    float credit_thr = customer->credit_threshold;

    // --- NEW VELOCITY CHECK ---
    int velocity_count = checkVelocitySpike(customer->b_tree_root, cutoff_time);

    printf("1. Checking Transaction Velocity (Past 1 Hour):\n");
    if (velocity_count >= TXN_LIMIT_PER_HOUR) {
        printf("        !!! FRAUD ALERT: EXTREME VELOCITY DETECTED !!!\n");
        printf("        -> %d transactions detected in the last hour. Hard Limit: %d.\n", velocity_count, TXN_LIMIT_PER_HOUR);
        debit_fraud_count++; // Treat hitting the hard limit as a major incident
    } else if (velocity_count >= TXN_WARNING_THRESHOLD) {
        printf("        !!! SUSPICION WARNING: High Velocity Detected !!!\n");
        printf("        -> %d transactions detected in the last hour. Warning Threshold: %d.\n", velocity_count, TXN_WARNING_THRESHOLD);
    } else {
        printf("        -> Transaction velocity (%d/hour) is normal.\n", velocity_count);
    }
    // --------------------------

    printf("\n2. Checking for high-value transactions:\n");

    checkTransactionSpike(customer->b_tree_root, debit_thr, credit_thr, &debit_fraud_count, &credit_fraud_count);

    if (debit_fraud_count == 0 && credit_fraud_count == 0 && velocity_count < TXN_WARNING_THRESHOLD) {
        printf("\nSummary: No major fraud or suspicion alerts detected.\n");
    } else {
        printf("\nSummary:\n");
        if (debit_fraud_count > 0) printf("    ** ALERT: %d High-Value Debit Spike(s) detected. **\n", debit_fraud_count);
        if (credit_fraud_count > 0) printf("    ** ALERT: %d Suspicious Credit Spike(s) detected. **\n", credit_fraud_count);
        if (velocity_count >= TXN_LIMIT_PER_HOUR) printf("    ** CRITICAL: Transaction Velocity Limit Exceeded. **\n");
    }
}


// --- D. Initialization & Menu Handlers ---

Customer* createCustomer(int id, const char *name, float debit_thr, float credit_thr) {
    Customer *newCustomer = (Customer*)malloc(sizeof(Customer));
    if (!newCustomer) {
        perror("Memory allocation failed for Customer");
        exit(EXIT_FAILURE);
    }
    newCustomer->id = id;
    strncpy(newCustomer->name, name, MAX_CUSTOMER_NAME - 1);
    newCustomer->name[MAX_CUSTOMER_NAME - 1] = '\0';
    newCustomer->b_tree_root = createBTreeNode(true);
    newCustomer->debit_threshold = debit_thr;
    newCustomer->credit_threshold = credit_thr;
    newCustomer->next = NULL;
    return newCustomer;
}

// SIMPLIFIED: Using standard time(NULL) and a random element for the key
Transaction generateTransaction(int id, float amount, char type, int counterpartyId, const char* channel, int terminalId) {
    Transaction t;
    t.id = id;
    t.amount = amount;
    t.type = type;

    // Use standard time()
    time_t current_time = time(NULL);
    t.date_time = current_time;

    // Create a key based on seconds, augmented by a random number
    t.time_key = (long long)current_time * 1000000LL + (rand() % 1000000);

    t.counterparty_id = counterpartyId;
    strncpy(t.channel, channel, 9);
    t.channel[9] = '\0';
    t.terminal_id = terminalId;
    return t;
}

void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* discard */ }
}

void handleAddCustomer(HashMap *map) {
    int id;
    char name[MAX_CUSTOMER_NAME];
    float debit_thr, credit_thr;

    printf("\n--- Add New Customer ---\n");
    printf("Enter new customer ID: ");
    if (scanf("%d", &id) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    Customer* existing = findCustomer(map, id);
    if (existing != NULL) {
        printf("Error: Customer ID %d already exists (Name: %s).\n", id, existing->name);
        return;
    }

    printf("Enter new customer name: ");
    if (!fgets(name, sizeof(name), stdin)) {
        printf("Input error.\n");
        return;
    }
    name[strcspn(name, "\n")] = 0;

    printf("Enter custom DEBIT fraud threshold (e.g., 500000.00): ");
    if (scanf("%f", &debit_thr) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    printf("Enter custom CREDIT suspicion threshold (e.g., 1000000.00): ");
    if (scanf("%f", &credit_thr) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    Customer *newCustomer = createCustomer(id, name, debit_thr, credit_thr);
    insertCustomer(map, newCustomer);

    printf("Success: Customer %s (ID: %d) added with DEBIT threshold Rs.%.2f and CREDIT threshold Rs.%.2f.\n",
           newCustomer->name, newCustomer->id, newCustomer->debit_threshold, newCustomer->credit_threshold);
    printf("        (Hash index: %d)\n", hashFunction(newCustomer->id));
}

void handleAddTransaction(HashMap *map) {
    int custId, transId, counterpartyId, terminalId;
    float amount;
    char type;
    char channel[10];

    printf("\n--- Add New Transaction ---\n");
    printf("Enter Customer ID for the transaction: ");
    if (scanf("%d", &custId) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    Customer *customer = findCustomer(map, custId);
    if (customer == NULL) {
        printf("Error: Customer ID %d not found. Cannot add transaction.\n", custId);
        return;
    }

    printf("Transaction for %s (ID: %d)\n", customer->name, customer->id);

    printf("Enter Transaction ID (for record keeping): ");
    if (scanf("%d", &transId) != 1) { clearInputBuffer(); return; }
    clearInputBuffer();

    if (findTransactionByID(customer->b_tree_root, transId) != NULL) {
        printf("\n[ERROR] Transaction ID %d already exists for customer %d. Please use a unique ID.\n", transId, custId);
        return;
    }

    printf("Enter Amount (in Rs.): ");
    if (scanf("%f", &amount) != 1) { clearInputBuffer(); return; }
    clearInputBuffer();

    printf("Enter Type (D for Debit, C for Credit): ");
    if (scanf(" %c", &type) != 1 || (type != 'D' && type != 'C')) {
        printf("Invalid type. Must be 'D' or 'C'.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    printf("Enter Counterparty ID: ");
    if (scanf("%d", &counterpartyId) != 1) { clearInputBuffer(); return; }
    clearInputBuffer();

    printf("Enter Channel (e.g., WEB, ATM, APP): ");
    if (scanf("%9s", channel) != 1) {
        printf("Invalid channel input.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    printf("Enter Terminal ID: ");
    if (scanf("%d", &terminalId) != 1) { clearInputBuffer(); return; }
    clearInputBuffer();

    Transaction t = generateTransaction(transId, amount, type, counterpartyId, channel, terminalId);
    insertTransaction(&customer->b_tree_root, t);

    printf("Success: Transaction %d added for customer %d. (Time Key: %lld)\n", transId, custId, t.time_key);
}

void handleAnalyzeCustomer(HashMap *map) {
    int custId;
    printf("\n--- Analyze Customer ---\n");
    printf("Enter Customer ID to analyze: ");
    if (scanf("%d", &custId) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    analyzeCustomerForFraud(map, custId);
}

void handleShowHistory(HashMap *map) {
    int custId;
    printf("\n--- Show Transaction History ---\n");
    printf("Enter Customer ID to view history: ");
    if (scanf("%d", &custId) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    Customer *customer = findCustomer(map, custId);

    if (customer == NULL) {
        printf("\n[ERROR] Customer ID %d not found in the system.\n", custId);
        return;
    }

    printf("\n--- Transaction History for %s (ID: %d) ---\n", customer->name, customer->id);
    if (customer->b_tree_root == NULL || customer->b_tree_root->n == 0) {
        printf("No transactions found.\n");
        return;
    }

    printf("(Transactions sorted by Time Key - Oldest to Newest):\n");
    printBTreeTransactions(customer->b_tree_root);
}


// --- Main Function ---

int main(void) {
    srand((unsigned)time(NULL));

    HashMap bankSystem;
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        bankSystem.table[i] = NULL;
    }

    printf("--- Banking System Initialization Complete ---\n");

    int choice = -1;
    while (choice != 0) {
        printf("\n==========================================\n");
        printf("             DS Banking system\n");
        printf("==========================================\n");
        printf("1. Add New Customer\n");
        printf("2. Add Transaction\n");
        printf("3. Analyze Customer for Fraud\n");
        printf("4. Show Transaction History\n");
        printf("0. Exit\n");
        printf("------------------------------------------\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number (0-4).\n");
            clearInputBuffer();
            choice = -1;
            continue;
        }
        clearInputBuffer();

        switch (choice) {
            case 1:
                handleAddCustomer(&bankSystem);
                break;
            case 2:
                handleAddTransaction(&bankSystem);
                break;
            case 3:
                handleAnalyzeCustomer(&bankSystem);
                break;
            case 4:
                handleShowHistory(&bankSystem);
                break;
            case 0:
                printf("\n--- System Shutdown. Exiting. ---\n");
                break;
            default:
                printf("\nInvalid choice. Please select from the menu options (0-4).\n");
                break;
        }
    }

    freeHashMap(&bankSystem);

    return 0;
}
