
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <fcntl.h>

#define PRECORD_STATUS_BOOTABLE         0x80
#define PRECORD_STATUS_NON_BOOTABLE     0x00
#define PRECORD_STATUS_BOOTABLE_LBA     0x81
#define PRECORD_STATUS_NON_BOOTABLE_LBA 0x01

#define PRECORD_TYPE_EXTENDED_1     0x05
#define PRECORD_TYPE_EXTENDED_2     0x0F
#define PRECORD_TYPE_UNUSED         0x00
#define PRECORD_TYPE_NTFS           0x07
#define PRECORD_LINUX_SWAP          0x82
#define PRECORD_LINUX               0x83

#pragma pack(1)
typedef struct {
    uint8_t  head;
    uint16_t cs;
} chs_block_t;

#pragma pack(1)


typedef struct {
    uint8_t     status;
    chs_block_t start_chs;
    uint8_t     type;
    chs_block_t end_chs;
    uint32_t    first_lba_sector;
    uint32_t    num_sectors;
} precord_t;

typedef struct {
    uint8_t     status;
    uint8_t     signature1;
    uint16_t    start_high;
    uint8_t     type;
    uint8_t     signature2;
    uint16_t    len_high;
    uint32_t    start_low;
    uint32_t    len_low;
} precord_lba_t;

#pragma pack(1)
typedef struct {
    uint8_t   code[440];
    uint32_t  disk_signiture;
    uint16_t  null;
    precord_t ptable[4];
    uint16_t  mbr_signature;
} mbr_t;

void
print_partition_geom(precord_t part)
{
    uint16_t c,s;
    /* print start */
    fprintf(stdout," - start chs: ");
    s = part.start_chs.cs&0x3F; /* only 6bits are c */
    c = (part.start_chs.cs&0xFFC0)>>6; /* 10 bits */
    fprintf(stdout," h:%u c:%u s:%u\n",part.start_chs.head,c,s);

    fprintf(stdout," - ending chs: ");
    s = part.end_chs.cs&0x3F; /* only 6bits are c */
    c = (part.end_chs.cs&0xFFC0)>>6;; /* 10 bits */
    fprintf(stdout," h:%u c:%u s:%u",part.end_chs.head,c,s);
    if (part.end_chs.head == 254 && s == 63 && c == 1023) {
        fprintf(stdout," (size>8GB)\n");
    } else {
        fprintf(stdout,"\n");
    }

    fprintf(stdout," - first LBA sector: %u\n",part.first_lba_sector);
    fprintf(stdout," - number of sectors: %u \n",part.num_sectors);
}

void
print_partition_status(uint8_t status)
{
    fprintf(stdout," - status: ");
    if (status == PRECORD_STATUS_BOOTABLE) {
        fprintf(stdout,"bootable\n");
        return;
    }
    if (status == PRECORD_STATUS_NON_BOOTABLE) {
        fprintf(stdout,"non-bootable\n");
        return;
    }
    if (status == PRECORD_STATUS_BOOTABLE_LBA) {
        fprintf(stdout,"bootable lba\n");
        return;
    }
    if (status == PRECORD_STATUS_NON_BOOTABLE_LBA) {
        fprintf(stdout,"non-bootable lba\n");
        return;
    }
    fprintf(stdout,"invalid partition status\n");
}

void
print_partition_type(uint8_t type)
{
    fprintf(stdout," - type: ");
    switch (type) {
        case PRECORD_TYPE_UNUSED:
            fprintf(stdout,"unused\n");
            break;
        case PRECORD_TYPE_NTFS:
            fprintf(stdout,"ntfs\n");
            break;
        case PRECORD_LINUX_SWAP:
            fprintf(stdout,"linux-swap\n");
            break;
        case PRECORD_LINUX:
            fprintf(stdout,"linux\n");
            break;
        case PRECORD_TYPE_EXTENDED_1:
        case PRECORD_TYPE_EXTENDED_2:
            fprintf(stdout,"extended\n");
            break;
        default:
            fprintf(stdout,"unknown\n");
            break;
    }
}

int main(int argc,char** argv)
{
    struct hd_driveid id;
    mbr_t mbr;
    int dfd,i;
    uint64_t sectors;
    if (argc != 2) {
        fprintf(stderr,"Usage: %s <device name>\n",argv[0]);
        return EXIT_FAILURE;
    }
    const char* device = argv[1];

    /* open device */
    dfd = open(device,O_RDONLY|O_NONBLOCK);
    if (dfd == -1) {
        perror("Error opening device");
        return EXIT_FAILURE;
    }

    /* output */
    fprintf(stdout,"DEVICE: %s\n",device);

    /* get mbr */
    if (read(dfd,(void*)&mbr,sizeof(mbr_t)) != sizeof(mbr_t)) {
        fprintf(stderr,"Error reading mbr.");
        return EXIT_FAILURE;
    }

    /* get hdd info */
    if (ioctl(dfd,HDIO_GET_IDENTITY, &id) <0) {
        perror("ioctl error");
        return EXIT_FAILURE;
    } else {
        sectors = id.lba_capacity_2;
        fprintf(stdout,"Sectors: %lu\n",sectors);
        /* output hdd info */
        fprintf(stdout,"Serial No: ");
        for (i=0; i<sizeof(id.serial_no); i++)
            fprintf(stdout,"%c",id.serial_no[i]);
        fprintf(stdout,"\n");
        fprintf(stdout,"Model: ");
        for (i=0; i<sizeof(id.model); i++)
            fprintf(stdout,"%c",id.model[i]);
        fprintf(stdout,"\n");
        fprintf(stdout,"Firmware: ");
        for (i=0; i<sizeof(id.fw_rev); i++)
            fprintf(stdout,"%c",id.fw_rev[i]);
        fprintf(stdout,"\n");
    }

    close(dfd);

    /* check MBR SIGNATURE */
    if (mbr.mbr_signature != 0xAA55) {
        fprintf(stderr,"No valid bootsector found on device '%s'.",device);
        return EXIT_FAILURE;
    }

    /* read the partition table */
    for (i=0; i<4; i++) {
        /* check if used */
        if (mbr.ptable[i].type == PRECORD_TYPE_UNUSED) continue;

        fprintf(stdout,"Partition %d\n",i+1);
        print_partition_status(mbr.ptable[i].status);
        print_partition_type(mbr.ptable[i].type);
        print_partition_geom(mbr.ptable[i]);
    }

    return EXIT_SUCCESS;
}

