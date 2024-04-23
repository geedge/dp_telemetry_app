#include "pcapng.h"

#include <alloca.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no
 * bigger than the first parameter. Second parameter must be a
 * power-of-two value.
 */
#define ALIGN_FLOOR(val, align) (typeof(val))((val) & (~((typeof(val))((align)-1))))

/**
 * Macro to align a value to a given power-of-two. The resultant
 * value will be of the same type as the first parameter, and
 * will be no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define ALIGN_CEIL(val, align) ALIGN_FLOOR(((val) + ((typeof(val))(align)-1)), align)
#define ALIGN(val, align) ALIGN_CEIL(val, align)

static inline uint16_t pcapng_optlen(uint16_t len)
{
    return ALIGN(sizeof(struct pcapng_option) + len, sizeof(uint32_t));
}

/* build TLV option and return location of next */
static inline struct pcapng_option * pcapng_add_option(struct pcapng_option * popt, uint16_t code, const void * data,
                                                       uint16_t len)
{
    popt->code = code;
    popt->length = len;
    memcpy(popt->data, data, len);

    return (struct pcapng_option *)((uint8_t *)popt + pcapng_optlen(len));
}

/*
 * Write required initial section header describing the capture
 */
static inline int pcapng_section_block(struct pcapng_t * self, const char * os, const char * hw, const char * app,
                                       const char * comment)
{
    struct pcapng_section_header * hdr;
    struct pcapng_option * opt;
    void * buf;
    uint32_t len;
    ssize_t cc;

    len = sizeof(*hdr);
    if (hw)
        len += pcapng_optlen(strlen(hw));
    if (os)
        len += pcapng_optlen(strlen(os));
    if (app)
        len += pcapng_optlen(strlen(app));
    if (comment)
        len += pcapng_optlen(strlen(comment));

    /* reserve space for OPT_END */
    len += pcapng_optlen(0);
    len += sizeof(uint32_t);

    buf = calloc(1, len);
    if (!buf)
        return -1;

    hdr = (struct pcapng_section_header *)buf;
    *hdr = (struct pcapng_section_header){
        .block_type = PCAPNG_SECTION_BLOCK,
        .block_length = len,
        .byte_order_magic = PCAPNG_BYTE_ORDER_MAGIC,
        .major_version = PCAPNG_MAJOR_VERS,
        .minor_version = PCAPNG_MINOR_VERS,
        .section_length = UINT64_MAX,
    };

    /* After the section header insert variable length options. */
    opt = (struct pcapng_option *)(hdr + 1);
    if (comment)
        opt = pcapng_add_option(opt, PCAPNG_OPT_COMMENT, comment, strlen(comment));
    if (hw)
        opt = pcapng_add_option(opt, PCAPNG_SHB_HARDWARE, hw, strlen(hw));
    if (os)
        opt = pcapng_add_option(opt, PCAPNG_SHB_OS, os, strlen(os));
    if (app)
        opt = pcapng_add_option(opt, PCAPNG_SHB_USERAPPL, app, strlen(app));

    /* The standard requires last option to be OPT_END */
    opt = pcapng_add_option(opt, PCAPNG_OPT_END, NULL, 0);

    /* clone block_length after option */
    memcpy(opt, &hdr->block_length, sizeof(uint32_t));

    cc = write(self->outfd, buf, len);
    free(buf);

    return cc;
}

/* Write an interface block */
static inline int pcapng_interface_block(struct pcapng_t * self)
{
    struct pcapng_interface_block * hdr;
    uint32_t len;
    const uint8_t tsresol = 9; /* nanosecond resolution */
    void * buf;
    struct pcapng_option * opt;

    /* Compute length of interface block options */
    len = sizeof(*hdr);

    len += pcapng_optlen(sizeof(tsresol)); /* timestamp */
    len += pcapng_optlen(0);               /* for OPT_END */
    len += sizeof(uint32_t);               /* Block length */

    buf = alloca(len);
    if (!buf)
        return -1;

    hdr = (struct pcapng_interface_block *)buf;
    *hdr = (struct pcapng_interface_block){
        .block_type = PCAPNG_INTERFACE_BLOCK,
        .link_type = 1, /* DLT_EN10MB - Ethernet */
        .block_length = len,
    };

    opt = (struct pcapng_option *)(hdr + 1);
    opt = pcapng_add_option(opt, PCAPNG_IFB_TSRESOL, &tsresol, sizeof(tsresol));
    opt = pcapng_add_option(opt, PCAPNG_OPT_END, NULL, 0);

    /* clone block_length after optionsa */
    memcpy(opt, &hdr->block_length, sizeof(uint32_t));

    return write(self->outfd, buf, len);
}

