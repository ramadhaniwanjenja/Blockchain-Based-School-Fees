#include "blockchain.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// hash comptation funtion
void compute_block_hash(Block *b, char *out_hex) {
    char buf[8192];
    int  len = 0;

    len += snprintf(buf + len, sizeof(buf) - len,
                    "%u|%ld|%s|%llu",
                    b->block_id,
                    (long)b->timestamp,
                    b->prev_hash,
                    (unsigned long long)b->nonce);

    for (int i = 0; i < b->tx_count; i++) {
        Transaction *t = &b->transactions[i];
        len += snprintf(buf + len, sizeof(buf) - len,
                        "|TX%d:%s:%s:%.2f:%.2f:%s:%d",
                        t->type,
                        t->student_id,
                        t->invoice_id,
                        t->amount,
                        t->balance,
                        t->reference,
                        t->confirmed);
    }

    sha256_hex((const uint8_t *)buf, (size_t)len, out_hex);
}

// mining proof of wrk
int mine_block(Block *b, int difficulty) {
    char hash[HASH_HEX_LEN];
    char prefix[64];

    if (difficulty < 1 || difficulty > 8) difficulty = 2;
    memset(prefix, '0', difficulty);
    prefix[difficulty] = '\0';

    b->nonce = 0;
    printf("Mining block %u (difficulty=%d) ", b->block_id, difficulty);
    fflush(stdout);

    while (1) {
        compute_block_hash(b, hash);
        if (strncmp(hash, prefix, difficulty) == 0) {
            memcpy(b->hash, hash, HASH_HEX_LEN);
            printf(" done!\n");
            printf("  Hash: %s  Nonce: %llu\n",
                   b->hash, (unsigned long long)b->nonce);
            return 1;
        }
        b->nonce++;
        if (b->nonce % 100000 == 0) { printf("."); fflush(stdout); }
    }
}

//bloack chain lifecycle

void blockchain_init(Blockchain *bc, int difficulty) {
    memset(bc, 0, sizeof(*bc));
    bc->length     = 0;
    bc->difficulty = (difficulty >= 1 && difficulty <= 8) ? difficulty : 2;

    //genesis block created here
    Block genesis;
    memset(&genesis, 0, sizeof(genesis));
    genesis.block_id  = 0;
    genesis.timestamp = time(NULL);
    memset(genesis.prev_hash, '0', 64);
    genesis.prev_hash[64] = '\0';
    genesis.tx_count  = 0;

    mine_block(&genesis, bc->difficulty);
    bc->blocks[0] = genesis;
    bc->length    = 1;
}

int blockchain_add_mined_block(Blockchain *bc, Block *b) {
    if (bc->length >= MAX_BLOCKS) {
        fprintf(stderr, "Error: blockchain full.\n");
        return 0;
    }

    //validate previous hash for linkage
    Block *prev = &bc->blocks[bc->length - 1];
    if (strcmp(b->prev_hash, prev->hash) != 0) {
        fprintf(stderr, "Error: prev_hash mismatch.\n");
        return 0;
    }

    //veriy proof of work
    char prefix[64];
    memset(prefix, '0', bc->difficulty);
    prefix[bc->difficulty] = '\0';
    if (strncmp(b->hash, prefix, bc->difficulty) != 0) {
        fprintf(stderr, "Error: block does not satisfy PoW.\n");
        return 0;
    }

    //veryfying hash corrects
    char expected[HASH_HEX_LEN];
    compute_block_hash(b, expected);
    if (strcmp(b->hash, expected) != 0) {
        fprintf(stderr, "Error: block hash is incorrect.\n");
        return 0;
    }

    bc->blocks[bc->length] = *b;
    bc->length++;
    return 1;
}

