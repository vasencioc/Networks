/* Defines functions for printing packet data of 
 * a network from a pcap file
 * CPE 464, Program 1
 */ 
 
#include <pcap/pcap.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <string.h>
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
    uint8_t sender_hardware_addr[6];
    uint32_t sender_protocol_addr;
    uint8_t target_hardware_addr[6];
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

struct TCP_pseudoheader{
    uint32_t srcIP;
    uint32_t destIP;
    uint8_t reserved;
    uint8_t protocol;
    uint16_t TCPlen;
}__attribute__((packed));

struct UDP_header{
    uint16_t src;
    uint16_t dest;
    uint16_t length;
    uint16_t checksum;
}__attribute__((packed));

/*Helper function*/

/**
 * Prints port number types
 * @param port_num TCP/UDP source and destination port number
 * @return void
 */
void print_port(uint16_t port_num){
    switch(port_num){
        case(53): printf("DNS\n"); break;
        case(80): printf("HTTP\n"); break;
        case(23): printf("TELNET\n"); break;
        case(21): printf("FTP_CONTROL\n"); break;
        case(20): printf("FTP_DATA\n"); break;
        case(110): printf("POP3\n"); break;
        case(25): printf("SMTP\n"); break;
        default: printf("%u\n", port_num); break;
    }
}

/*Header info printing functions*/

/**
 * Prints UDP header information
 * @param pkt_data pointer to header data
 * @return void
 */
void UDP_print(const u_char *pkt_data){
    //Copy header information into the header structure
    struct UDP_header *udp_head = (struct UDP_header *)pkt_data;
    //print header information
    printf("\n\tUDP Header\n");
    //put addresses in host format for printing
    uint16_t source = ntohs(udp_head->src);
    uint16_t destination = ntohs(udp_head->dest);
    //check if address indicates DNS
    printf("\t\tSource Port:  ");
    print_port(source);
    printf("\t\tDest Port:  ");
    print_port(destination);
}

/**
 * Prints TCP header information
 * @param pkt_data pointer to header data
 * @param pseudoheader struct containing IP infor for TCP checksum pseudoheader
 * @return void
 */
void TCP_print(const u_char *pkt_data, struct TCP_pseudoheader pseudohead){
    //Copy header information into the header structure
    struct TCP_header *tcp_head = (struct TCP_header *)pkt_data;
    //format header info
    uint16_t offset_res_flags = ntohs(tcp_head->offset_res_flags);
    uint8_t flags = offset_res_flags & 0xFF; //get flags
    uint8_t offset = (offset_res_flags >> 12) * 4; //get offset value
    
    //calculate pseudoheader length
    int pseudo_and_tcp_len = sizeof(pseudohead) + ntohs(pseudohead.TCPlen);
    //build array for checksum pseudoheader
    uint8_t checksum_val[pseudo_and_tcp_len];
    memcpy(checksum_val, &pseudohead, sizeof(pseudohead));
    memcpy(checksum_val + sizeof(pseudohead), pkt_data, ntohs(pseudohead.TCPlen));
    
    //print TCP header information
    printf("\n\tTCP Header\n\t\tSource Port:  ");
    print_port(ntohs(tcp_head->src));
    printf("\t\tDest Port:  ");
    print_port(ntohs(tcp_head->dest));
    printf("\t\tSequence Number: %u\n", ntohl(tcp_head->sequence));
    printf("\t\tACK Number: %u\n", ntohl(tcp_head->ack));
    printf("\t\tData Offset (bytes): %u\n", offset);
    (flags&2) ? printf("\t\tSYN Flag: Yes\n") : printf("\t\tSYN Flag: No\n");
    (flags&4) ? printf("\t\tRST Flag: Yes\n") : printf("\t\tRST Flag: No\n");
    (flags&1) ? printf("\t\tFIN Flag: Yes\n") : printf("\t\tFIN Flag: No\n");
    (flags&16) ? printf("\t\tACK Flag: Yes\n") : printf("\t\tACK Flag: No\n");
    printf("\t\tWindow Size: %u\n", ntohs(tcp_head->window_size));

    //run checksum function and print results
    uint16_t check = in_cksum((unsigned short *)checksum_val, pseudo_and_tcp_len);
    (check == 0) ? printf("\t\tChecksum: Correct (0x%04x)\n", ntohs(tcp_head->checksum)) : printf("\t\tChecksum: Incorrect (0x%04x)\n", ntohs(tcp_head->checksum));
}

/**
 * Prints ICMP header information
 * @param pkt_data pointer to header data
 * @return void
 */
void ICMP_print(const u_char *pkt_data){
    //Copy header information into the header structure
    struct ICMP_header *icmp_head = (struct ICMP_header *)pkt_data;
    //print ICMP type
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
            printf("\n\tICMP Header\n\t\tType: %u\n", icmp_head->type);
            break;
        }
    }
}

/**
 * Prints ARP header information
 * @param pkt_data pointer to header data
 * @return void
 */
