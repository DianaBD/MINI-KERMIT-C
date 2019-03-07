#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define receive_init 1
#define receive_file 2
#define receive_data 3
#define received_EOT 4

#define HOST "127.0.0.1"
#define PORT 10001

int check_crc(msg r){
    unsigned short newcrc,oldcrc;
    char bufcrc[2];

    memcpy(bufcrc,r.payload + r.len - 3,2);
    oldcrc = (short)( ((unsigned char)(bufcrc[1])) << 8 | ((unsigned char)(bufcrc[0]) ));
    newcrc = crc16_ccitt(r.payload, r.len - 3);

    if(newcrc == oldcrc)
        return 1;
    return 0;
}

int main(int argc, char** argv) {
    setbuf(stdout, NULL);
    msg r,*y;

    init(HOST, PORT);

    int tries = 3;
    int state = receive_init;
    
    int seq = 0;
    int len;

    // astept mesaj send-init - astept pana primesc ceva, orice
    
    if (recv_message(&r) < 0) {
        perror("receive error");
    } else {
        tries = 3;
        printf("[keceiver] got: ");
        printf("SOH:%x LEN:%d SEQ:%d TYPE:%c MAXL:%d TIME:%d NAPD:%x PADC:%x EOL:%x CHECK1:%x CHECK2:%x MARK:%x\n",r.payload[0],r.payload[1],r.payload[2],r.payload[3],(unsigned char)r.payload[4],r.payload[5],r.payload[6],r.payload[7],r.payload[8],r.payload[15],r.payload[16],r.payload[17]);
        // verific CHECK
        if(check_crc(r)) {
            /* am primit init bun => 
                - trimit inapoi ACK (de confirmare 'S')
                - trec in starea receive_file
                - increnentez seq*/
            printf("[keceiver] sending ACK to SEQ[%d]\n",r.payload[2]);
            state = receive_file;
            r.payload[2] = seq;
            r.payload[3] = 'Y';
            send_message(&r);
        }
        else{
            printf("[keceiver] sending NAK(corupt) to SEQ[%d]\n",r.payload[2]);
            r.payload[2] = seq;
            r.payload[3] = 'N';
            send_message(&r);
            
        }
        seq = (seq + 1)%64;
    }
    
    // cat timp mesajul send init nu e bun, astept un init bun
    while((state == receive_init) && (tries != 0)){
        y = receive_message_timeout(5000);
        if (y == NULL) {
            // sent previous NAK : 2 more tries
            
            printf("[keceiver] sending NAK(nimic) to last received SEQ[%d]\n",r.payload[2]);
            send_message(&r);
        } else {
            
            tries = 3;
            printf("[keceiver] Got reply with payload: ");
            printf("SOH:%x LEN:%d SEQ:%d TYPE:%c MAXL:%d TIME:%d NAPD:%x PADC:%x EOL:%x CHECK1:%x CHECK2:%x MARK:%x\n",y->payload[0],y->payload[1],y->payload[2],y->payload[3],(unsigned char)y->payload[4],y->payload[5],y->payload[6],y->payload[7],y->payload[8],y->payload[15],y->payload[16],y->payload[17]);
            if(check_crc(*y)) {
            /* am primit init bun => 
                - trimit inapoi ACK (de confirmare 'S')
                - trec in starea receive_file
                - incrementez seq*/
                printf("[keceiver] sending ACK to SEQ[%d]\n",y->payload[2]);
                state = receive_file;
                r.payload[3] = 'Y';
                r.payload[2] = seq;
                send_message(&r);
            }
            else{
                r.payload[3] = 'N';
                printf("[keceiver] sending NAK(corupt) to SEQ[%d]\n",y->payload[2]);
                r.payload[2] = seq;
                send_message(&r);
            }
            seq = (seq + 1)%64;
        }
    }

    
    FILE* f;
    //printf("R a primit init bun yass!! \n");

    while( (state != received_EOT) && (tries != 0) ){
        y = receive_message_timeout(5000);
        if (y == NULL || (y->payload[2] < seq)) {
            // sent previous NAK : 2 more tries
            if(y == NULL) tries --;
            printf("[keceiver] sends again[%d] SEQ[%d]\n",tries,r.payload[2]);
            send_message(&r);
        }

        else{
            
            if(check_crc(*y) ) {
            /* am primit pachet bun => 
                - trimit inapoi ACK
                - gestionez pachetul in functie de stare*/
                
                tries = 3;

                //len = y->payload[1] - 5;
                //printf("[keceiver] Got SEQ[%d]\n",y->payload[2]);
                /*printf("SOH:%x LEN:%d SEQ:%d TYPE:%c ",y->payload[0],y->payload[1],y->payload[2],y->payload[3]);
                printf("DATA[0]: %c DATA[1]: %c DATA[2]: %c DATA[k-3]: %c DATA[k-2]: %c DATA[k-1]: %c ",t.payload[4],t.payload[5],t.payload[6],t.payload[4+len-3],t.payload[4+len-2],t.payload[4+len-1]);
                printf("CHECK1:%x CHECK2:%x MARK:%x\n\n", t.payload[4+len],t.payload[4+len+1],t.payload[len+4+2]);*/

                if(y->payload[3] == 'F'){

                    len = y->payload[1] - 5;
                    printf("[kreceiver] got 'F': SOH:%x LEN:%d SEQ:%d TYPE:%c \n",y->payload[0],y->payload[1],y->payload[2],y->payload[3]);
                    //printf("DATA[0]:%c DATA[1]:%c DATA[2]:%c DATA[k-3]:%c DATA[k-2]:%c DATA[k-1]:%c ",y->payload[4],y->payload[5],y->payload[6],y->payload[4+len-3],y->payload[4+len-2],y->payload[4+len-1]);
                    //printf("CHECK1:%x CHECK2:%x MARK:%x\n\n", y->payload[4+len],y->payload[4+len+1],y->payload[len+4+2]);
                    char prefix[30] = "recv_";
                    f = fopen(strcat(prefix,y->payload + 4),"wb");
                    memcpy(r.payload,y->payload,y->len);
                    r.payload[3] = 'Y';
                    r.payload[2] = seq;
                    send_message(&r);
                    state = receive_data;
                    seq = (seq + 1)%64;
                    continue;
                }

                if(y->payload[3] == 'D'){
                    
                    //state = receive_data;
                    len = y->len - 7;
                    printf("[kreceiver] got 'D': SOH:%x LEN:%d SEQ:%d TYPE:%c\n ",y->payload[0],y->payload[1],y->payload[2],y->payload[3]);
                    //printf("DATA[0]:%c DATA[1]:%c DATA[2]:%c DATA[k-3]:%c DATA[k-2]:%c DATA[k-1]:%c ",y->payload[4],y->payload[5],y->payload[6],y->payload[4+len-3],y->payload[4+len-2],y->payload[4+len-1]);
                    //printf("CHECK1:%x CHECK2:%x MARK:%x\n\n", y->payload[4+len],y->payload[4+len+1],y->payload[len+4+2]);

                    fwrite(y->payload + 4,1,len-1,f);
                    r.payload[3] = 'Y';
                    r.payload[2] = seq;
                    send_message(&r);
                    seq = (seq + 1)%64;
                    continue;
                }

                if(y->payload[3] == 'Z'){
                    
                    //state = receive_data;
                    printf("[kreceiver] got 'Z': SOH:%x LEN:%d SEQ:%d TYPE:%c\n",y->payload[0],y->payload[1],y->payload[2],y->payload[3]);
                    fclose(f);
                    r.payload[3] = 'Y';
                    r.payload[2] = seq;
                    send_message(&r);
                    seq = (seq + 1)%64;
                    continue;
                }

                if(y->payload[3] == 'B'){

                    printf("[kreceiver] got 'B': SOH:%x LEN:%d SEQ:%d TYPE:%c\n",y->payload[0],y->payload[1],y->payload[2],y->payload[3]);
                    state = received_EOT;
                    //r.payload[3] = 'Y';
                    //r.payload[2] = y->payload[2];
                    //send_message(&r);
                    seq = (seq + 1)%64;
                    continue;
                }

            }
            else{
                // corupt -> send NAK
                r.payload[3] = 'N';
                r.payload[2] = seq;
                printf("seq=%d [keceiver] sending NAK(corupt) to SEQ[%d]\n",seq, y->payload[2]);
                send_message(&r);
                seq = (seq + 1)%64; 
            }
        }
    }

    printf("[kreceiver] pa pa\n");
	return 0;
}
