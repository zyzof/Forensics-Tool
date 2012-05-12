/* Author: Joshua Scott */
/* Description: Sample code to read the partition table for the COSC425 group project */
/*                  So far, only supporting primary partitions. */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
//#include <stropts.h>
#include <linux/hdreg.h>
#include <linux/fs.h>

#include "disk.h"

unsigned int read_uint32(unsigned char *buffer, unsigned int index) {
    return  buffer[index]
         + (buffer[index + 0x01] << 8)
         + (buffer[index + 0x02] << 16)
         + (buffer[index + 0x03] << 24);
}

int print_type(unsigned char type, char *output) {
    int index = 0;
    switch(type) { /* Values based on Wikipedia article on Partition Type */
        case 0x00:
            index += sprintf(output + index, "Empty");
            break;
        case 0x05:
        case 0x0F:
        case 0x15:
        case 0x1F:
        case 0x85:
        case 0x91:
        case 0x9B:
        case 0xC5:
        case 0xCF:
        case 0xD5:
            index += sprintf(output + index, "Extended partition");
            break;
        case 0x41:
        case 0x43:
        case 0x83:
        case 0x88:
        case 0x89:
        case 0xFD:
            index += sprintf(output + index, "Linux");
            break;
        case 0x42:
        case 0x82:
            index += sprintf(output + index, "Linux swap");
            break;
        case 0x01:
        case 0xC1:
        case 0xD1:
            index += sprintf(output + index, "FAT12");
            break;
        case 0x06:
        case 0x08:
        case 0x11:
        case 0x14:
        case 0x16:
        case 0x24:
        case 0x56:
        case 0x8D:
        case 0xE5:
        case 0xF2:
            index += sprintf(output + index, "FAT12/FAT16");
            break;
        case 0x04:
        case 0x0E:
        case 0x1E:
        case 0x86:
        case 0x90:
        case 0x92:
        case 0x9A:
        case 0xC4:
        case 0xC6:
        case 0xD4:
        case 0xD6:
            index += sprintf(output + index, "FAT16");
            break;
        case 0x0B:
        case 0x0C:
        case 0x1B:
        case 0x1C:
        case 0x8B:
        case 0x8C:
        case 0x97:
        case 0x98:
        case 0xCB:
        case 0xCC:
        case 0xCE:
            index += sprintf(output + index, "FAT32");
            break;
        case 0x07:
        case 0x17:
        case 0x27:
        case 0x87:
            index += sprintf(output + index, "NTFS");
            break;
        case 0xA8:
        case 0xAB:
        case 0xAF:
            index += sprintf(output + index, "Apple");
            break;
        default:
            index += sprintf(output + index, "Unknown");
            break;
    }
    return index;
}
        
int print_geometry(int disk_fd, unsigned char flags, char *output) {
    int error = 0;
    struct hd_geometry disk_geometry;
    unsigned long disk_blocks = 0;
    int index = 0;

    /* Get the geometry */
    /* Note: disk_geometry.cylinders is not accurate, so cylinders must be calculated */
    error = ioctl(disk_fd, HDIO_GETGEO, &disk_geometry);

    if(error == -1) {
        index += sprintf(output + index, "Error: Could not access geometry data.\n");
    }

    /* Get the number of blocks */
    error = ioctl(disk_fd, BLKGETSIZE, &disk_blocks);

    if(error == -1) {
        index += sprintf(output + index, "Error: Could not access the number of blocks.\n");
    }

    /* Print results */
    if(flags & PRINT_CHS) {
        index += sprintf(output + index, "Heads:     %i\n", disk_geometry.heads);
        index += sprintf(output + index, "Sectors:   %i\n", disk_geometry.sectors);
        index += sprintf(output + index, "Cylinders: %lu\n", disk_blocks / (disk_geometry.heads * disk_geometry.sectors));
    }
    index += sprintf(output + index, "Capacity: %.2f GB\n", disk_blocks * 512.0 / 1000000000.0);

    return index;
}

/* Print a list of extended partitions */
/* Note: Only follows linked lists using the LBA system. 
 * The obsolite CHS addressing is not used when looking for the next EBR */
int print_extended(int disk_fd, unsigned char flags, unsigned int address, char *output) {
    int error = 0;
    unsigned int ebr_start = address;
    unsigned char this_ebr[512]; /* A buffer for the current EBR sector */
    int index = 0;

    do {
        error = pread(disk_fd, this_ebr, 512, 512 * address);

        if(error == -1) {
            index += sprintf(output + index, "Error: Could not read EBR.\n");
        }

        index += sprintf(output + index, "\n    Extended partition record at LBA %u:\n", address);

        /* Check for signature */
        index += sprintf(output + index, "        EBR Signature: %p : %p. ", (unsigned char*) this_ebr[0x01FE], (unsigned char*) this_ebr[0x01FF]);
        if(this_ebr[0x01FE] == 0x55 && this_ebr[0x01FF] == 0xAA) {
            index += sprintf(output + index, "(Valid)\n");
        }
        else {
            index += sprintf(output + index, "(Invalid)\n");
            return index; /* No point trying to read a corrupt entry */
        }

        /* Status */
        switch(this_ebr[0x01BE]) {
            case 0x80:
                index += sprintf(output + index, "        Status: Bootable.\n");
                break;
            case 0x00:
                index += sprintf(output + index, "        Status: Not bootable.\n");
                break;
            default:
                index += sprintf(output + index, "        Status: Invalid.\n");
                break;
        }

        /* Partition Type */
        index += sprintf(output + index, "        Type: %p. (", (unsigned char*) this_ebr[0x01C2]);
        index += print_type(this_ebr[0x01C2], output + index);
        index += sprintf(output + index, ")\n");
        
        /* Do we want to print the obsolite format? */
        /* Perhaps add a function so that it can be printed if it is wanted */

        /* LBA Address */
        index += sprintf(output + index, "        Position on disk (LBA): %i\n", ebr_start + read_uint32(this_ebr, 0x01C6));

        /* Total sectors in partition */
        index += sprintf(output + index, "        Total sectors: %u\n", read_uint32(this_ebr, 0x01CA));
        index += sprintf(output + index, "        Total size: %.2f GB\n", read_uint32(this_ebr, 0x01CA)
                                        * 512.0 / 1000000000.0);

        address = read_uint32(this_ebr, 0x01D6);
        address = address ? address + ebr_start : 0x00;

    } while(address);
}

