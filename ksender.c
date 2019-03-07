#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#include <string.h>

#define HOST "127.0.0.1"
#define PORT 10000

#define send_init 1
#define send_file 2
#define send_data 3
#define send_EOT 4
#define send_EOF 5
#define stop 6


void update_packet(msg *t,int secv){
    //secv = (secv + 1)%64;
    t->payload[2] = secv;
    unsigned short crc = crc16_ccitt(t->payload, t->len - 3);
    memcpy(t->payload + t->len - 3, &crc, 2);
}

///////////////////////////                  //////////////////////////////////////                   ////////////////////////


int main(int argc, char** argv) {
    setbuf(stdout, NULL);
    msg t;
    int tries = 2;
    int state;
    char seq = 0;
    int file_nr = 1;
    int lastfile = argc;

    printf("lastfile=%d\n\n",lastfile);
    
    init(HOST, PORT);
    
    // pregatesc un pachet de tip 'S' si il trimit
    state = send_init;
    t = make_package_S();
    printf("[ksender] sends 'S': SOH:%x LEN:%d SEQ:%d TYPE:%c MAXL:%d TIME:%d NAPD:%x PADC:%x EOL:%x CHECK1:%x CHECK2:%x MARK:%x\n\n ",t.payload[0],t.payload[1],t.payload[2],t.payload[3],(unsigned char)t.payload[4],t.payload[5],t.payload[6],t.payload[7],t.payload[8],t.payload[15],t.payload[16],t.payload[17]);
    send_message(&t);

    while( (state == send_init) && (tries != 0) ){
        msg *y = receive_message_timeout(5000);
        if (y == NULL || (y->payload[2] < seq)) {
            printf("seq=%d ",seq);
            printf("[ksender] timeout -> send 'S' again[%d] SEQ[%d]\n\n",tries,t.payload[2]);
            // pachet pierdut -> tr ac packet cu secv neschimbata
            tries --;
            send_message(&t);
            continue;
        } 
        else {
            seq = (seq + 1)%64;
            tries = 3;
            if(y->payload[3] == 'N'){
                // am primit NAK pachetul a fost corupt -> retrimit ac pachet cu SEQ+1
                //tries--; trebuie?????
                update_packet(&t,seq);
                printf("   updated sequence: %d\n",t.payload[2]);
                printf("[ksender] got NAK to SEQ[%d] => send 'S' again[%d] SEQ[%d]\n",t.payload[2],tries,t.payload[2]);
                send_message(&t);
            }
            else{
                // pam primit ACK pt init -> trec in starea send_file
                state = send_file;
                printf("[ksender] got ACK to 'S' SEQ[%d]: ",y->payload[2]); 
                printf("   SOH:%x LEN:%d SEQ:%d TYPE:%c MAXL:%d TIME:%d NAPD:%x PADC:%x EOL:%x CHECK1:%x CHECK2:%x MARK:%x\n\n",y->payload[0],y->payload[1],y->payload[2],y->payload[3],(unsigned char)y->payload[4],y->payload[5],y->payload[6],y->payload[7],y->payload[8],y->payload[15],y->payload[16],y->payload[17]);

            }
        }
    }

    //printf("S a stabilit conexiunea yay yuppi\n\n");

    // am stabilit conexiunea -> trimit primul pachet file-heder
    if(tries > 0){
    t = make_package_F(argv[file_nr], seq);
    printf("[ksender] sends 'F': SOH:%x LEN:%d SEQ:%d TYPE:%c ",t.payload[0],t.payload[1],t.payload[2],t.payload[3]);
    printf("DATA: %s ",t.payload + 4);
    printf("CHECK1:%x CHECK2:%x MARK:%x\n\n", t.payload[9],t.payload[10],t.payload[11]);
    send_message(&t);
    file_nr++;
    state = send_data;
    }
    FILE* f = fopen(argv[1],"rb");
    char buf[255];


    while( (state != stop) && (tries != 0)){
        // astept ACK pt ult pachet
        msg *y = receive_message_timeout(5000);
        if (y == NULL || (y->payload[2] < seq)) {
            // pachet pierdut -> tr ac packet file cu secv neschimbata
            if(y == NULL) tries --;
            printf("[ksender] timeout -> send again[%d] SEQ[%d]\n\n",tries,t.payload[2]);
            send_message(&t);
        } 
        else {
            tries = 3;
            seq = (seq + 1)%64;

            if(y->payload[3] == 'N'){
                
                // am primit NAK pachetul file a fost corupt -> retrimit ac pachet cu SEQ+1
                //tries--; trebuie?????
                printf("seq=%d",(unsigned char)seq);
                update_packet(&t,seq);
                printf("[ksender] got NAK to SEQ[%d] => send again[%d] SEQ[%d]\n",y->payload[2],tries,(unsigned char)t.payload[2]);
                send_message(&t);
            }
            else{
                printf("[ksender] got ACK to SEQ[%d]\n",y->payload[2]);
                printf("seq=%d",(unsigned char)seq);

                if((state == send_file) && (file_nr < lastfile)) {
                    t = make_package_F(argv[file_nr], seq);
                    printf("[ksender] sends 'F': SOH:%x LEN:%d SEQ:%d TYPE:%c \n",t.payload[0],t.payload[1],t.payload[2],t.payload[3]);
                    //printf("DATA: %s ",t.payload + 4);
                    //printf("CHECK1:%x CHECK2:%x MARK:%x\n\n", t.payload[9],t.payload[10],t.payload[11]);
                    send_message(&t);
                    file_nr++;
                    state = send_data;
                    continue;

                }

                if((state == send_file) && (file_nr == lastfile)) {
                    // am trimis toate fisierele - zic papa
                    state = send_EOT;
                    fclose(f);
                    t = make_null_data_package(seq,'B');
                    //printf("[ksender] sends 'B': SOH:%x LEN:%d SEQ:%d TYPE:%c ",t.payload[0],t.payload[1],t.payload[2],t.payload[3]);
                    //printf("CHECK1:%x CHECK2:%x MARK:%x\n\n", t.payload[5],t.payload[6],t.payload[7]);
                    send_message(&t);
                    continue;
                }

                if(state == send_data){
                    char k = fread(buf,1,254,f);
                    if((unsigned char)k < 254)
                        state = send_EOF;
                    buf[(unsigned char)k] ='\0';
                    t = make_package_D(buf,seq,k+1);
                    printf("[ksender] sends 'D': SOH:%x LEN:%d SEQ:%d TYPE:%c \n",t.payload[0],t.payload[1],t.payload[2],t.payload[3]);
                    //printf("DATA[0]:%c DATA[1]:%c DATA[2]:%c DATA[k-3]:%c DATA[k-2]:%c DATA[k-1]:%c ",t.payload[4],t.payload[5],t.payload[6],t.payload[t.len-6],t.payload[t.len-5],t.payload[t.len-4]);
                    //printf("CHECK1:%x CHECK2:%x MARK:%x\n\n", t.payload[4+(unsigned char)k +1],t.payload[4+(unsigned char)k+2],t.payload[(unsigned char)k+4+3]);
                    send_message(&t);
                    
                    continue;
                }

                if(state == send_EOF){
                    printf("file_nr=%d\n",file_nr);
                    t = make_null_data_package(seq,'Z');
                    send_message(&t);
                    printf("[ksender] sends 'Z' SEQ[%d]\n\n\n\n",seq); 
                    fclose(f);
                    if(file_nr < lastfile){
                        f = fopen(argv[file_nr],"rb");
                        state = send_file;
                    }   
                    else
                        state = send_EOT; 
                    continue;
                }

                if(state == send_EOT){
                    t = make_null_data_package(seq,'B');
                    send_message(&t);
                    printf("[ksender] sends 'B' end of transmission \n ");
                    state = stop;
                    continue;
                }
            }
        }
    }
    printf("[ksender] pa pa\n");

    return 0;
}
