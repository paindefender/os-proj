#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>

#define	QLEN			5
#define	BUFSIZE			4096
#define DEBUG_INFO 1

int passivesock( char *service, char *protocol, int qlen, int *rport );

// <Main Project Stuff>

// --Data Structures and Methods--

// REGISTERALL linked list
typedef struct RA_node_s {
    int descriptor;
    struct RA_node_s *next;
} RA_node;

RA_node *RA_head = NULL;

int RA_isRegistered(int descriptor){
    RA_node *current = RA_head;
    
    if (current == NULL){
        return 0;
    }
    
    while (current->descriptor != descriptor){
        if (current->next == NULL){
            return 0;
        }
        current = current->next;
    }
    return 1;
}

void RA_register(int descriptor){
    if (RA_isRegistered(descriptor)) return;
    RA_node *new = (RA_node*) malloc( sizeof(RA_node) );
    new->descriptor = descriptor;
    new->next = RA_head;
    RA_head = new;
    if (DEBUG_INFO){
        printf("RA_register: %d registered\n", RA_head->descriptor);
    }
}

void RA_deregister(int descriptor){
    RA_node *new = RA_head;
    
    if (new == NULL) return;
    
    if (new->descriptor == descriptor){
        RA_head = new->next;
        free(new);
        return;
    }
    
    while (new != NULL){
        if (new->next == NULL){
            return;
        } else {
            
            if (new->next->descriptor == descriptor){
                new->next = new->next->next;
                free(new->next);
                return;
            }
            
            new = new->next;
        }
    }
}

/*void RA_print(){
    RA_node *new = RA_head;
    while (new != NULL){
        printf(".%d.\n", new->descriptor);
        if (new->next == NULL){
            return;
        } else {
            new = new->next;
        }
    }
}*/

void RA_sendAll(char *str, sem_t *writesem){
    RA_node *new = RA_head;
    while (new != NULL){
        sem_wait(&writesem[new->descriptor]);
        write( new->descriptor, str, strlen(str) );
        sem_post(&writesem[new->descriptor]);
        if (new->next == NULL){
            return;
        } else {
            new = new->next;
        }
    }
}

void RA_sendAllE(char *str, int length, sem_t *writesem){ //With the length provided, for encrypted strings where strlen yields unexpected results
    RA_node *new = RA_head;
    while (new != NULL){
        sem_wait(&writesem[new->descriptor]);
        write( new->descriptor, str, length );
        sem_post(&writesem[new->descriptor]);
        if (new->next == NULL){
            return;
        } else {
            new = new->next;
        }
    }
}

void RA_sendAllI(char *str, int length){ //Image version
    RA_node *new = RA_head;
    while (new != NULL){
        write( new->descriptor, str, length );
        
        if (new->next == NULL){
            return;
        } else {
            new = new->next;
        }
    }
}

// TAGS linked list
typedef struct TAGS_node_s{
    char *tag;
    int *descriptors;
    int descriptorSize;
    struct TAGS_node_s *next;
} TAGS_node;

TAGS_node *TAGS_head = NULL;

void TAGS_node_destroy(TAGS_node *node){
    free(node->tag);
    free(node->descriptors);
    free(node);
}

void TAGS_tag_remove(char *tag){
    TAGS_node *current = TAGS_head;
    
    if (strcasecmp(TAGS_head->tag, tag) == 0){
        current = TAGS_head->next;
        TAGS_node_destroy(TAGS_head);
        TAGS_head = current;
        if (DEBUG_INFO){
            printf("TAGS_tag_remove: removed tag (%s)\n", tag);
        }
        return;
    }
    
    while (current->next != NULL){
        
        int strc = strcasecmp(tag, current->next->tag);
        if (strc == 0){
            if (DEBUG_INFO){
                printf("TAGS_tag_remove: found tag!\n");
            }
            
            TAGS_node *tmp = current->next;
            current->next = current->next->next;
            TAGS_node_destroy(tmp);
            
            if (DEBUG_INFO){
                printf("TAGS_tag_remove: removed tag (%s)\n", tag);
            }
            return;
        }
        
        current = current->next;
    }
}

