/* Defines functions for 
 * CSC 357, Assignment 1
 * Given code, Spring '24. */ 
 
#include <pcap/pcap.h>
#include <arpa/inet.h>
#include "checksum.h"

/* Header Structures */
struct Ethernet_header{
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;
}__attribute__((packed));

struct IP_header{
    uint8_t version_IHL;
    uint16_t length;\
    uint16_t identification;
    uint16_t flags_offset; 
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src;
    uint32_t dest;
}__attribute__((packed));

struct ARP_header{
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_length;
    uint8_t protocol_length;
    uint16_t operation;
    uint32_t sender_hardware_addr;
    uint32_t sender_protocol_addr;
    uint32_t target_hardware_addr;
    uint32_t target_protocol_addr;
}__attribute__((packed));

struct ICMP_header{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t content;
}__attribute__((packed));

struct TCP_header{
    uint16_t src;
    uint16_t dest;
    uint32_t sequence;
    uint32_t ack;
    uint16_t offset_res_flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
}__attribute__((packed));

struct UDP_header{
    uint16_t src;
    uint16_t dest;
    uint16_t length;
    uint16_t checksum;
}

void IP_print(){
    
}

void find_next(uint8_t type){
    printf("Type: ");
    switch(e_head->type){
        //case(1) IP_print();

    }
}

int8_t main(int argc, char* argv[]){
    char errbuf[PCAP_ERRBUF_SIZE]; // buffer for pcap open file error
    pcap_t *p; // packet information read from pcap file
    struct pcap_pkthdr **pkt_header;
    const u_char **pkt_data;
    int8_t packet_num = 1;

    packet_info = pcap_open_offline(argv[1], errbuf);
    //error check
    if (p == NULL) {
        fprintf(stderr, "Error opening file: %s\n", errbuf);
        return EXIT_FAILURE;
    }
    while(pcap_next_ex(p, pkt_header, pkt_data)){
        printf("Packet Number: %d  Packet Len: %d\n\n", packet_num, pkt_header->len);
        struct Ethernet_header *e_head = (struct Ethernet_header *)pkt_header;
        printf("Ethernet Header\n    Dest MAC: %02x:%02x:%02x:%02x\n", 
        e_head->dest[0], e_head->dest[1], e_head->dest[2], e_head->dest[3], e_head->dest[4], e_head->dest[5]);
        printf("Source MAC: %02x:%02x:%02x:%02x\n", 
        e_head->src[0], e_head->src[1], e_head->src[2], e_head->src[3], e_head->src[4], e_head->src[5]);
        find_next(e_head->type);
    }
    // clean up
    pcap_close(packet_info);
    return 0;
}
