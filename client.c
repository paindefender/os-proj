#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define BUFSIZE		4096

unsigned char state[256],key[]={"123"},stream[2048];

// Key Scheduling Algorithm 
// Input: state - the state used to generate the keystream
//        key - Key to use to initialize the state 
//        len - length of key in bytes  
void ksa(unsigned char state[], unsigned char key[], int len)
{
    int i,j=0,t; 
    
    for (i=0; i < 256; ++i)
        state[i] = i; 
    for (i=0; i < 256; ++i) {
        j = (j + state[i] + key[i % len]) % 256; 
        t = state[i]; 
        state[i] = state[j]; 
        state[j] = t; 
    }   
}

// Pseudo-Random Generator Algorithm 
// Input: state - the state used to generate the keystream 
//        out - Must be of at least "len" length
//        len - number of bytes to generate 
void prga(unsigned char state[], unsigned char out[], int len)
{  
    int i=0,j=0,x,t; 
    unsigned char key; 
    
    for (x=0; x < len; ++x)  {
        i = (i + 1) % 256; 
        j = (j + state[i]) % 256; 
        t = state[i]; 
        state[i] = state[j]; 
        state[j] = t; 
        out[x] = state[(state[i] + state[j]) % 256];
    }   
}  

char* encrypt(){
    
}

pthread_t thread;

int connectsock( char *host, char *service, char *protocol );

void *readT(void *arg){
    char buf[BUFSIZE];
    char crypt[BUFSIZE];
    int csock = (int) arg;
    int cc;
    while(1){
        if ( (cc = read( csock, buf, BUFSIZE )) <= 0 )
        {
            printf( "The server has gone.\n" );
            close(csock);
            break;
        }
        else
        {
            buf[cc] = '\0';
            //printf("raw buf: %s\n", buf);
            if(buf[3] == 'E'){ // BEGIN Very hacky HACK HACK should probably make it a function
                int numst = 4;
                if (buf[5] == '#'){
                    //printf("msge with tag!\n");
                    while (buf[++numst] != ' '); //Get space right before the number
                }
                printf("numst = %d\n", numst);
                int crypt_length = 0;
                int current_digit;
                while (buf[++numst] != '/'){
                    current_digit = buf[numst] - '0';
                    crypt_length = crypt_length * 10 + current_digit;
                }
                //printf("length is %d\n", crypt_length);
                int i;
                for (i = 1; i <= crypt_length; i++){
                    buf[numst+i] = buf[numst+i] ^ stream[i];
                }
            } // END OF HACK HACK
            printf( "%s\n", buf );
        }
    }
}

/*
**	Client
*/
int
main( int argc, char *argv[] )
{
    ksa(state,key,3);
    prga(state,stream,2048);
    
	char		buf[BUFSIZE];
    char        crypt[BUFSIZE];
	char		*service;		
	char		*host = "localhost";
	int		cc;
	int		csock;
	
	switch( argc ) 
	{
		case    2:
			service = argv[1];
			break;
		case    3:
			host = argv[1];
			service = argv[2];
			break;
		default:
			fprintf( stderr, "usage: chat [host] port\n" );
			exit(-1);
	}

	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}

	printf( "The server is ready, please start sending to the server.\n" );
	printf( "Type q or Q to quit.\n" );
	fflush( stdout );

    pthread_create(&thread, NULL, readT, (void *) csock);
    
	// 	Start the loop
	while ( fgets( buf, BUFSIZE, stdin ) != NULL )
	{
		// If user types 'q' or 'Q', end the connection
		if ( buf[0] == 'q' || buf[0] == 'Q' )
		{
			break;
		}
		
		if(buf[0] == 'K'){
            sscanf(buf, "K %s", key);
            ksa(state,key,strlen(key));
            prga(state,stream,2048);
            continue;
        } else if(buf[3] == 'E'){ // BEGIN Very hacky HACK HACK should probably make it a function
            int numst = 4;
            if (buf[5] == '#'){
                //printf("sending msge with tag!\n");
                while (buf[++numst] != ' '); //Get space right before the number
            }
            //printf("numst = %d\n", numst);
            int crypt_length = 0;
            int current_digit;
            while (buf[++numst] != '/'){
                current_digit = buf[numst] - '0';
                crypt_length = crypt_length * 10 + current_digit;
            }
            //printf("length is %d\n", crypt_length);
            int i;
            for (i = 1; i <= crypt_length; i++){
                buf[numst+i] = buf[numst+i] ^ stream[i];
            }
        } // END OF HACK HACK
        //printf("writing this: %s\n", buf);
		
		// Send to the server
		if ( write( csock, buf, strlen(buf) ) < 0 )
		{
			fprintf( stderr, "client write: %s\n", strerror(errno) );
			exit( -1 );
		}	
		// Read the echo and print it out to the screen
	}
	close( csock );

}