int blockchain_verify(const Blockchain *bc) {
    char prefix[64];
    memset(prefix, '0', bc->difficulty);
    prefix[bc->difficulty] = '\0';

    printf("\n Blockchain Integrity Verification \n");
    printf("Difficulty : %d\n", bc->difficulty);
    printf("Blocks     : %d\n\n", bc->length);

    int ok = 1;
    for (int i = 0; i < bc->length; i++) {
        const Block *b = &bc->blocks[i];
        char computed[HASH_HEX_LEN];
      //cast away const only for compute; function doesn't modify block
        compute_block_hash((Block *)b, computed);

        int hash_ok    = (strcmp(b->hash, computed) == 0);
        int pow_ok     = (strncmp(b->hash, prefix, bc->difficulty) == 0);
        int link_ok    = (i == 0)
                         ? 1
                         : (strcmp(b->prev_hash,
                                   bc->blocks[i-1].hash) == 0);

        printf("Block %u : hash=%s pow=%s link=%s\n",
               b->block_id,
               hash_ok ? "OK" : "FAIL",
               pow_ok  ? "OK" : "FAIL",
               link_ok ? "OK" : "FAIL");

        if (!hash_ok || !pow_ok || !link_ok) ok = 0;
    }
    printf("\nVerification result: %s\n", ok ? "VALID" : "INVALID");
    return ok;
}

static const char *tx_type_str(TxType t) {
    switch (t) {
        case TX_INVOICE_CREATE:  return "INVOICE_CREATE";
        case TX_PAYMENT_MADE:    return "PAYMENT_MADE";
        case TX_PAYMENT_CONFIRM: return "PAYMENT_CONFIRM";
        case TX_INVOICE_SETTLE:  return "INVOICE_SETTLE";
        default:                 return "UNKNOWN";
    }
}

void blockchain_print(const Blockchain *bc) {
    printf("\n BLOCKCHAIN LEDGER (%d blocks) \n",
           bc->length);
    for (int i = 0; i < bc->length; i++) {
        const Block *b = &bc->blocks[i];
        char tbuf[32];
        struct tm *tm = localtime(&b->timestamp);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);

        printf("\n+-- Block %u ----------------------------------\n",
               b->block_id);
        printf("|  Time      : %s\n", tbuf);
        printf("|  Prev Hash : %.20s...\n", b->prev_hash);
        printf("|  Hash      : %.20s...\n", b->hash);
        printf("|  Nonce     : %llu\n", (unsigned long long)b->nonce);
        printf("|  TX count  : %d\n", b->tx_count);

        for (int j = 0; j < b->tx_count; j++) {
            const Transaction *t = &b->transactions[j];
            char et[32];
            struct tm *etm = localtime(&t->event_time);
            strftime(et, sizeof(et), "%Y-%m-%d %H:%M:%S", etm);
            printf("|   [TX %d] Type      : %s\n", j, tx_type_str(t->type));
            printf("|          Student   : %s\n", t->student_id);
            printf("|          Invoice   : %s\n", t->invoice_id);
            printf("|          Amount    : %.2f RWF\n", t->amount);
            printf("|          Balance   : %.2f RWF\n", t->balance);
            printf("|          Reference : %s\n", t->reference);
            printf("|          Confirmed : %s\n",
                   t->confirmed ? "YES" : "NO");
            printf("|          Time      : %s\n", et);
        }
        printf("+----------------------------------------------\n");
    }
}



int blockchain_save(const Blockchain *bc, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror("blockchain_save"); return 0; }

    //writes dificulty level plus it's length
    fwrite(&bc->difficulty, sizeof(int),      1, f);
    fwrite(&bc->length,     sizeof(int),      1, f);
    fwrite(bc->blocks,      sizeof(Block), bc->length, f);
    fclose(f);
    return 1;
}

int blockchain_load(Blockchain *bc, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;   //file not fount

    memset(bc, 0, sizeof(*bc));
    fread(&bc->difficulty, sizeof(int), 1, f);
    fread(&bc->length,     sizeof(int), 1, f);
    if (bc->length > MAX_BLOCKS) bc->length = MAX_BLOCKS;
    fread(bc->blocks, sizeof(Block), bc->length, f);
    fclose(f);
    return 1;
}

//pending pool

