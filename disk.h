#define PRINT_CHS 0x01
#define NONE 0x00

/* Print the disk geometry */
int print_geometry(int disk_fd, unsigned char flags, char *output);

/* Print all primary and extended partition tables on the disk */
int print_primary(int disk_fd, unsigned char flags, char *output);
