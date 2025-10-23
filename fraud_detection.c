#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// --- B-TREE PARAMETERS ---
// T (minimum degree) controls the size of the nodes.
// Max keys per node: 2*T - 1 (e.g., 5 transactions)
// Max children per node: 2*T (e.g., 6 pointers)
#define T 3
#define MAX_TRANSACTIONS (2 * T - 1)
#define MAX_CHILDREN (2 * T)

// --- HASH MAP PARAMETERS ---
#define HASH_MAP_SIZE 10
#define MAX_CUSTOMER_NAME 50

// --- Data Structures Definitions ---

// 1. Transaction Structure (UPDATED with Counterparty, Channel, and Terminal ID)
typedef struct Transaction {
    int id;
    float amount;
    time_t date_time; // Using time_t for realistic timestamps
    char type;        // 'D' for Debit, 'C' for Credit
    // NEW ATTRIBUTES FOR ENHANCED FRAUD ANALYSIS
    int counterparty_id; // For detecting circular transfers (the other account ID)
    char channel[10];    // ATM, WEB, APP
    int terminal_id;     // Location/Terminal marker for geo-anomaly
} Transaction;

// 2. B-Tree Node Structure (Stores Transactions)
typedef struct BTreeNode {
    Transaction transactions[MAX_TRANSACTIONS];
    struct BTreeNode *children[MAX_CHILDREN];
    int n;          // Current number of transactions stored
    bool is_leaf;
} BTreeNode;

// 3. Customer Structure (Hash Map Payload)
typedef struct Customer {
    int id;
    char name[MAX_CUSTOMER_NAME];
    BTreeNode *b_tree_root; // Root of the customer's personal transaction B-Tree
    struct Customer *next;  // For Hash Map Chaining
} Customer;

// 4. Hash Map Structure
typedef struct HashMap {
    Customer *table[HASH_MAP_SIZE];
} HashMap;

// --- Helper Functions for Data Structure Operations ---

// --- A. B-Tree Operations (Simplified B-Tree-like Search Tree) ---

BTreeNode* createBTreeNode(bool leaf) {
    BTreeNode *newNode = (BTreeNode*)malloc(sizeof(BTreeNode));
    if (!newNode) {
        perror("Memory allocation failed for BTreeNode");
        exit(EXIT_FAILURE);
    }
    newNode->is_leaf = leaf;
    newNode->n = 0;
    // Initialize children pointers to NULL
    for (int i = 0; i < MAX_CHILDREN; i++) {
        newNode->children[i] = NULL;
    }
    return newNode;
}

// Function to insert a key into a non-full node (simplified for this demo)
// Note: This implementation is a simplified B-Tree search/insertion, not a full B-Tree
// with splitting logic, which would require significant complexity.
void insertNonFull(BTreeNode *x, Transaction t) {
    int i = x->n - 1;

    // If it's a leaf, shift elements and insert the new transaction
    if (x->is_leaf) {
        // Find the correct position based on transaction ID (the key)
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
        i++; // Index of the child pointer

        // Check if the child is full (simplified condition)
        if (x->children[i] == NULL) {
            // If child doesn't exist, create it as a leaf for insertion
            x->children[i] = createBTreeNode(true);
        } else if (x->children[i]->n == MAX_TRANSACTIONS) {
             printf("\n[Warning]: B-Tree node full (%d keys). Transaction not inserted. (In a real B-Tree, this node would split).\n", MAX_TRANSACTIONS);
             return;
        }

        insertNonFull(x->children[i], t);
    }
}

// Public wrapper for inserting a transaction into a customer's B-Tree
void insertTransaction(BTreeNode **root, Transaction t) {
    if (*root == NULL) {
        *root = createBTreeNode(true);
    }

    // Check if the root node is full (simplified condition)
    if ((*root)->n == MAX_TRANSACTIONS) {
        printf("\n[Warning]: Root node full. Cannot insert more transactions.\n");
        // A real B-Tree would create a new root and split the old root here to maintain balance.
    } else {
        insertNonFull(*root, t);
    }
}

// Function to traverse the B-Tree and print all transactions (In-order traversal) (UPDATED)
void printBTreeTransactions(BTreeNode *x) {
    if (x == NULL) return;

    int i;
    for (i = 0; i < x->n; i++) {
        // Recursively print the transactions in the i-th child (smaller than key)
        printBTreeTransactions(x->children[i]);

        // Print the transaction itself
        char time_buffer[30];
        strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&x->transactions[i].date_time));
        printf("  - ID: %d, Type: %c, Amount: $%.2f, Date: %s | Counterparty: %d, Channel: %s, Terminal: %d\n",
               x->transactions[i].id,
               x->transactions[i].type,
               x->transactions[i].amount,
               time_buffer,
               x->transactions[i].counterparty_id,
               x->transactions[i].channel,
               x->transactions[i].terminal_id);
    }

    // Recursively print the transactions in the last child (larger than the last key)
    printBTreeTransactions(x->children[i]);
}

// --- B. Hash Map Operations (Customer Lookup) ---

// Simple hash function (Division method)
int hashFunction(int customerId) {
    return customerId % HASH_MAP_SIZE;
}

// Function to insert a new customer into the Hash Map (with chaining)
void insertCustomer(HashMap *map, Customer *newCustomer) {
    int index = hashFunction(newCustomer->id);
    newCustomer->next = map->table[index];
    map->table[index] = newCustomer;
}

// Function to find a customer in the Hash Map
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
 * Traverses a customer's B-Tree to check for a basic fraud pattern:
 * A single transaction above a certain threshold (e.g., $5,000).
 * This function utilizes the efficient tree traversal property.
 */