int print_primary(int disk_fd, unsigned char flags, char *output) {
    int error = 0;
    int partition = 0;
    unsigned char disk_mbr[512];   /* A buffer to store the disk MBR */
    int index = 0;

    /* First, we should read the master boot record into our buffer */
    error = pread(disk_fd, disk_mbr, 512, 0);

    if(error == -1) {
        index += sprintf(output + index, "Error: Could not read disk MBR.\n");
        return index;
    }

    
    /* Check for MBR signature */
    index += sprintf(output + index, "MBR Signature: %p : %p. ", (unsigned char*) disk_mbr[0x01FE], (unsigned char*) disk_mbr[0x01FF]);
    if(disk_mbr[0x01FE] == 0x55 && disk_mbr[0x01FF] == 0xAA) {
        index += sprintf(output + index, "(Valid)\n");
    }
    else {
        index += sprintf(output + index, "(Invalid)\n");
    }
    
    /* Look at the four primary partitions */
    for(partition = 0x01BE; partition < 0x01FE; partition += 0x10) {
        if(disk_mbr[partition + 0x04]) { /* If the partition type is not null */
            index += sprintf(output + index, "\nPrimary partition record at MBR[%p]:\n", (unsigned char) partition);
        
            /* Status */
            switch(disk_mbr[partition]) {
                case 0x80:
                    index += sprintf(output + index, "    Status: Bootable.\n");
                    break;
                case 0x00:
                    index += sprintf(output + index, "    Status: Not bootable.\n");
                    break;
                default:
                    index += sprintf(output + index, "    Status: Invalid.\n");
                    break;
            }

            /* Partition Type */
            index += sprintf(output + index, "    Type: %p. (", (unsigned char*) disk_mbr[partition + 0x04]);
            index += print_type(disk_mbr[partition + 0x04], output + index);
            index += sprintf(output + index, ")\n");
        
            /* Do we want to print the obsolite format? */
            if(flags & PRINT_CHS) {
                /* CHS Partition Start */
                index += sprintf(output + index, "    Start point: Cylinder %i, Head %i, Sector %i.\n", 
                    disk_mbr[partition + 0x03] + ((disk_mbr[partition + 0x02] & 0xC0) << 2),
                    disk_mbr[partition + 0x01], 
                    disk_mbr[partition + 0x02] & 0x3F);

                /* CHS Partition End */
                index += sprintf(output + index, "    End point: Cylinder %i, Head %i, Sector %i.\n", 
                    disk_mbr[partition + 0x07] + ((disk_mbr[partition + 0x06] & 0xC0) << 2), /* Is this correct? Look in to little endian */
                    disk_mbr[partition + 0x05], 
                    disk_mbr[partition + 0x06] & 0x3F);
            }

            /* LBA Address */
            index += sprintf(output + index, "    Position on disk (LBA): %i\n", read_uint32(disk_mbr, partition + 0x08));

            /* Total sectors in partition */
            index += sprintf(output + index, "    Total sectors: %u\n", read_uint32(disk_mbr, partition + 0x0C));
            index += sprintf(output + index, "    Total size: %.2f GB\n", read_uint32(disk_mbr, partition + 0x0C)
                                            * 512.0 / 1000000000.0);
            switch(disk_mbr[partition + 0x04]) { /* If we have an extended partition */
                case 0x05:
                case 0x0F:
                case 0x15:
                case 0x1F:
                case 0x85:
                case 0x91:
                case 0x9B:
                case 0xC5:
                case 0xCF:
                case 0xD5:
                    index += print_extended(disk_fd, flags, read_uint32(disk_mbr, partition + 0x08), output + index);
                    break;
                default:
                    break;
            }
        }
    }
}    

/*
int main(int argc, char **argv) {
    
    int disk_fd = 0;       
    int error = 0;         

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <device>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Open the device
    disk_fd = open(argv[1], O_RDONLY | O_NONBLOCK);

    if(disk_fd == -1) {
        fprintf(stderr, "Error: Could not open %s.\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    // Get the geometry 
    print_geometry(disk_fd, PRINT_CHS);

    // Time to look at the primary partition table 
    print_primary(disk_fd, NONE);

    printf("\n");
    return EXIT_SUCCESS;
}
*/
