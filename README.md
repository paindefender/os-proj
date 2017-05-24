# os-proj
Server for the chat with ability to send encrypted messages and images. OS final project and concurrency exercise.

## Basic overview of the server
Code is based on sample lab multiplexing client/server. After the server reads a `buf`, `optype(buf)`
returns the index of operation, or `-1` if operation is invalid. Two data structures `RA` and `TAGS` are
used, for users registered using `REGISTER TAG` and `REGISTERALL` respectively. Each writing and
reading operation uses semaphores to avoid any concurrency problems.

## Data structures
### `RA` - REGISTERALL Users linked list
Each node in this linked list has only one field - file descriptor. It has six methods:
* `int RA_isRegistered(int descriptor)` - `1` if descriptor is in linked list, `0` otherwise.
* `void RA_register(int descriptor)` - add descriptor to linked list
* `void RA_deregister(int descriptor)` - remove descriptor from linked list
* `void RA_sendAll(char *str, sem_t *writesem)` - write string `str` to all descriptors in linked list, `writesem` semaphore array is used to check if any descriptors are already being written to.
* `void RA_sendAllE(char *str, int length, sem_t *writesem)` - same as above, but with explicitly specified length, added in Part 2, to avoid any errors with `MSGE`
* `void RA_sendAllI(char *str, int length)` - same as above, but concurrency is handled separately.
### TAGS - TAG Users linked list
Each node in this linked list has three fields, tag string, array of descriptors and descriptorSize - length of the array. It has eight methods:
* `void TAGS_node_destroy(TAGS_node *node)` - deallocates all the memory for the node.
* `void TAGS_tag_remove(char *tag)` - removes the tag from the linked list. Happens when number of users of this tag is zero.
* `TAGS_node *TAGS_find(char *tag)` - returns the node that corresponds to the tag, or `NULL`.
* `void TAGS_fd_register(char *tag, int descriptor)` - Registers a `descriptor` to a `tag`.
* `void TAGS_fd_deregister(char *tag, int descriptor)` - Deregisters a `descriptor` from a `tag`.
* `void TAGS_sendTagged(char *tag, char *msg, sem_t *writesem)` - writes `msg` to all descriptors registered on `tag`. Uses semaphore array `writesem` to check if any descriptors are already being written to.
* `void TAGS_sendTaggedE(char *tag, char *msg, int length, sem_t *writesem)` - same as above, but with explicitly specified length.
* `void TAGS_sendTaggedI(char *tag, char *msg, int length)` - same as above, but concurrency is handled separately.
## Concurrency model
### Reading
Images are handled using separate threads for each image. Images are read to an array, where each entry is of size `BUFSIZE`. To make sure that whole image is read, `sem_wait()` is called in the main thread, blocking it and preventing from reading data from file descriptors. After whole image was read to the array, `sem_post()` is called in the image reading thread, and allows the main thread to continue it’s job.
### Writing
Each function that writes to the descriptors uses a writesem semaphore array to make sure that there are no concurrency problems. The only functions that do not use it are `RA_sendAllI()` and `TAGS_sendTaggedI()`, they are used to write chunks of an image. `sem_wait()` and `sem_post()` are called outside of a loop in which `RA_sendAllI()` and `TAGS_sendTaggedI()` are used to write image data chunk at a time. All of this guarantees that write operations will not interfere with each other.
## Simple client
Simple client is a command line tool and does not support sending/receiving images.
### Threads
Reading and writing are done in two separate threads.
### Setting a keyword
Use `K` command to set the keyword. Default key is 123