TAGS_node *TAGS_find(char *tag){ //Returns null if found nothing
    TAGS_node *current = TAGS_head;
    while (current != NULL){
        
        int strc = strcasecmp(tag, current->tag);
        if (strc == 0){
            if (DEBUG_INFO){
                printf("TAGS_find: found tag!\n");
            }
            return current;
        }
        
        if (current->next == NULL){
            return NULL;
        } else {
            current = current->next;
        }
    }
}

void TAGS_fd_register(char *tag, int descriptor){
    if (TAGS_head == NULL){
        TAGS_node *new = (TAGS_node*) malloc( sizeof(TAGS_node) );
        new->tag = (char*) malloc( sizeof(char) * strlen(tag) );
        strcpy(new->tag, tag);
        
        new->descriptorSize = 1;
        new->descriptors = (int*) malloc( sizeof(int) );
        new->descriptors[0] = descriptor;
        
        TAGS_head = new;
        if (DEBUG_INFO){
            printf("TAGS_fd_register: added new tag %s\n", tag);
            printf("TAGS_fd_register: registered %d\n", descriptor);
        }
        return;
    }
    
    TAGS_node *node = TAGS_find(tag);
    
    if (node == NULL){
        TAGS_node *new = (TAGS_node*) malloc( sizeof(TAGS_node) );
        new->tag = (char*) malloc( sizeof(char) * strlen(tag) );
        strcpy(new->tag, tag);
        
        new->descriptorSize = 1;
        new->descriptors = (int*) malloc( sizeof(int) );
        new->descriptors[0] = descriptor;
        
        new->next = TAGS_head;
        TAGS_head = new;
        if (DEBUG_INFO){
            printf("TAGS_fd_register: added new tag %s\n", tag);
            printf("TAGS_fd_register: registered %d\n", descriptor);
        }
        return;
    }
    
    int *fds = node->descriptors;
    
    int i;
    for (i = 0; i < node->descriptorSize; i++){
        if (fds[i] == descriptor){ // Already registered here
            return;
        }
    }
    
    node->descriptorSize++;
    if (DEBUG_INFO){
        printf("trying to realloc\n");
        printf("nfds is %d\n", node->descriptorSize);
    }
    fds = (int*) realloc( fds, node->descriptorSize * sizeof(int) );
    fds[node->descriptorSize-1] = descriptor;
    if (DEBUG_INFO){
        printf("TAGS_fd_register: found existing tag %s\n", tag);
        printf("TAGS_fd_register: registered %d\n", descriptor);
    }
    return;
}

void TAGS_fd_deregister(char *tag, int descriptor){
    TAGS_node *node = TAGS_find(tag);
    
    if (node == NULL){
        if (DEBUG_INFO){
            printf("TAGS_fd_deregister: tag (%s) is not found.\n", tag);
        }
        return;
    }
    
    int i;
    for (i = 0; i < node->descriptorSize; i++){
        if (node->descriptors[i] == descriptor){
            node->descriptorSize--;
            if (DEBUG_INFO){
                printf("TAGS_fd_deregister: descriptor size is %d.\n", node->descriptorSize);
            }
            int j;
            for (j = i; j < node->descriptorSize; j++){ // Shift array elements
                node->descriptors[j] = node->descriptors[j+1];
            }
            if (DEBUG_INFO){
                printf("TAGS_fd_deregister: Current array: [");
                int sf;
                for (sf = 0; sf<node->descriptorSize; sf++){
                    printf("%d ", node->descriptors[sf]);
                }
                printf("]\n");
            }
            node->descriptors = (int*) realloc(node->descriptors, node->descriptorSize * sizeof(int));
            if (node->descriptorSize == 0){ // Remove the tag
                TAGS_tag_remove(tag);
            }
            if (DEBUG_INFO){
                printf("TAGS_fd_deregister: deregistered\n");
            }
            return;
        }
    }
    if (DEBUG_INFO){
        printf("TAGS_fd_deregister: descriptor (%d) is not registered in tag (%s).\n", descriptor, tag);
    }
}

void TAGS_sendTagged(char *tag, char *msg, sem_t *writesem){
    TAGS_node *node = TAGS_find(tag);
    
    if (node == NULL){
        if (DEBUG_INFO){
            printf("TAGS_sendTagged: tag (%s) not found\n", tag);
        }
        return;
    }
    
    int i;
    for (i = 0; i < node->descriptorSize; i++){
        sem_post(&writesem[node->descriptors[i]]);
        write(node->descriptors[i], msg, strlen(msg));
        sem_wait(&writesem[node->descriptors[i]]);
    }
    return;
}

