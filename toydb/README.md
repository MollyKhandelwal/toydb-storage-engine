# Database Management Systems - toyDB Assignment

This repository contains the implementation of the three stated objectives extending the ToyDB database system. The implementations build upon the Paged File (PF) layer and the Access Method (AM) layer.

## Overview of Objectives

### 1. Page Buffering (PF Layer)
The buffer manager in `pflayer/buf.c` and `pflayer/pf.c` has been extended to support configurable buffer sizes and two page replacement strategies: **Least Recently Used (LRU)** and **Most Recently Used (MRU)**.
- **Statistics**: Global variables track logical reads/writes and physical reads/writes.
- **Testing**: `test_obj1.c` simulates read and write queries, generating data in `obj1_stats.csv`. A Python script `plot_obj1.py` can be used to generate a graph visualizing the performance differences.

### 2. Slotted-Page Structure (AM/PF Layer extension)
A Slotted-Page structure was built on top of the PF layer (`slotted_page.h` and `slotted_page.c`) to accommodate variable-length records (e.g., records from `student.txt`).
- **Storage**: The page header is stored at the very end of the page, growing downwards. Records grow upwards from the start of the page.
- **Operations**: Includes functions to insert, delete, and read records efficiently without wasting space.
- **Testing**: `test_obj2.c` loads `student.txt` into slotted pages and compares the space utilization against static record management using 50, 100, and 150 bytes maximum lengths.

### 3. Index Construction (AM Layer)
Three approaches for index construction were evaluated:
- **Approach A**: Creating the index in a single operation from an existing populated data file.
- **Approach B**: Incremental index building where data records are inserted and index entries are added concurrently.
- **Approach C**: A highly efficient **Bulk-Loading** technique (`amlayer/bulkload.c`) that directly allocates and links leaf nodes horizontally and builds internal parent nodes, circumventing top-down search traversals.
- **Testing**: `test_obj3.c` builds the three indexes using the respective approaches and outputs the logical and physical I/O statistics to showcase the superiority of bottom-up bulk loading.

## Compilation and Execution Instructions

Since this codebase relies heavily on standard Unix system calls (`unistd.h`, `fcntl.h`), it is recommended to compile and run it in a Unix/Linux environment or using WSL/MinGW on Windows.

### Compiling the Code

We provide a simple manual compilation method using `gcc`. From the `toydb` root directory, execute the following commands to build the object files for the layers and the test executables:

```bash
# 1. Compile PF Layer
gcc -c pflayer/pf.c pflayer/buf.c pflayer/hash.c
ar rcs libpf.a pf.o buf.o hash.o

# 2. Compile AM Layer and Slotted Page Layer
gcc -c amlayer/am.c amlayer/aminsert.c amlayer/amsearch.c amlayer/amfns.c amlayer/amstack.c amlayer/bulkload.c slotted_page.c
ar rcs libam.a am.o aminsert.o amsearch.o amfns.o amstack.o bulkload.o slotted_page.o

# 3. Compile Test Executables
gcc test_obj1.c -L. -lpf -o test_obj1
gcc test_obj2.c -L. -lpf -lam -o test_obj2
gcc test_obj3.c -L. -lpf -lam -o test_obj3
```

*(Note: Depending on your exact GCC version, you might need to append `-g -Wall -Wno-implicit-function-declaration` or similar flags. The libraries `-lpf` and `-lam` are created locally above.)*

### Running the Tests

1. **Objective 1 (Page Buffering)**
   ```bash
   ./test_obj1
   python3 plot_obj1.py
   ```
   *Expectation*: Generates `obj1_stats.csv` and outputs `obj1_plot.png` which visually charts the physical I/O metrics between LRU and MRU.

2. **Objective 2 (Slotted Pages)**
   ```bash
   ./test_obj2
   ```
   *Expectation*: Outputs a table to the console detailing the total number of pages used and the percentage space utilization for Slotted Pages versus Static Management (at max lengths 50, 100, and 150).

3. **Objective 3 (Index Construction)**
   ```bash
   ./test_obj3
   ```
   *Expectation*: Prints the logical and physical read/write counts for all three approaches. You should observe that Approach C (Bulk Load) minimizes logical page accesses and eliminates top-down traversal overheads.