void pool_init(TxPool *p) {
    memset(p, 0, sizeof(*p));
}

int pool_add(TxPool *p, Transaction *tx) {
    if (p->count >= MAX_PENDING) {
        fprintf(stderr, "Error: pending pool full.\n");
        return 0;
    }
    p->txs[p->count++] = *tx;
    return 1;
}


int pool_flush_to_block(TxPool *p, Block *b, const Blockchain *bc) {
    if (p->count == 0) {
        printf("No pending transactions to mine.\n");
        return 0;
    }

    memset(b, 0, sizeof(*b));
    b->block_id  = (uint32_t)bc->length;
    b->timestamp = time(NULL);
    memcpy(b->prev_hash, bc->blocks[bc->length - 1].hash, HASH_HEX_LEN);

    int take = p->count < MAX_TRANSACTIONS ? p->count : MAX_TRANSACTIONS;
    for (int i = 0; i < take; i++)
        b->transactions[i] = p->txs[i];
    b->tx_count = take;

 
    int remaining = p->count - take;
    for (int i = 0; i < remaining; i++)
        p->txs[i] = p->txs[i + take];
    p->count = remaining;
    return 1;
}

//invoice helpers

int invoice_exists(const Blockchain *bc, const TxPool *p,
                   const char *invoice_id) {
    for (int i = 0; i < bc->length; i++)
        for (int j = 0; j < bc->blocks[i].tx_count; j++)
            if (bc->blocks[i].transactions[j].type == TX_INVOICE_CREATE &&
                strcmp(bc->blocks[i].transactions[j].invoice_id,
                       invoice_id) == 0)
                return 1;
    for (int i = 0; i < p->count; i++)
        if (p->txs[i].type == TX_INVOICE_CREATE &&
            strcmp(p->txs[i].invoice_id, invoice_id) == 0)
            return 1;
    return 0;
}

int invoice_settled(const Blockchain *bc, const char *invoice_id) {
    for (int i = 0; i < bc->length; i++)
        for (int j = 0; j < bc->blocks[i].tx_count; j++)
            if (bc->blocks[i].transactions[j].type == TX_INVOICE_SETTLE &&
                strcmp(bc->blocks[i].transactions[j].invoice_id,
                       invoice_id) == 0)
                return 1;
    return 0;
}


double get_balance(const Blockchain *bc, const TxPool *p,
                   const char *invoice_id) {
    double balance  = -1.0;
    int    found    = 0;

    /* Chain */
    for (int i = 0; i < bc->length; i++) {
        for (int j = 0; j < bc->blocks[i].tx_count; j++) {
            Transaction *t = (Transaction *)&bc->blocks[i].transactions[j];
            if (strcmp(t->invoice_id, invoice_id) != 0) continue;
            if (t->type == TX_INVOICE_CREATE) {
                balance = t->amount;
                found   = 1;
            } else if (t->type == TX_PAYMENT_MADE ||
                       t->type == TX_INVOICE_SETTLE) {
                balance = t->balance;
            }
        }
    }

  
    for (int i = 0; i < p->count; i++) {
        Transaction *t = &p->txs[i];
        if (strcmp(t->invoice_id, invoice_id) != 0) continue;
        if (t->type == TX_INVOICE_CREATE) {
            balance = t->amount;
            found   = 1;
        } else if (t->type == TX_PAYMENT_MADE ||
                   t->type == TX_INVOICE_SETTLE) {
            balance = t->balance;
        }
    }

    return found ? balance : -1.0;
}

//input validation

int validate_student_id(const char *s) {
    if (!s || strlen(s) < 3 || strlen(s) >= MAX_STUDENT_ID) return 0;
    for (const char *c = s; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_') return 0;
    return 1;
}

int validate_invoice_id(const char *s) {
    if (!s || strlen(s) < 3 || strlen(s) >= MAX_INVOICE_ID) return 0;
    for (const char *c = s; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_') return 0;
    return 1;
}

int validate_amount(double a) {
    return (a > 0.0 && a < 1e10);
}