void TAGS_sendTaggedE(char *tag, char *msg, int length, sem_t *writesem){ //With the length provided, for encrypted strings where strlen yields unexpected results
    TAGS_node *node = TAGS_find(tag);
    
    if (node == NULL){
        if (DEBUG_INFO){
            printf("TAGS_sendTagged: tag (%s) not found\n", tag);
        }
        return;
    }
    
    int i;
    for (i = 0; i < node->descriptorSize; i++){
        sem_post(&writesem[node->descriptors[i]]);
        write(node->descriptors[i], msg, length);
        sem_wait(&writesem[node->descriptors[i]]);
    }
    return;
}

void TAGS_sendTaggedI(char *tag, char *msg, int length){ // Image version
    TAGS_node *node = TAGS_find(tag);
    
    if (node == NULL){
        if (DEBUG_INFO){
            printf("TAGS_sendTagged: tag (%s) not found\n", tag);
        }
        return;
    }
    
    int i;
    for (i = 0; i < node->descriptorSize; i++){
        write(node->descriptors[i], msg, length);
    }
    return;
}
// --Functions--

/* DETERMINE THE MESSAGE TYPE
// -1 - illegal command
// 0 - REGISTERALL
// 1 - DEREGISTERALL
// 2 - REGISTER tag
// 3 - DEREGISTER tag
// 4 - MSG
*/ 
int optype(char *str){
    char *teststr = (char*) malloc( sizeof(char) * strlen(str) );
    sscanf(str, "%s", teststr); // Read first token
    if (DEBUG_INFO){
        printf("optype: got %s\n", teststr);
    }
    
    char *commands[] = {
        "REGISTERALL", "DEREGISTERALL",
        "REGISTER", "DEREGISTER",
        "MSG", "MSGE", "IMAGE"
    }; // All known commands
    int commands_number = 7;
    
    int i;
    for (i = 0; i < commands_number; i++){
        int strc = strcmp(commands[i], teststr);
        if (strc == 0){
            free(teststr);
            
            if (DEBUG_INFO){
                printf("optype: legal command\n", commands[i]);
            }
            
            return i;
        }
    }
    
    free(teststr);
    
    if (DEBUG_INFO){
        printf("optype: illegal command\n", teststr);
    }
    
    return -1;
}

void op_deregisterall(int descriptor);

void op_registerall(int descriptor){
    op_deregisterall(descriptor); //HACK HACK HACK HACK
    RA_register(descriptor);
}

void op_deregisterall(int descriptor){
    RA_deregister(descriptor);
    
    //FIXME this is bad and should be fixed
    TAGS_node *current = TAGS_head;
    while (current != NULL){
        
        if(DEBUG_INFO){
            printf("op_deregisterall: TRYING TO FIND AND DELETE FROM %s\n", current->tag);
        }
        TAGS_fd_deregister(current->tag, descriptor); //FIXME FIXME FIXME FIXME TODO FIXME HACK HACK HACK HACK HACK
        
        if (current->next == NULL){
            return;
        } else {
            current = current->next;
        }
    }
}

void op_register(char *str, int descriptor){
    if (RA_isRegistered(descriptor)){
        if (DEBUG_INFO){
            printf("op_register: descriptor %d already registered for all channels\n", descriptor);
        }
        return;
    }
    char *tag = (char*) malloc( sizeof(char) * strlen(str) );
    tag[0] = '\0';
    sscanf(str, "REGISTER %s", tag);
    if (strlen(tag) != 0){
        TAGS_fd_register(tag, descriptor);
    }
    free(tag);
}

void op_deregister(char *str, int descriptor){
    if (RA_isRegistered(descriptor)){
        if (DEBUG_INFO){
            printf("op_deregister: descriptor %d is registered for all channels\nPlease use DEREGISTERALL\n", descriptor);
        }
        return;
    }
    char *tag = (char*) malloc( sizeof(char) * strlen(str) );
    tag[0] = '\0';
    sscanf(str, "DEREGISTER %s", tag);
    if (strlen(tag) != 0){
        if (DEBUG_INFO){
            printf("op_deregister: calling TAGS_fd_deregister with tag (%s) and fd (%d)\n", tag, descriptor);
        }
        TAGS_fd_deregister(tag, descriptor);
    }
    free(tag);
}

