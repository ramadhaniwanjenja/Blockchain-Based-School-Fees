#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "blockchain.h"
#include "sha256.h"

//file paths
#define CHAIN_FILE   "data/chain.bin"
#define PENDING_FILE "data/pending.bin"
#define DIFF_FILE    "data/difficulty.txt"

//global state
static Blockchain bc;
static TxPool     pool;

//helper: safe line input
static void read_line(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
    //trim leading/trailing whitespace
    int start = 0;
    while (buf[start] && isspace((unsigned char)buf[start])) start++;
    if (start) memmove(buf, buf + start, strlen(buf) - start + 1);
    int end = (int)strlen(buf) - 1;
    while (end >= 0 && isspace((unsigned char)buf[end])) buf[end--] = '\0';
}

//persistence helpers
static void save_all(void) {
    blockchain_save(&bc, CHAIN_FILE);

    //save pending pol
    FILE *f = fopen(PENDING_FILE, "wb");
    if (f) {
        fwrite(&pool.count, sizeof(int), 1, f);
        fwrite(pool.txs, sizeof(Transaction), pool.count, f);
        fclose(f);
    }
}

static void load_all(int default_difficulty) {
    //try to load existin chain
    if (!blockchain_load(&bc, CHAIN_FILE)) {
        printf("No existing chain found. Initialising genesis block...\n");
        blockchain_init(&bc, default_difficulty);
        save_all();
    }

    //load pendin pool
    pool_init(&pool);
    FILE *f = fopen(PENDING_FILE, "rb");
    if (f) {
        int count = 0;
        fread(&count, sizeof(int), 1, f);
        if (count > MAX_PENDING) count = MAX_PENDING;
        fread(pool.txs, sizeof(Transaction), count, f);
        pool.count = count;
        fclose(f);
    }
}

//CLI handlers

static void cmd_invoice_create(void) {
    char student_id[MAX_STUDENT_ID];
    char invoice_id[MAX_INVOICE_ID];
    char amount_str[32];
    char note[MAX_REF];
    double amount;

    printf("\n--- Create Invoice ---\n");

    //Student ID
    do {
        read_line("  Student ID (alphanumeric, 3-31 chars): ", student_id, sizeof(student_id));
        if (!validate_student_id(student_id))
            printf("  [!] Invalid student ID. Use letters, numbers, '-', '_'.\n");
    } while (!validate_student_id(student_id));

    //Invoice ID must be unique
    do {
        read_line("  Invoice ID (alphanumeric, 3-31 chars): ", invoice_id, sizeof(invoice_id));
        if (!validate_invoice_id(invoice_id)) {
            printf("  [!] Invalid invoice ID.\n");
            continue;
        }
        if (invoice_exists(&bc, &pool, invoice_id)) {
            printf("  [!] Invoice ID already exists. Choose another.\n");
            invoice_id[0] = '\0';
        }
    } while (!validate_invoice_id(invoice_id) || invoice_id[0] == '\0');

    //Amount
    do {
        read_line("  Total Amount (RWF): ", amount_str, sizeof(amount_str));
        amount = atof(amount_str);
        if (!validate_amount(amount))
            printf("  [!] Amount must be positive.\n");
    } while (!validate_amount(amount));

    read_line("  Note/Description: ", note, sizeof(note));
    if (strlen(note) == 0) strncpy(note, "ALU Tuition Invoice", sizeof(note)-1);

    Transaction tx;
    memset(&tx, 0, sizeof(tx));
    tx.type       = TX_INVOICE_CREATE;
    tx.event_time = time(NULL);
    tx.amount     = amount;
    tx.balance    = amount;
    tx.confirmed  = 1;
    strncpy(tx.student_id, student_id, MAX_STUDENT_ID - 1);
    strncpy(tx.invoice_id, invoice_id, MAX_INVOICE_ID - 1);
    strncpy(tx.reference,  note,       MAX_REF - 1);

    if (pool_add(&pool, &tx)) {
        save_all();
        printf("  [OK] Invoice %s created for student %s — %.2f RWF\n",
               invoice_id, student_id, amount);
        printf("  [*] Pending — run 'mine' to commit to blockchain.\n");
    }
}

