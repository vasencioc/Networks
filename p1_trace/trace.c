/* Defines functions for 
 * CSC 357, Assignment 1
 * Given code, Spring '24. */ 
 
#include <pcap/pcap.h>
#include <netinet/ether.h>
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
    uint8_t TOS;
    uint16_t length;
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
}__attribute__((packed));

void ICMP_print(const u_char *pkt_data){
    struct ICMP_header *icmp_head = (struct ICMP_header *)pkt_data;
    switch(icmp_head->type){
        case(0):{
            printf("\n\tICMP Header\n\t\tType: Reply\n");
            break;
        }
        case(8):{
            printf("\n\tICMP Header\n\t\tType: Request\n");
            break;
        }
        default:{
            printf("\n\tICMP Header\n\t\tType: Error\n");
            break;
        }
    }
}

void ARP_print(const u_char *pkt_data){
    struct ARP_header *arp_head = (struct ARP_header *)pkt_data;
    switch(arp_head->operation){
        case(2):{
            printf("\n\tARP Header\n\t\tType: Reply\n");
            break;
        }
        case(1):{
            printf("\n\tARP Header\n\t\tType: Request\n");
            break;
        }
        default:{
            printf("\n\tARP Header\n\t\tType: Error (%u)\n", arp_head->operation);
            break;
        }
    }
    printf("\t\tSender MAC: %s\n", inet_ntoa(*(struct in_addr *)&arp_head->sender_hardware_addr));
    printf("\t\tSender MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
    arp_head->sender_hardware_addr, arp_head->sender_hardware_addr + 1, arp_head->sender_hardware_addr + 2, arp_head->sender_hardware_addr + 3, arp_head->sender_hardware_addr + 4, arp_head->sender_hardware_addr + 5);
    printf("\t\tSender IP: %s\n", inet_ntoa(*(struct in_addr *)&arp_head->sender_protocol_addr));
    printf("\t\tSender MAC: %s\n", inet_ntoa(*(struct in_addr *)&arp_head->target_hardware_addr));
    printf("\t\tSender IP: %s\n", inet_ntoa(*(struct in_addr *)&arp_head->target_protocol_addr));
}

void IP_print(const u_char *pkt_data){
    struct IP_header *ip_head = (struct IP_header *)pkt_data;
    uint8_t version = ip_head->version_IHL >> 4;
    uint8_t ihl = ip_head->version_IHL & 0x0F;
    uint8_t diffserv = ip_head->TOS >> 2;
    uint8_t ecn = ip_head->TOS & 0x03;
    printf("\n\tIP Header\n\t\tIP Version: %u\n", version);
    printf("\t\tHeader Len (bytes): %u\n", ihl * 4);
    printf("\t\tTOS subfields:\n\t\t   Diffserv bits: %u\n", diffserv);
    printf("\t\t   ECN bits: %u\n", ecn);
    printf("\t\tTTL: %u\n", ip_head->ttl);
    switch(ip_head->protocol){
        case(1):{
            printf("\t\tProtocol: ICMP\n");
            break;
        }
        case(6):{
            printf("\t\tProtocol: TCP\n");
            break;
        }
        case(17):{
            printf("\t\tProtocol: UDP\n");
            break;
        }
        default:{
            printf("\t\tProtocol: Unknown\n");
            break;
        }
    }
    printf("\t\tChecksum: \n");
    printf("\t\tSender IP: %s\n", inet_ntoa(*(struct in_addr *)&ip_head->src));
    printf("\t\tDest IP: %s\n", inet_ntoa(*(struct in_addr *)&ip_head->dest));
    switch(ip_head->protocol){
        case(1):{
            pkt_data += sizeof(struct IP_header);
            ICMP_print(pkt_data);
            break;
        }
        case(6):{
            //TCP_print();
            break;
        }
        default: break;
    }
}

void Ethernet_print(const u_char *pkt_data){
    struct Ethernet_header *e_head = (struct Ethernet_header *)pkt_data;
    printf("\tEthernet Header\n\t\tDestination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
    e_head->dest[0], e_head->dest[1], e_head->dest[2], e_head->dest[3], e_head->dest[4], e_head->dest[5]);
    printf("\t\tSource MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
    e_head->src[0], e_head->src[1], e_head->src[2], e_head->src[3], e_head->src[4], e_head->src[5]);
    switch(ntohs(e_head->type)){
        case(0x0800):{
            printf("\t\tType: IP\n");
            pkt_data += sizeof(struct Ethernet_header);
            IP_print(pkt_data);
            break;
        }
        case(0x86DD):{
            printf("\t\tType: IP\n");
            pkt_data += sizeof(struct Ethernet_header); 
            IP_print(pkt_data);
            break;
        }
        case(0x0806):{
            printf("\t\tType: ARP\n");
            pkt_data += sizeof(struct Ethernet_header);
            ARP_print(pkt_data);
            break;
        }
        default: printf("\t\tType: error\n"); break;
    }
}

int main(int argc, char* argv[]){
    pcap_t *p;
    struct pcap_pkthdr *pkt_header;
    const u_char *pkt_data;
    char errbuf[PCAP_ERRBUF_SIZE];
    int8_t packet_num = 1;
    p = pcap_open_offline(argv[1], errbuf);
    //error check
    if (p == NULL) {
        fprintf(stderr, "Error opening file: %s\n", errbuf);
        return 2;
    }
    while(pcap_next_ex(p, &pkt_header, &pkt_data) == 1){
        printf("Packet Number: %d  Packet Len: %u\n\n", packet_num, pkt_header->len);
        Ethernet_print(pkt_data);
        packet_num++;
    }
    // clean up
    pcap_close(p);
    return 0;
}