void op_msg(char *str, sem_t *writesem){
    RA_sendAll(str, writesem);
    char *tag = (char*) malloc( sizeof(char) * strlen(str) );
    tag[0] = '\0';
    sscanf(str, "MSG #%s", tag);
    if (strlen(tag) != 0){
        TAGS_sendTagged(tag, str, writesem);
    }
    free(tag);
}

void op_msge(char *str, int length, sem_t *writesem){
    RA_sendAllE(str, length, writesem);
    char *tag = (char*) malloc( sizeof(char) * length );
    tag[0] = '\0';
    sscanf(str, "MSGE #%s", tag); //Only difference with op_msg
    if (strlen(tag) != 0){
        TAGS_sendTaggedE(tag, str, length, writesem);
    }
    free(tag);
}

//Part 3 stuff

typedef struct image_info_struct{
    char *read;
    int descriptor;
    int length;
    fd_set *afds;
    fd_set *rfds;
    pthread_mutex_t *readmutexes;
    pthread_mutex_t *writemutexes;
    sem_t *readsems;
    sem_t *writesems;
} image_info;

typedef struct image_chunk_struct{
    char *bytes;
    int length;
} image_chunk;

void *image_thread(void *arg){ //FIXME THREAD THREAD FIXME
    image_info *info = (image_info*) arg;
    if (DEBUG_INFO)
        printf("READ IMAGE and %d bytes\n", info->length);
    
    char *tag = (char*) malloc( sizeof(char) * strlen(info->read) );
    tag[0] = '\0';
    sscanf(info->read, "IMAGE #%s", tag);
    if (DEBUG_INFO)
        printf("Tag is %s\n", tag);
    
    int numst = 5;
    if (info->read[6] == '#'){
        while (info->read[++numst] != ' '); //Get space right before the number
    }
    int image_length = 0;
    int current_digit;
    while (info->read[++numst] != '/'){
        current_digit = info->read[numst] - '0';
        image_length = image_length * 10 + current_digit;
    }
    if (DEBUG_INFO){
        printf("IMAGE length is %d\n", image_length);
        printf("NUMST is %d\n", numst);
    }
    
    int bytes_left = numst + image_length + 1 - info->length;
    if (DEBUG_INFO)
        printf("Bytes left: %d\n", bytes_left);
    
    image_chunk *image_chunks = (image_chunk*)malloc(sizeof(image_chunk));
    int chunk_number = 0;
    image_chunks[chunk_number].length = 0;
    image_chunks[chunk_number].bytes = (char *)malloc(BUFSIZE * sizeof(char));
    
    while (bytes_left > 0){
        int cc;
        if ( (cc = read( info->descriptor, image_chunks[chunk_number].bytes, ((BUFSIZE < bytes_left)?BUFSIZE:bytes_left) )) <= 0 )
        {
            printf( "The client has gone.\n" );
            (void) close(info->descriptor);
            free(tag);
            free(info);
            pthread_exit(0);
        }
        image_chunks[chunk_number].length = cc;
        bytes_left -= cc;
        chunk_number++;
        
        image_chunks = (image_chunk*)realloc(image_chunks, (chunk_number + 1) * sizeof(image_chunk));
        image_chunks[chunk_number].length = 0;
        image_chunks[chunk_number].bytes = (char *)malloc(BUFSIZE * sizeof(char));
        
        if(DEBUG_INFO)
            printf("Bytes left: %d\n", bytes_left);
    }
    sem_post(&info->readsems[info->descriptor]); // HACK Done reading, release semaphore
    if (DEBUG_INFO)
        printf("Got %d chunks\n", chunk_number);
    
    //HACK STARTING WRITING SEMAPHORE THE FD!!!
    sem_wait(&info->readsems[info->descriptor]);
    RA_sendAllI(info->read, info->length);
    if (strlen(tag) != 0){
        TAGS_sendTaggedI(tag, info->read, info->length);
    }
    int i;
    for (i = 0; i < chunk_number; i++){
        if (DEBUG_INFO)
            printf("Sending chunk %d\n", i);
        RA_sendAllI(image_chunks[i].bytes, image_chunks[i].length);
        if (strlen(tag) != 0){
            TAGS_sendTaggedI(tag, image_chunks[i].bytes, image_chunks[i].length);
        }
    }
    //HACK STOPPED WRITING POST THE SEMAPHORE
    sem_post(&info->readsems[info->descriptor]);
    free(tag);
    free(info);
}