/* Create new pcapng writer handle */
struct pcapng_t * pcapng_fdopen(int fd, const char * osname, const char * hardware, const char * appname,
                                const char * comment)
{
    struct pcapng_t * self;

    self = malloc(sizeof(*self));
    if (!self)
    {
        return NULL;
    }

    self->outfd = fd;

    if (pcapng_section_block(self, osname, hardware, appname, comment) < 0)
        goto fail;

    if (pcapng_interface_block(self) < 0)
        goto fail;

    return self;
fail:
    free(self);
    return NULL;
}

struct pcapng_t * pcapng_open(const char * path)
{
    int pcapng_fd = open(path, O_WRONLY | O_CREAT, 0640);
    if (pcapng_fd < 0)
    {
        return NULL;
    }

    return pcapng_fdopen(pcapng_fd, NULL, NULL, NULL, NULL);
}

void pcapng_close(struct pcapng_t * self)
{
    close(self->outfd);
    free(self);
}

/* Make a copy of original mbuf with pcapng header and options */
int pcapng_copy(marsio_buff_t * mbuf, uint32_t snaplen, const char * comment,
                struct pcapng_enhance_packet_block ** obj_p)
{
    uint32_t orig_len = 0;
    uint32_t data_len = 0;
    uint32_t pedding_data_len = 0;
    uint64_t timestamp;
    uint32_t len = 0;
    uint32_t optlen = 0;
    struct pcapng_option * opt = NULL;

    orig_len = marsio_buff_buflen(mbuf);
    data_len = marsio_buff_datalen(mbuf);
    data_len = snaplen < data_len ? snaplen : data_len;

    // All blocks in a PcapNG file must be aligned to a 32 bit boundary.
    pedding_data_len = ALIGN(data_len, sizeof(uint32_t));

    if (comment)
    {
        optlen += pcapng_optlen(strlen(comment));
    }

    len = sizeof(struct pcapng_enhance_packet_block) + pedding_data_len + optlen + sizeof(uint32_t);

    /* Note: END_OPT necessary here. Wireshark doesn't do it. */

    struct pcapng_enhance_packet_block * epb = calloc(1, len);
    if (epb == NULL)
    {
        return -1;
    }

    epb->block_type = PCAPNG_ENHANCED_PACKET_BLOCK;
    epb->block_length = len;

    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    timestamp = (uint64_t)current_time.tv_sec * 1000000000 + current_time.tv_nsec;

    epb->timestamp_hi = timestamp >> 32;
    epb->timestamp_lo = (uint32_t)timestamp;
    epb->capture_length = data_len;
    epb->original_length = orig_len;

    memcpy((char *)epb + sizeof(*epb), marsio_buff_mtod(mbuf), data_len);

    opt = (struct pcapng_option *)((char *)epb + sizeof(*epb) + pedding_data_len);

    if (comment)
    {
        opt = pcapng_add_option(opt, PCAPNG_OPT_COMMENT, comment, strlen(comment));
    }

    /* set trailer of block length */
    memcpy(opt, &epb->block_length, sizeof(uint32_t));

    *obj_p = epb;
    return 0;
}

/* Write pre-formatted packets to file. */
int pcapng_write_packets(struct pcapng_t * self, struct pcapng_enhance_packet_block * epbs[], uint16_t nb_epb)
{
    unsigned int write_packet_cnt = 0;
    int ret = 0;

    for (unsigned int i = 0; i < nb_epb; i++)
    {
        struct pcapng_enhance_packet_block * epb = epbs[i];

        if (epb->block_type != PCAPNG_ENHANCED_PACKET_BLOCK)
        {
            continue;
        }

        ret = write(self->outfd, epb, epb->block_length);
        if (ret == epb->block_length)
        {
            write_packet_cnt++;
        }
    }

    return write_packet_cnt;
}