void ARP_print(const u_char *pkt_data){
    //Copy header information into the header structure
    struct ARP_header *arp_head = (struct ARP_header *)pkt_data;
    struct ether_addr sender_mac, target_mac; //create structs for ether_ntoa()
    //copy mac addresses into structs for printing
    memcpy(sender_mac.ether_addr_octet, arp_head->sender_hardware_addr, 6);
    memcpy(target_mac.ether_addr_octet, arp_head->target_hardware_addr, 6);
    uint16_t operation = ntohs(arp_head->operation); //put operation code in host format for printing
    //print ARP operation
    switch(operation){
        case(2):{
            printf("\n\tARP header\n\t\tOpcode: Reply\n");
            break;
        }
        case(1):{
            printf("\n\tARP header\n\t\tOpcode: Request\n");
            break;
        }
        default:{
            printf("\n\tARP header\n\t\tType: Error (%u)\n", arp_head->operation);
            break;
        }
    }
    //print addresses using *ntoa() functions for formatting
    printf("\t\tSender MAC: %s\n", ether_ntoa(&sender_mac));
    printf("\t\tSender IP: %s\n", inet_ntoa(*(struct in_addr *)&arp_head->sender_protocol_addr));
    printf("\t\tTarget MAC: %s\n", ether_ntoa(&target_mac));
    printf("\t\tTarget IP: %s\n\n", inet_ntoa(*(struct in_addr *)&arp_head->target_protocol_addr));
}

/**
 * Prints IP header information
 * @param pkt_data pointer to header data
 * @return void
 */
void IP_print(const u_char *pkt_data){
    //Copy header information into the header structure
    struct IP_header *ip_head = (struct IP_header *)pkt_data;
    struct TCP_pseudoheader tcp_pseudohead; //create TCP pseudoheader
    //populate pseudoheader with IP header information
    tcp_pseudohead.srcIP = ip_head->src;
    tcp_pseudohead.destIP = ip_head->dest;
    tcp_pseudohead.reserved = 0;
    tcp_pseudohead.protocol = 6;
    //format header information
    uint8_t version = ip_head->version_IHL >> 4;
    uint8_t ihl = (ip_head->version_IHL & 0x0F) * 4;
    tcp_pseudohead.TCPlen = htons(ntohs(ip_head->length) - ihl); //use formatted ihl for pseudoheader
    uint8_t diffserv = ip_head->TOS >> 2;
    uint8_t ecn = ip_head->TOS & 0x03;
    //run checksum function
    uint16_t check = in_cksum((unsigned short *)ip_head, ihl);
    //print header info
    printf("\n\tIP Header\n\t\tIP Version: %u\n", version);
    printf("\t\tHeader Len (bytes): %u\n", ihl);
    printf("\t\tTOS subfields:\n\t\t   Diffserv bits: %u\n", diffserv);
    printf("\t\t   ECN bits: %u\n", ecn);
    printf("\t\tTTL: %u\n", ip_head->ttl);
    switch(ip_head->protocol){
        case(1): printf("\t\tProtocol: ICMP\n"); break;
        case(6): printf("\t\tProtocol: TCP\n"); break;
        case(17): printf("\t\tProtocol: UDP\n"); break;
        default: printf("\t\tProtocol: Unknown\n"); break;
    }
    // print checksum results
    (check == 0) ? printf("\t\tChecksum: Correct (0x%04x)\n", ntohs(ip_head->checksum)) : printf("\t\tChecksum: Incorrect (0x%04x)\n", ntohs(ip_head->checksum));
    printf("\t\tSender IP: %s\n", inet_ntoa(*(struct in_addr *)&ip_head->src));
    printf("\t\tDest IP: %s\n", inet_ntoa(*(struct in_addr *)&ip_head->dest));
    //move pointer to next header and call function to print it
    pkt_data += ihl; 
    switch(ip_head->protocol){
        case(1): ICMP_print(pkt_data); break;
        case(6): TCP_print(pkt_data, tcp_pseudohead); break;
        case(17): UDP_print(pkt_data); break;
        default: break;
    }
}

/**
 * Prints Ethernet header information
 * @param pkt_data pointer to header data
 * @return void
 */
void Ethernet_print(const u_char *pkt_data){
    //copy packet data into the ethernet header values
    struct Ethernet_header *e_head = (struct Ethernet_header *)pkt_data;
    struct ether_addr src_mac, dest_mac; //create structs for ether_ntoa()
    //copy mac addresses into structs for printing
    memcpy(src_mac.ether_addr_octet, e_head->src, 6);
    memcpy(dest_mac.ether_addr_octet, e_head->dest, 6);
    //print mac addresse using ether_ntoa() for formatting
    printf("\tEthernet Header\n\t\tDest MAC: %s\n", ether_ntoa(&dest_mac));
    printf("\t\tSource MAC: %s\n", ether_ntoa(&src_mac));
    //unpack and print next header
    switch(ntohs(e_head->type)){
        case(0x0800):{ //print IP header
            printf("\t\tType: IP\n");
            pkt_data += sizeof(struct Ethernet_header);
            IP_print(pkt_data);
            break;
        }
        case(0x0806):{ //print ARP header
            printf("\t\tType: ARP\n");
            pkt_data += sizeof(struct Ethernet_header);
            ARP_print(pkt_data);
            break;
        }
        default: printf("\t\tType: unknown\n"); break;
    }
}

/* Program Entry Point */
int main(int argc, char* argv[]){
    //Define values for pcap_next_ex() function
    pcap_t *p;
    struct pcap_pkthdr *pkt_header;
    const u_char *pkt_data;
    char errbuf[PCAP_ERRBUF_SIZE]; //buffer for pcap error message
    int16_t packet_num = 1; //for tracking packet numbers
    p = pcap_open_offline(argv[1], errbuf); //open pcap file
    //check for error opening pcap file
    if (p == NULL) {
        fprintf(stderr, "Error opening file: %s\n", errbuf);
        return 0;
    }
    //retrieve packets
    while(pcap_next_ex(p, &pkt_header, &pkt_data) == 1){
        printf("\nPacket number: %d  Packet Len: %u\n\n", packet_num, pkt_header->len);
        //start printing headers
        Ethernet_print(pkt_data);
        packet_num++;
    }
    //close pcap file
    pcap_close(p);
    return 1;
}
