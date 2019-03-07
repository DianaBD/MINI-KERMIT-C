#ifndef LIB
#define LIB

// flosita pt pachetele de tip 'S','F','D'
typedef struct {
	char SOH;
	char LEN;
	char SEQ;
	char TYPE;
	char *DATA;
	unsigned short CHECK;
	char MARK;

}pkt_kermit ;

typedef struct {
	char MAXL;
	char TIME;
	char NPAD;
	char PADC;
	char EOL;

	char QCTL;
	char QBIN;
	char CHKT;
	char REPT;
	char CAPA;
	char R;

}__attribute__((__packed__)) data_S ;

typedef struct {
    int len;
    char payload[1400];
}__attribute__((__packed__)) msg;

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

void initialize_kermit(pkt_kermit *p){
    (*p).SOH = 0x01;
    (*p).SEQ = 0x00;
    (*p).MARK = 0x0D;
}

msg make_package_S(){
    msg t;
    pkt_kermit p;
    p.TYPE = 'S';
    initialize_kermit(&p);

    data_S s;
    s.MAXL = 0xFA;
    s.TIME = 5;
    s.NPAD = 0x00;
    s.PADC = 0x00;
    s.EOL = 0x0D;

    s.QCTL = 0x00;
    s.QBIN = 0x00;
    s.CHKT = 0x00;
    s.REPT = 0x00;
    s.CAPA = 0x00;
    s.R = 0x00;

    p.DATA = (char *)malloc(11);
    memcpy(p.DATA,&s,11);

    // campurile SEQ(1 byte) + TYPE(1 byte) + DATA(sizeof(s)) + CHECK(2 bytes) + MARK(1 byte)
    p.LEN = 2 + sizeof(s) + 3;
    //printf("%d\n ",p.LEN); // 16

    // campurile SOH(1 byte) + LEN(1 byte) + p.LEN(16 bytes)
    t.len = p.LEN + 2;
    memcpy(t.payload, &p, 4); 
    memcpy(t.payload + 4, p.DATA, 11);
    
    // crc fara campurile CHECK + MARK
    p.CHECK = crc16_ccitt(t.payload, t.len - 3);
    memcpy(t.payload + 15, &(p.CHECK), 2);
    memcpy(t.payload + 17, &(p.MARK), 1);

    return t;
}

msg make_package_F(char *file_name, int seq){
    msg t;
    pkt_kermit p;
    p.TYPE = 'F';
    initialize_kermit(&p);

    // pun secventa buna
    p.SEQ = seq;

    int header_len = strlen(file_name);

    // lungimea numelui + '\0'
    p.DATA = (char *)malloc(header_len + 1);
    memcpy(p.DATA,file_name,header_len);
    p.DATA[header_len] = '\0';

    // campurile SEQ(1 byte) + TYPE(1 byte) + DATA(header_len) + CHECK(2 bytes) + MARK(1 byte)
    p.LEN = 2 + header_len + 1 + 3;

    // campurile SOH(1 byte) + LEN(1 byte) + p.LEN
    t.len = p.LEN + 2;
    memcpy(t.payload, &p, 4); 
    memcpy(t.payload + 4, p.DATA, header_len + 1);
    
    // crc fara campurile CHECK + MARK
    p.CHECK = crc16_ccitt(t.payload, t.len - 3);
    memcpy(t.payload + t.len - 3, &(p.CHECK), 2);
    memcpy(t.payload + t.len -1, &(p.MARK), 1);

    return t;

}

msg make_package_D(char *buf, char seq, char len){
    msg t;
    pkt_kermit p;
    p.TYPE = 'D';
    initialize_kermit(&p);

    int l = (unsigned char)len;
    // pun secventa buna
    p.SEQ = seq;
    // lungimea numelui + '\0'
    p.DATA = (char *)malloc(l);
    memcpy(p.DATA,buf,l);
    // campurile SEQ(1 byte) + TYPE(1 byte) + DATA(str_len) + CHECK(2 bytes) + MARK(1 byte)
    p.LEN = 2 + l + 3;

    // campurile SOH(1 byte) + LEN(1 byte) + p.LEN
    t.len = l + 7;
    //memcpy(t.payload, &p, 4);
    t.payload[0] = p.SOH;
    t.payload[1] = l + 5; 
    t.payload[2] = seq;
    t.payload[3] = 'D';
    for(int i = 4; i <= l+4; i++)
        t.payload[i] = p.DATA[i-4];
    
    // crc fara campurile CHECK + MARK
    p.CHECK = crc16_ccitt(t.payload, t.len - 3);
    memcpy(t.payload + t.len - 3, &(p.CHECK), 2);
    memcpy(t.payload + t.len -1, &(p.MARK), 1);

    return t;

}

msg make_null_data_package(char seq, char type){
    msg t;
    pkt_kermit p;
    p.TYPE = type;
    initialize_kermit(&p);

    // pun secventa buna
    p.SEQ = seq;

    // campurile SEQ(1 byte) + TYPE(1 byte) + DATA(header_len) + CHECK(2 bytes) + MARK(1 byte)
    p.LEN = 5;

    // campurile SOH(1 byte) + LEN(1 byte) + p.LEN(5 bytes)
    t.len = p.LEN + 2;
    memcpy(t.payload, &p, 4);
    
    // crc fara campurile CHECK + MARK
    p.CHECK = crc16_ccitt(t.payload, t.len - 3);
    memcpy(t.payload + t.len - 3, &(p.CHECK), 2);
    memcpy(t.payload + t.len -1, &(p.MARK), 1);

    return t;
}

#endif

