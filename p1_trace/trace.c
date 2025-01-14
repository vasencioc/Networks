/* Defines functions for 
 * CSC 357, Assignment 1
 * Given code, Spring '24. */ 
 
#include <pcap/pcap.h>
#include "checksum.h"

struct ethernet_header{
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type
}__attribute__((packed));

int main(int argc, char* argv[]){
    char errbuf[PCAP_ERRBUF_SIZE]; // buffer for pcap open file error
    pcap_t *packet_info; // packet information read from pcap file
    // struct pcap_pkthdr *pkt_header;
    // const u_char *pkt_data;

    // packet_info = pcap_open_offline(argv[1], errbuf);
    // //error check
    // if (packet_info == NULL) {
    //     fprintf(stderr, "Error opening file: %s\n", errbuf);
    //     return 2;
    // }
    // printf("Successfully opened trace file: %s\n", argv[1]);

    // if(pcap_next_ex(packet_info, &pkt_header, &pkt_data) ){
    //     printf("Packet captured at: %ld.%06ld\n", pkt_header->ts.tv_sec, pkt_header->ts.tv_usec);
    //     printf("Packet length: %u bytes\n", pkt_header->len);
    //     printf("Captured length: %u bytes\n\n", pkt_header->caplen);
    // }
    

    // clean up
    pcap_close(packet_info);
    return 0;
}
