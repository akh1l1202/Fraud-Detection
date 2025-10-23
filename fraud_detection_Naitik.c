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

#define HASH_MAP_SIZE 10
#define MAX_CUSTOMER_NAME 50

// --- Data Structures ---

typedef struct Transaction {
    int id;
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
    float fraud_threshold; // Custom threshold per customer
    struct Customer *next;  // For Hash Map Chaining
} Customer;

typedef struct HashMap {
    Customer *table[HASH_MAP_SIZE];
} HashMap;


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

// Note: This is a simplified insertion, not a full B-Tree.
// It doesn't handle node splitting when full.
void insertNonFull(BTreeNode *x, Transaction t) {
    int i = x->n - 1;

    if (x->is_leaf) {
        // Find correct position based on transaction ID
        while (i >= 0 && x->transactions[i].id > t.id) {
            x->transactions[i + 1] = x->transactions[i];
            i--;
        }
        x->transactions[i + 1] = t;
        x->n++;
    } else {
        // Find the child where the new key should be inserted
        while (i >= 0 && x->transactions[i].id > t.id) {
            i--;
        }
        i++;

        if (x->children[i] == NULL) {
            x->children[i] = createBTreeNode(true);
        } else if (x->children[i]->n == MAX_TRANSACTIONS) {
             printf("\n[Warning]: B-Tree node full. Transaction not inserted. (Node splitting not implemented).\n");
             return;
        }

        insertNonFull(x->children[i], t);
    }
}

void insertTransaction(BTreeNode **root, Transaction t) {
    if (*root == NULL) {
        *root = createBTreeNode(true);
    }

    if ((*root)->n == MAX_TRANSACTIONS) {
        printf("\n[Warning]: Root node full. Cannot insert. (Root splitting not implemented).\n");
    } else {
        insertNonFull(*root, t);
    }
}

// In-order traversal to print all transactions
void printBTreeTransactions(BTreeNode *x) {
    if (x == NULL) return;

    int i;
    for (i = 0; i < x->n; i++) {
        printBTreeTransactions(x->children[i]);

        // Print transaction details
        char time_buffer[30];
        strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&x->transactions[i].date_time));
        // --- CURRENCY CHANGED ---
        printf("  - ID: %d, Type: %c, Amount: Rs.%.2f, Date: %s | Counterparty: %d, Channel: %s, Terminal: %d\n",
               x->transactions[i].id,
               x->transactions[i].type,
               x->transactions[i].amount,
               time_buffer,
               x->transactions[i].counterparty_id,
               x->transactions[i].channel,
               x->transactions[i].terminal_id);
    }
    // Print transactions in the last child
    printBTreeTransactions(x->children[i]);
}

// --- B. Hash Map Operations ---

// Simple hash function (Division method)
int hashFunction(int customerId) {
    return customerId % HASH_MAP_SIZE;
}

void insertCustomer(HashMap *map, Customer *newCustomer) {
    int index = hashFunction(newCustomer->id);
    // Chain by inserting at the head of the list
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
    return NULL; // Customer not found
}

// --- C. Core Fraud Detection Logic ---

/**
 * @brief Traverses the B-Tree checking for high-value debit transactions.
 */
void checkFraudulentSpike(BTreeNode *x, float threshold, int *fraud_count) {
    if (x == NULL) return;

    for (int i = 0; i < x->n; i++) {
        checkFraudulentSpike(x->children[i], threshold, fraud_count);

        // Check the transaction itself
        if (x->transactions[i].type == 'D' && x->transactions[i].amount > threshold) {
            // --- CURRENCY CHANGED ---
            printf("      !!! FRAUD ALERT: High-Value Debit Transaction Detected (Above Rs.%.2f) !!!\n", threshold);
            // --- CURRENCY CHANGED ---
            printf("      -> Transaction ID: %d, Amount: Rs.%.2f, Channel: %s, Terminal: %d\n",
                   x->transactions[i].id,
                   x->transactions[i].amount,
                   x->transactions[i].channel,
                   x->transactions[i].terminal_id);
            (*fraud_count)++;
        }
    }
    // Check the last child
    checkFraudulentSpike(x->children[x->n], threshold, fraud_count);
}

/**
 * @brief Public-facing function to run a full fraud analysis on a customer.
 */
void analyzeCustomerForFraud(HashMap *map, int customerId) {
    // Step 1: O(1) average time lookup
    Customer *customer = findCustomer(map, customerId);

    if (customer == NULL) {
        printf("\n[ERROR] Customer ID %d not found in the system.\n", customerId);
        return;
    }

    printf("\n--- Real-time Fraud Analysis for %s (ID: %d) ---\n", customer->name, customer->id);
    if (customer->b_tree_root == NULL) {
        printf("No transactions to analyze.\n");
        return;
    }

    int fraud_count = 0;
    // Use the customer's specific threshold
    float threshold = customer->fraud_threshold;

    // Step 2: Traverse B-Tree to find anomalies
    // --- CURRENCY CHANGED ---
    printf("1. Checking for high-value debits (Custom Threshold: Rs.%.2f):\n", threshold);
    checkFraudulentSpike(customer->b_tree_root, threshold, &fraud_count);

    if (fraud_count == 0) {
        printf("      -> No high-value fraud spikes detected.\n");
    } else {
        printf("   ** System detected %d potential fraudulent spike(s). **\n", fraud_count);
    }

    printf("\n2. Full Transaction History (Sorted by ID):\n");
    printBTreeTransactions(customer->b_tree_root);
}