static void cmd_payment_record(void) {
    char invoice_id[MAX_INVOICE_ID];
    char amount_str[32];
    char ref[MAX_REF];
    double pay_amount;

    printf("\n--- Record Payment ---\n");

    do {
        read_line("  Invoice ID: ", invoice_id, sizeof(invoice_id));
        if (!validate_invoice_id(invoice_id)) {
            printf("  [!] Invalid invoice ID.\n");
            continue;
        }
        if (!invoice_exists(&bc, &pool, invoice_id)) {
            printf("  [!] Invoice not found.\n");
            invoice_id[0] = '\0';
        } else if (invoice_settled(&bc, invoice_id)) {
            printf("  [!] Invoice is already fully settled.\n");
            return;
        }
    } while (!validate_invoice_id(invoice_id) || invoice_id[0] == '\0');

    double current_balance = get_balance(&bc, &pool, invoice_id);
    printf("  Current outstanding balance: %.2f RWF\n", current_balance);

    do {
        read_line("  Payment Amount (RWF): ", amount_str, sizeof(amount_str));
        pay_amount = atof(amount_str);
        if (!validate_amount(pay_amount))
            printf("  [!] Amount must be positive.\n");
        else if (pay_amount > current_balance + 0.001) {
            printf("  [!] Payment (%.2f) exceeds balance (%.2f).\n",
                   pay_amount, current_balance);
            pay_amount = -1;
        }
    } while (pay_amount <= 0);

    read_line("  Payment Reference: ", ref, sizeof(ref));
    if (strlen(ref) == 0) strncpy(ref, "PAYMENT", sizeof(ref)-1);

    double new_balance = current_balance - pay_amount;

    Transaction tx;
    memset(&tx, 0, sizeof(tx));
    tx.type       = TX_PAYMENT_MADE;
    tx.event_time = time(NULL);
    tx.amount     = pay_amount;
    tx.balance    = new_balance;
    tx.confirmed  = 0;   //awaiting confirmation
    strncpy(tx.invoice_id, invoice_id, MAX_INVOICE_ID - 1);
    strncpy(tx.reference,  ref,        MAX_REF - 1);

    //Copy student_id from the invoice creation record
    for (int i = 0; i < bc.length; i++)
        for (int j = 0; j < bc.blocks[i].tx_count; j++)
            if (bc.blocks[i].transactions[j].type == TX_INVOICE_CREATE &&
                strcmp(bc.blocks[i].transactions[j].invoice_id, invoice_id) == 0)
                strncpy(tx.student_id,
                        bc.blocks[i].transactions[j].student_id,
                        MAX_STUDENT_ID - 1);
    for (int i = 0; i < pool.count; i++)
        if (pool.txs[i].type == TX_INVOICE_CREATE &&
            strcmp(pool.txs[i].invoice_id, invoice_id) == 0)
            strncpy(tx.student_id, pool.txs[i].student_id, MAX_STUDENT_ID - 1);

    if (pool_add(&pool, &tx)) {
        save_all();
        printf("  [OK] Payment of %.2f RWF recorded (ref: %s).\n",
               pay_amount, ref);
        printf("  [*] Remaining balance: %.2f RWF\n", new_balance);
        printf("  [*] Pending confirmation — run 'mine' then 'payment confirm'.\n");
    }
}

