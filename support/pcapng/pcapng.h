#pragma once
#include "marsio.h"
#include "pcapng_proto.h"

struct pcapng_t
{
    int outfd; /* output file */
};

struct pcapng_t * pcapng_fdopen(int fd, const char * osname, const char * hardware, const char * appname,
                                const char * comment);
struct pcapng_t * pcapng_open(const char * path);
void pcapng_close(struct pcapng_t * self);
int pcapng_copy(marsio_buff_t * mbuf, uint32_t snaplen, const char * comment,
                struct pcapng_enhance_packet_block ** obj_p);
int pcapng_write_packets(struct pcapng_t * self, struct pcapng_enhance_packet_block * epbs[], uint16_t nb_epb);