// <Main Project Stuff/>

/*
**	The server ... 
*/
int
main( int argc, char *argv[] )
{
	char			buf[BUFSIZE];
	char			*service;
	struct sockaddr_in	fsin;
	int			msock;
	int			ssock;
	fd_set			rfds;
	fd_set			afds;
	int			alen;
	int			fd;
	int			nfds;
	int			rport = 0;
	int			cc;
	
	switch (argc) 
	{
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			// User provides a port? then use it
			service = argv[1];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}

	
	nfds = getdtablesize();
    
    pthread_t thread; //One is enough???
    pthread_mutex_t readmutex[nfds]; //Mutexes and semaphores for each descriptor
    pthread_mutex_t writemutex[nfds];
    sem_t readsem[nfds];
    sem_t writesem[nfds];
    for (int i = 0; i < nfds; i++){ //Initialize them
        pthread_mutex_init(&readmutex[i], NULL);
        pthread_mutex_init(&writemutex[i], NULL);
        sem_init(&readsem[i], 0, 1);
        sem_init(&writesem[i], 0, 1);
    }

	FD_ZERO(&afds);
	FD_SET( msock, &afds );
    
    if (DEBUG_INFO){ // You can turn it off on line 14
        printf("DEBUG INFO IS ON\n");
    }
    
	for (;;)
	{
		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));
        printf("Copied thing\n");

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0)
		{
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}

		/*	Handle the main socket - a new guy has checked in  */
		if (FD_ISSET( msock, &rfds)) 
		{
			int	ssock;

			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0)
			{
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}

			/* start listening to this guy */
			FD_SET( ssock, &afds );
		}

		/*	Handle the participants requests  */
		for ( fd = 0; fd < nfds; fd++ )
		{
            //printf("In fd %d\n", fd);
			if (fd != msock && FD_ISSET(fd, &rfds))
			{
                sem_wait(&readsem[fd]);
				if ( (cc = read( fd, buf, BUFSIZE )) <= 0 )
				{
					printf( "The client has gone.\n" );
					(void) close(fd);
					FD_CLR( fd, &afds );

				}
				else
				{
					buf[cc] = '\0';
					printf( "The client(%d) says: %s\n", fd, buf );
                    
                    int operation = optype(buf);
                    
                    if (DEBUG_INFO){
                        printf("request length = %d\n", cc);
                        printf("debug: optype returned %d\n", operation);
                    }
                    
                    switch( operation ){
                        case 0:
                            op_registerall(fd);
                            break;
                        case 1:
                            op_deregisterall(fd);
                            break;
                        case 2:
                            op_register(buf, fd);
                            break;
                        case 3:
                            op_deregister(buf,fd);
                            break;
                        case 4:
                            op_msg(buf, writesem);
                            break;
                        case 5:
                            op_msge(buf, cc, writesem);
                            break;
                        case 6: ;
                            // We need:
                            // * Bytes already read
                            // * Their length
                            // * File descriptor
                            // * Read/write mutexes/semaphores
                            image_info *info = (image_info*) malloc(sizeof(image_info));
                            info->length = cc;
                            info->read = (char *)malloc(cc * sizeof(char));
                            memcpy(info->read, buf, cc);
                            info->descriptor = fd;
                            //FD_CLR( fd, &afds ); // Remove fd from multiplexer
                            info->readmutexes = readmutex;
                            info->writemutexes = writemutex;
                            info->readsems = readsem;
                            info->writesems = writesem;
                            info->afds = &afds;
                            info->rfds = &rfds;
                            pthread_create(&thread, NULL, image_thread, (void *) info);
                            sem_wait(&readsem[fd]);
                            break;
                        default:
                            break;
                    }
					//sprintf( buf, "OK\n" );
                    //write( fd, buf, strlen(buf) );
				}
				sem_post(&readsem[fd]);
			}

		}
	}
}