static void cmd_payment_confirm(void) {
    char invoice_id[MAX_INVOICE_ID];
    printf("\n--- Confirm Payment ---\n");

    do {
        read_line("  Invoice ID: ", invoice_id, sizeof(invoice_id));
        if (!validate_invoice_id(invoice_id)) {
            printf("  [!] Invalid invoice ID.\n");
            invoice_id[0] = '\0';
        }
    } while (!validate_invoice_id(invoice_id) || invoice_id[0] == '\0');

    //Find the most recent unconfirmed PAYMENT_MADE in the chain
    int found = 0;
    for (int i = bc.length - 1; i >= 0 && !found; i--) {
        for (int j = bc.blocks[i].tx_count - 1; j >= 0 && !found; j--) {
            Transaction *t = &bc.blocks[i].transactions[j];
            if (t->type == TX_PAYMENT_MADE &&
                !t->confirmed &&
                strcmp(t->invoice_id, invoice_id) == 0) {

                t->confirmed = 1;
                found = 1;
                printf("  [OK] Payment for invoice %s confirmed (block %u).\n",
                       invoice_id, bc.blocks[i].block_id);

                //If balance == 0, automatically add settlement tx
                if (t->balance < 0.005) {
                    Transaction settle;
                    memset(&settle, 0, sizeof(settle));
                    settle.type       = TX_INVOICE_SETTLE;
                    settle.event_time = time(NULL);
                    settle.amount     = 0;
                    settle.balance    = 0;
                    settle.confirmed  = 1;
                    strncpy(settle.invoice_id, invoice_id, MAX_INVOICE_ID - 1);
                    strncpy(settle.student_id, t->student_id, MAX_STUDENT_ID - 1);
                    strncpy(settle.reference, "AUTO-SETTLE", MAX_REF - 1);
                    pool_add(&pool, &settle);
                    printf("  [*] Balance cleared — settlement event queued.\n");
                    printf("  [*] Run 'mine' to commit settlement.\n");
                }
            }
        }
    }

    //Also search the pending pool
    for (int i = 0; i < pool.count && !found; i++) {
        Transaction *t = &pool.txs[i];
        if (t->type == TX_PAYMENT_MADE &&
            !t->confirmed &&
            strcmp(t->invoice_id, invoice_id) == 0) {
            t->confirmed = 1;
            found = 1;
            printf("  [OK] Pending payment for invoice %s confirmed.\n",
                   invoice_id);
        }
    }

    if (!found)
        printf("  [!] No unconfirmed payment found for invoice %s.\n",
               invoice_id);

    save_all();
}

static void cmd_invoice_status(void) {
    char invoice_id[MAX_INVOICE_ID];
    printf("\n--- Invoice Status ---\n");

    do {
        read_line("  Invoice ID: ", invoice_id, sizeof(invoice_id));
        if (!validate_invoice_id(invoice_id))
            printf("  [!] Invalid invoice ID.\n");
    } while (!validate_invoice_id(invoice_id));

    if (!invoice_exists(&bc, &pool, invoice_id)) {
        printf("  [!] Invoice %s not found.\n", invoice_id);
        return;
    }

    printf("\n  ===== Invoice: %s =====\n", invoice_id);

    //Walk chain for all events on this invoice
    for (int i = 0; i < bc.length; i++) {
        for (int j = 0; j < bc.blocks[i].tx_count; j++) {
            Transaction *t = &bc.blocks[i].transactions[j];
            if (strcmp(t->invoice_id, invoice_id) != 0) continue;
            char tbuf[32];
            struct tm *tm = localtime(&t->event_time);
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
            printf("  [Block %u] %-16s | Amount: %8.2f | Balance: %8.2f | %s | %s\n",
                   bc.blocks[i].block_id,
                   t->type == TX_INVOICE_CREATE  ? "INVOICE_CREATE"  :
                   t->type == TX_PAYMENT_MADE    ? "PAYMENT_MADE"    :
                   t->type == TX_PAYMENT_CONFIRM ? "PAYMENT_CONFIRM" :
                                                   "INVOICE_SETTLE",
                   t->amount, t->balance, tbuf,
                   t->confirmed ? "CONFIRMED" : "PENDING");
        }
    }
    /* Pool */
    for (int i = 0; i < pool.count; i++) {
        Transaction *t = &pool.txs[i];
        if (strcmp(t->invoice_id, invoice_id) != 0) continue;
        char tbuf[32];
        struct tm *tm = localtime(&t->event_time);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
        printf("  [PENDING]   %-16s | Amount: %8.2f | Balance: %8.2f | %s | UNCONFIRMED\n",
               t->type == TX_INVOICE_CREATE  ? "INVOICE_CREATE"  :
               t->type == TX_PAYMENT_MADE    ? "PAYMENT_MADE"    :
               t->type == TX_PAYMENT_CONFIRM ? "PAYMENT_CONFIRM" :
                                               "INVOICE_SETTLE",
               t->amount, t->balance, tbuf);
    }

    double bal = get_balance(&bc, &pool, invoice_id);
    printf("\n  Outstanding Balance : %.2f RWF\n", bal);
    printf("  Status              : %s\n",
           invoice_settled(&bc, invoice_id) ? "SETTLED" :
           (bal < 0.005 ? "CLEARED (mine to settle)" : "OUTSTANDING"));
}