void checkFraudulentSpike(BTreeNode *x, float threshold, int *fraud_count) {
    if (x == NULL) return;

    for (int i = 0; i < x->n; i++) {
        // Recursive check in the child node
        checkFraudulentSpike(x->children[i], threshold, fraud_count);

        // Check the transaction itself
        if (x->transactions[i].type == 'D' && x->transactions[i].amount > threshold) {
            printf("      !!! FRAUD ALERT: High-Value Debit Transaction Detected !!!\n");
            printf("      -> Transaction ID: %d, Amount: $%.2f, Channel: %s, Terminal: %d\n",
                   x->transactions[i].id,
                   x->transactions[i].amount,
                   x->transactions[i].channel,
                   x->transactions[i].terminal_id);
            (*fraud_count)++;
        }
    }

    // Recursive check in the last child node
    checkFraudulentSpike(x->children[x->n], threshold, fraud_count);
}


// Public wrapper for fraud detection
void analyzeCustomerForFraud(HashMap *map, int customerId) {
    // Step 1: O(1) Lookup using Hash Map
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
    float threshold = 5000.00; // Define a simple fraud threshold

    // Step 2: Traverse B-Tree for sorted analysis
    printf("1. Checking for high-value debits (Threshold: $%.2f):\n", threshold);
    checkFraudulentSpike(customer->b_tree_root, threshold, &fraud_count);

    if (fraud_count == 0) {
        printf("      -> No high-value fraud spikes detected.\n");
    } else {
        printf("   ** System detected %d potential fraudulent spike(s). **\n", fraud_count);
    }

    printf("\n2. Full Transaction History (Sorted by ID):\n");
    printBTreeTransactions(customer->b_tree_root);
}

// --- D. Initialization and Demo ---

// Helper function to create a new customer struct
Customer* createCustomer(int id, const char *name) {
    Customer *newCustomer = (Customer*)malloc(sizeof(Customer));
    if (!newCustomer) {
        perror("Memory allocation failed for Customer");
        exit(EXIT_FAILURE);
    }
    newCustomer->id = id;
    strncpy(newCustomer->name, name, MAX_CUSTOMER_NAME - 1);
    newCustomer->name[MAX_CUSTOMER_NAME - 1] = '\0';
    newCustomer->b_tree_root = createBTreeNode(true); // Start with an empty, leaf root
    newCustomer->next = NULL;
    return newCustomer;
}

// Helper function to generate a transaction (UPDATED)
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

int main() {
    // Seed the random number generator for transaction IDs/times
    srand(time(NULL));

    // Initialize the main Hash Map
    HashMap bankSystem;
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        bankSystem.table[i] = NULL;
    }

    // 1. Setup Customers and Insert into Hash Map (O(1) lookup time)
    Customer *cust1 = createCustomer(1001, "Alice Johnson");
    Customer *cust2 = createCustomer(2002, "Bob Smith");
    Customer *cust3 = createCustomer(3003, "Charlie Brown"); // Customer for a fraudulent demo

    insertCustomer(&bankSystem, cust1);
    insertCustomer(&bankSystem, cust2);
    insertCustomer(&bankSystem, cust3);

    printf("--- Banking System Initialization Complete ---\n");
    printf("Customers added to Hash Map. (Hash index check: Alice -> %d, Bob -> %d, Charlie -> %d)\n",
           hashFunction(1001), hashFunction(2002), hashFunction(3003));


    // 2. Add Transactions to Customer B-Trees (UPDATED with new parameters)
    printf("\n--- Adding Transactions to Customer B-Trees ---\n");

    // Transactions for Alice (Normal Activity)
    insertTransaction(&cust1->b_tree_root, generateTransaction(1, 150.00, 'D', 9001, "WEB", 101));
    insertTransaction(&cust1->b_tree_root, generateTransaction(2, 500.00, 'C', 4004, "APP", 101));
    insertTransaction(&cust1->b_tree_root, generateTransaction(3, 75.00, 'D', 9002, "WEB", 102));

    // Transactions for Charlie (Normal + Fraudulent Activity)
    insertTransaction(&cust3->b_tree_root, generateTransaction(10, 25.00, 'D', 9003, "APP", 205));
    insertTransaction(&cust3->b_tree_root, generateTransaction(11, 120.00, 'C', 4005, "WEB", 205));
    insertTransaction(&cust3->b_tree_root, generateTransaction(12, 4500.00, 'D', 9004, "ATM", 310)); // Near miss, unusual channel/location
    insertTransaction(&cust3->b_tree_root, generateTransaction(13, 8999.99, 'D', 9005, "ATM", 310)); // **FRAUD SPIKE**, unusual channel/location
    insertTransaction(&cust3->b_tree_root, generateTransaction(14, 50.00, 'C', 4006, "APP", 205));


    // 3. Demonstrate System Functionality (Lookup and Analysis)
    int targetId = 3003; // Check the fraudulent customer

    printf("\n======================================================\n");
    printf("--- DEMO: Lookup Customer and Run Fraud Detection ---\n");
    printf("Lookup Target: Customer ID %d (O(1) Hash Map access)\n", targetId);
    printf("======================================================\n");

    analyzeCustomerForFraud(&bankSystem, targetId);

    // 4. Check a non-fraudulent customer
    targetId = 1001;
    printf("\n======================================================\n");
    printf("Lookup Target: Customer ID %d (O(1) Hash Map access)\n", targetId);
    printf("======================================================\n");
    analyzeCustomerForFraud(&bankSystem, targetId);

    // 5. Cleanup (Free memory) - essential for C
    // Note: A full free requires complex B-Tree post-order traversal and Hash Map iteration,
    // which is omitted here for brevity and focus on the main demonstration logic.
    printf("\n--- System Demo Concluded. ---\n");
    return 0;
}
