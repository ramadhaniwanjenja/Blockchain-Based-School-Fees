#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <time.h>
#include <stdint.h>

//size
#define HASH_HEX_LEN     65   
#define MAX_STUDENT_ID   32
#define MAX_INVOICE_ID   32
#define MAX_REF          64
#define MAX_TRANSACTIONS 8    /* transactions per block */
//transaction types
typedef enum {
    TX_INVOICE_CREATE  = 0,
    TX_PAYMENT_MADE    = 1,
    TX_PAYMENT_CONFIRM = 2,
    TX_INVOICE_SETTLE  = 3
} TxType;

//single transaction 
typedef struct {
    TxType  type;
    char    student_id[MAX_STUDENT_ID];
    char    invoice_id[MAX_INVOICE_ID];
    double  amount;            
    double  balance;           
    char    reference[MAX_REF];
    time_t  event_time;
    int     confirmed;        
} Transaction;

//block
typedef struct {
    uint32_t    block_id;
    time_t      timestamp;
    char        prev_hash[HASH_HEX_LEN];
    char        hash[HASH_HEX_LEN];
    uint64_t    nonce;
    int         tx_count;
    Transaction transactions[MAX_TRANSACTIONS];
} Block;

//blockchain in memory
#define MAX_BLOCKS 1024

typedef struct {
    Block    blocks[MAX_BLOCKS];
    int      length;
    int      difficulty;   
} Blockchain;

//pending transaction pool
#define MAX_PENDING 64

typedef struct {
    Transaction txs[MAX_PENDING];
    int         count;
} TxPool;

//function prototypes

void compute_block_hash(Block *b, char *out_hex);

//chain
void    blockchain_init(Blockchain *bc, int difficulty);
int     blockchain_add_mined_block(Blockchain *bc, Block *b);
int     blockchain_verify(const Blockchain *bc);
void    blockchain_print(const Blockchain *bc);

//mining
int     mine_block(Block *b, int difficulty);

//persistence
int     blockchain_save(const Blockchain *bc, const char *path);
int     blockchain_load(Blockchain *bc, const char *path);

//pool
void    pool_init(TxPool *p);
int     pool_add(TxPool *p, Transaction *tx);
int     pool_flush_to_block(TxPool *p, Block *b, const Blockchain *bc);

//invoice helpers
double  get_balance(const Blockchain *bc, const TxPool *p,
                    const char *invoice_id);
int     invoice_exists(const Blockchain *bc, const TxPool *p,
                       const char *invoice_id);
int     invoice_settled(const Blockchain *bc, const char *invoice_id);

//validation
int  validate_student_id(const char *s);
int  validate_invoice_id(const char *s);
int  validate_amount(double a);

#endif