static void cmd_mine(void) {
    printf("\n--- Mine Block ---\n");
    printf("  Pending transactions: %d\n", pool.count);
    if (pool.count == 0) {
        printf("  Nothing to mine.\n");
        return;
    }

    Block b;
    if (!pool_flush_to_block(&pool, &b, &bc)) return;

    if (!mine_block(&b, bc.difficulty)) {
        printf("  [!] Mining failed.\n");
        return;
    }

    if (blockchain_add_mined_block(&bc, &b)) {
        printf("  [OK] Block %u added to chain.\n", b.block_id);
        save_all();
    }
}

static void cmd_chain_view(void) {
    blockchain_print(&bc);
    if (pool.count > 0)
        printf("\n  [Pending pool: %d unconfirmed transaction(s)]\n",
               pool.count);
}

static void cmd_chain_verify(void) {
    blockchain_verify(&bc);
}

static void print_menu(void) {
    printf("   ALU Blockchain Fees System                 \n");
    printf("══════════════════════════════════════════════\n");
    printf("  1. invoice create  – Create a fee invoice   \n");
    printf("  2. payment record  – Record a payment       \n");
    printf("  3. payment confirm – Confirm a payment      \n");
    printf("  4. invoice status  – View invoice details   \n");
    printf("  5. mine            – Mine pending block     \n");
    printf("  6. chain view      – Display blockchain     \n");
    printf("  7. chain verify    – Verify integrity       \n");
    printf("  0. exit                                     \n");
    printf("  Pending txs: %d  |  Chain length: %d blocks\n",
           pool.count, bc.length);
}

//Entry  point 
int main(int argc, char *argv[]) {
    //Optional difficulty argument: ./alu_fees <difficulty>
    int difficulty = 2;
    if (argc == 2) {
        int d = atoi(argv[1]);
        if (d >= 1 && d <= 6) difficulty = d;
    }

    //Ensure data directory exists
    system("mkdir -p data");

    load_all(difficulty);

    printf("\nWelcome to the ALU Blockchain Fees System\n");
    printf("Chain loaded: %d block(s), difficulty=%d\n",
           bc.length, bc.difficulty);

    char choice[64];
    while (1) {
        print_menu();
        read_line("\nEnter choice: ", choice, sizeof(choice));

        if (strcmp(choice, "1") == 0 || strcmp(choice, "invoice create") == 0)
            cmd_invoice_create();
        else if (strcmp(choice, "2") == 0 || strcmp(choice, "payment record") == 0)
            cmd_payment_record();
        else if (strcmp(choice, "3") == 0 || strcmp(choice, "payment confirm") == 0)
            cmd_payment_confirm();
        else if (strcmp(choice, "4") == 0 || strcmp(choice, "invoice status") == 0)
            cmd_invoice_status();
        else if (strcmp(choice, "5") == 0 || strcmp(choice, "mine") == 0)
            cmd_mine();
        else if (strcmp(choice, "6") == 0 || strcmp(choice, "chain view") == 0)
            cmd_chain_view();
        else if (strcmp(choice, "7") == 0 || strcmp(choice, "chain verify") == 0)
            cmd_chain_verify();
        else if (strcmp(choice, "0") == 0 || strcmp(choice, "exit") == 0) {
            save_all();
            printf("Goodbye.\n");
            break;
        } else {
            printf("  [!] Unknown command. Enter a number 0-7.\n");
        }
    }
    return 0;
}