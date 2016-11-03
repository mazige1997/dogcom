#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "libs/md4.h"
#include "libs/md5.h"
// #include "libs/sha1.h"
#include "keepalive.h"
#include "configparse.h"
#include "auth.h"
#include "debug.h"

int keepalive_1(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]) {
    unsigned char keepalive_1_packet[42], recv_packet[100], MD5A[16];
    memset(keepalive_1_packet, 0, 42);
    keepalive_1_packet[0] = 0xff;
    int MD5A_len = 6 + strlen(drcom_config.password);
    unsigned char MD5A_str[MD5A_len];
    MD5A_str[0] = 0x03;
    MD5A_str[1] = 0x01;
    memcpy(MD5A_str + 2, seed, 4);
    memcpy(MD5A_str + 6, drcom_config.password, strlen(drcom_config.password));
    MD5(MD5A_str, MD5A_len, MD5A);
    memcpy(keepalive_1_packet + 1, MD5A, 16);
    memcpy(keepalive_1_packet + 20, auth_information, 16);
    keepalive_1_packet[36] = rand() & 0xff;
    keepalive_1_packet[37] = rand() & 0xff;

    sendto(sockfd, keepalive_1_packet, 42, 0, (struct sockaddr *)&addr, sizeof(addr));

    if (verbose_flag) {
        print_packet("[Keepalive1 sent] ", keepalive_1_packet, 42);
    }

#ifdef TEST
    printf("[TEST MODE]IN TEST MODE, PASS\n");
    return 0;
#endif

    socklen_t addrlen = sizeof(addr);
    if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
        perror("Failed to recv data");
        return 1;
    }
    if (recv_packet[0] != 0x07) {
        printf("Bad keepalive1 response received.\n");
        return 1;
    }

    if (verbose_flag) {
        print_packet("[Keepalive1 recv] ", recv_packet, sizeof(recv_packet));
    }

    return 0;
}

void keepalive_2_packetbuilder(unsigned char keepalive_2_packet[], int keepalive_counter, int filepacket, int type){
    keepalive_2_packet[0] = 0x07;
    keepalive_2_packet[1] = keepalive_counter;
    keepalive_2_packet[2] = 0x28;
    keepalive_2_packet[4] = 0x0b;
    keepalive_2_packet[5] = type;
    if (filepacket) {
        keepalive_2_packet[6] = 0x0f; 
        keepalive_2_packet[7] = 0x27;
    } else {
        memcpy(keepalive_2_packet + 6, drcom_config.KEEP_ALIVE_VERSION, 2);
    }
    keepalive_2_packet[8] = 0x2f;
    keepalive_2_packet[9] = 0x12; 
    if(type == 3) {
        unsigned char host_ip[4];
        sscanf(drcom_config.host_ip, "%hhd.%hhd.%hhd.%hhd",
               &host_ip[0],
               &host_ip[1],
               &host_ip[2],
               &host_ip[3]);
        memcpy(keepalive_2_packet + 28, host_ip, 4);
    }
}

int keepalive_2(int sockfd, struct sockaddr_in addr, unsigned char seed[], int *keepalive_counter, int *first) {
    unsigned char keepalive_2_packet[40], recv_packet[40], tail[4];
    socklen_t addrlen = sizeof(addr);

#ifdef TEST
        printf("[TEST MODE]IN TEST MODE, PASS\n");
#else
    if (*first) {
        // send the file packet
        memset(keepalive_2_packet, 0, 40);
        keepalive_2_packetbuilder(keepalive_2_packet, *keepalive_counter % 0xFF, *first, 1);
        (*keepalive_counter)++;

        sendto(sockfd, keepalive_2_packet, 40, 0, (struct sockaddr *)&addr, sizeof(addr));

        if (verbose_flag) {
            print_packet("[Keepalive2_file sent] ", keepalive_2_packet, 40);
        }
        if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
            perror("Failed to recv data");
            return 1;
        }
        if (verbose_flag) {
            print_packet("[Keepalive2_B recv] ", recv_packet, sizeof(recv_packet));
        }

        if (recv_packet[0] == 0x07) {
            if (recv_packet[2] == 0x10) {
                printf("Filepacket received.\n");
            } else if (recv_packet[2] != 0x28) {
                printf("Bad keepalive2 response received.\n");
                return 1;
            }
        } else {
            printf("Bad keepalive2 response received.\n");
            return 1;
        }
    }
#endif

    // send the first pacekt
    *first = 0;
    memset(keepalive_2_packet, 0, 40);
    keepalive_2_packetbuilder(keepalive_2_packet, *keepalive_counter % 0xFF, *first, 1);
    (*keepalive_counter)++;
    sendto(sockfd, keepalive_2_packet, 40, 0, (struct sockaddr *)&addr, sizeof(addr));

    if (verbose_flag) {
        print_packet("[Keepalive2_A sent] ", keepalive_2_packet, 40);
    }

#ifdef TEST
    unsigned char test[4] = {0x13, 0x38, 0xe2, 0x11};
    memcpy(tail, test, 4);
    print_packet("[TEST MODE]<PREP TAIL> ", tail, 4);
#else
    if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
        perror("Failed to recv data");
        return 1;
    }
    if (verbose_flag) {
        print_packet("[Keepalive2_B recv] ", recv_packet, sizeof(recv_packet));
    }

    if (recv_packet[0] == 0x07) {
        if (recv_packet[2] != 0x28) {
            printf("Bad keepalive2 response received.\n");
            return 1;
        }
    } else {
        printf("Bad keepalive2 response received.\n");
        return 1;
    }
    memcpy(tail, &recv_packet[16], 4);
#endif

#ifdef DEBUG
    print_packet("<GET TAIL> ", tail, 4);
#endif

    // send the third packet
    memset(keepalive_2_packet, 0, 40);
    keepalive_2_packetbuilder(keepalive_2_packet, *keepalive_counter % 0xFF, *first, 3);
    memcpy(keepalive_2_packet + 16, tail, 4);
    (*keepalive_counter)++;
    sendto(sockfd, keepalive_2_packet, 40, 0, (struct sockaddr *)&addr, sizeof(addr));

    if (verbose_flag) {
        print_packet("[Keepalive2_C sent] ", keepalive_2_packet, 40);
    }

#ifdef TEST
    printf("[TEST MODE]IN TEST MODE, PASS\n");
    exit(0);
#endif

    if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
        perror("Failed to recv data");
        return 1;
    }
    if (verbose_flag) {
        print_packet("[Keepalive2_D recv] ", recv_packet, sizeof(recv_packet));
    }

    if (recv_packet[0] == 0x07) {
        if (recv_packet[2] != 0x28) {
            printf("Bad keepalive2 response received.\n");
            return 1;
        }
    } else {
        printf("Bad keepalive2 response received.\n");
        return 1;
    }

    return 0;
}