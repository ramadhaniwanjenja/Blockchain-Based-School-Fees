# ALU Blockchain-Based School Fees Collection and Invoicing System

## Project Description

This project is a blockchain based school fees collection and invoicing payment system that i build as part of the Software Engineering Low-Level Specialization course at the African Leadership University (ALU). The main idea behind this project was to simulate how a real university can manage student fee invoices, payments and confirmations in a secure and transparent way using blockchain technology.

Instead of using a normal database that can be edited or tampered with, this system use a blockchain where every financial record is stored in a block that is linked to the previous one using cryptographic hashes. Once a record is added to the chain, it cannot be changed without breaking the entire chain, which is what make this approach very powerful for institutional finance.

The system is fully written in C programming language and include a SHA-256 hashing algorithm that i implemented from scratch without using any external crypto library. It also implement a Proof of Work (PoW) mining mechanism that require blocks to have a configurable number of leading zeros in their hash before they can be added to the chain.

Everything runs through a command-line interface (CLI) where a finance officer or administrator can create invoices, record payments, confirm payments and view the full blockchain ledger at any time.

---

## How to Compile and Run the System

### Requirements

- GCC compiler (version 7 or above)
- Linux or macOS terminal (or WSL on Windows)
- Make utility

### Steps to Compile

First, clone or download the project folder and navigate into it:

```bash
cd blockchain_fees
```

Then compile using the Makefile:

```bash
make
```

This will produce an executable file called `alu_fees` in the project root directory. If you want to clean and recompile from scratch:

```bash
make clean
make
```

### Steps to Run

Run the system with an optional difficulty argument (1 to 6). Higher difficulty means more leading zeros required and longer mining time:

```bash
./alu_fees 3
```

If you dont pass a difficulty it will default to 2:

```bash
./alu_fees
```

### Using the CLI

When you run the program you will see a menu like this:

```
╔══════════════════════════════════════════════╗
║   ALU Blockchain Fees System                 ║
╠══════════════════════════════════════════════╣
║  1. invoice create  – Create a fee invoice   ║
║  2. payment record  – Record a payment       ║
║  3. payment confirm – Confirm a payment      ║
║  4. invoice status  – View invoice details   ║
║  5. mine            – Mine pending block     ║
║  6. chain view      – Display blockchain     ║
║  7. chain verify    – Verify integrity       ║
║  0. exit                                     ║
╚══════════════════════════════════════════════╝
```

You can type the number (1-7) or the full command name like `invoice create` or `chain verify`. The typical workflow is:

1. Type `1` to create a new invoice for a student
2. Type `5` to mine the pending transaction into a block
3. Type `2` to record a payment against that invoice
4. Type `5` again to mine the payment block
5. Type `3` to confirm the payment
6. Type `4` to check the invoice status and balance
7. Type `6` to see the full blockchain
8. Type `7` to verify the chain integrity

### Data Persistence

The system automatically saves the blockchain to `data/chain.bin` and the pending transaction pool to `data/pending.bin`. This means if you close the program and run it again, all your data will be there. You dont need to do anything special to enable this, it happen automatically.

---

## System Limitations and Known Bugs

- **No networking** – This system is a local simulation only. It does not connect to any network or other nodes. It is single machine only.

- **No smart contracts** – The business logic is hardcoded in C. There is no scripting or smart contract layer.

- **Binary file storage** – The blockchain is saved in a binary format which is not human readable. If the binary file become corrupted (for example by editing it manually) the chain may fail to load properly.

- **Difficulty is fixed at start** – Once a chain is created with a certain difficulty, changing the difficulty argument when you rerun the program will NOT change the difficulty of the existing chain. Only a fresh chain (after `make clean`) will use the new difficulty.

- **Max 1024 blocks** – The in-memory blockchain array holds up to 1024 blocks. For a production system this would need dynamic memory allocation.

- **Max 8 transactions per block** – Each block can hold a maximum of 8 transactions. If you have more than 8 pending, multiple mining rounds are needed.

- **No GUI** – The system is CLI only. There is no web interface or graphical dashboard.

- **Payment confirmation is manual** – After recording a payment, a finance officer must manually run `payment confirm`. In a real system this might be automated via bank callback or API.

---

## Contributor(s)

1. Name: Ramadhani Shafii Wanjenja 
2. ALU Student ID: 176632965

---

*Submitted as part of the Software Engineering – Low-Level Specialization (Introduction to Blockchain Development) course at the African Leadership University (ALU).*