// --- D. Initialization & Menu Handlers ---

Customer* createCustomer(int id, const char *name, float threshold) {
    Customer *newCustomer = (Customer*)malloc(sizeof(Customer));
    if (!newCustomer) {
        perror("Memory allocation failed for Customer");
        exit(EXIT_FAILURE);
    }
    newCustomer->id = id;
    strncpy(newCustomer->name, name, MAX_CUSTOMER_NAME - 1);
    newCustomer->name[MAX_CUSTOMER_NAME - 1] = '\0';
    newCustomer->b_tree_root = createBTreeNode(true);
    newCustomer->fraud_threshold = threshold;
    newCustomer->next = NULL;
    return newCustomer;
}

Transaction generateTransaction(int id, float amount, char type, int counterpartyId, const char* channel, int terminalId) {
    Transaction t;
    t.id = id;
    t.amount = amount;
    t.type = type;
    t.date_time = time(NULL) + (rand() % 1000); // Simulate time progression
    t.counterparty_id = counterpartyId;
    strncpy(t.channel, channel, 9);
    t.channel[9] = '\0';
    t.terminal_id = terminalId;
    return t;
}

/**
 * @brief Clears the standard input buffer.
 * Call after scanf() to consume trailing newline and prevent errors.
 */
void clearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * @brief Handles user input for adding a new customer.
 */
void handleAddCustomer(HashMap *map) {
    int id;
    char name[MAX_CUSTOMER_NAME];
    float threshold;

    printf("\n--- Add New Customer ---\n");
    printf("Enter new customer ID: ");
    if (scanf("%d", &id) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    // Check for duplicate ID
    Customer* existing = findCustomer(map, id);
    if (existing != NULL) {
        printf("Error: Customer ID %d already exists (Name: %s).\n", id, existing->name);
        return;
    }

    printf("Enter new customer name: ");
    fgets(name, MAX_CUSTOMER_NAME, stdin);
    name[strcspn(name, "\n")] = 0; // Remove trailing newline from fgets

    // --- CURRENCY CHANGED (example) ---
    printf("Enter custom fraud threshold for this customer (e.g., 500000.00): ");
    if (scanf("%f", &threshold) != 1) {
        printf("Invalid input. Please enter a number.\n");
        clearInputBuffer();
        return;
    }
    clearInputBuffer();

    Customer *newCustomer = createCustomer(id, name, threshold);
    insertCustomer(map, newCustomer);

    // --- CURRENCY CHANGED ---
    printf("Success: Customer %s (ID: %d) added with fraud threshold Rs.%.2f.\n",
           newCustomer->name, newCustomer->id, newCustomer->fraud_threshold);
    printf("         (Hash index: %d)\n", hashFunction(newCustomer->id));
}

/**
 * @brief Handles user input for adding a new transaction.
 */
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

    // Ensure customer exists before proceeding
    Customer *customer = findCustomer(map, custId);
    if (customer == NULL) {
        printf("Error: Customer ID %d not found. Cannot add transaction.\n", custId);
        return;
    }

    printf("Transaction for %s (ID: %d)\n", customer->name, customer->id);

    printf("Enter Transaction ID (as key): ");
    if (scanf("%d", &transId) != 1) { clearInputBuffer(); return; }
    clearInputBuffer();

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
    if (scanf("%9s", channel) != 1) { // Read max 9 chars to leave room for '\0'
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

    printf("Success: Transaction %d added for customer %d.\n", transId, custId);
}

/**
 * @brief Handles user input for selecting a customer to analyze.
 */
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

    // The analysis function has its own NULL check
    analyzeCustomerForFraud(map, custId);
}


// --- Main Function ---

int main() {
    srand(time(NULL)); // Seed random number generator once

    HashMap bankSystem;
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        bankSystem.table[i] = NULL;
    }

    printf("--- Banking System Initialization Complete ---\n");

    int choice = -1;
    while (choice != 0) {
        printf("\n==========================================\n");
        printf("         DS Banking system\n");
        printf("==========================================\n");
        printf("1. Add New Customer\n");
        printf("2. Add Transaction\n");
        printf("3. Analyze Customer for Fraud\n");
        printf("0. Exit\n");
        printf("------------------------------------------\n");
        printf("Enter your choice: ");

        // Validate user input
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number (0-3).\n");
            clearInputBuffer();
            choice = -1; // Reset choice to loop again
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
            case 0:
                printf("\n--- System Shutdown. Exiting. ---\n");
                break;
            default:
                printf("\nInvalid choice. Please select from the menu options (0-3).\n");
                break;
        }
    }

    // Note: A full application would need to free all allocated memory
    // (all B-Tree nodes and all Customer structs) here.
    return 0;
}
