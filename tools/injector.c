#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <lorcon2/lorcon.h>
#include <lorcon2/lorcon_packet.h>

void usage()
{
	lorcon_driver_t *drvlist, *dri;
	printf("Usage: injector [options]\n"
		"  -i <interface> specify the interface name\n"
		"  -n <number>          number of packets to send\n"
  	        "  -c <channel>         channel to transmit packets on.\n"
		"  -s <sleep>           sleep time in usec between packets.\n"
		"  -d <drivername>      string indicating driver used on interface\n");
	dri = drvlist = lorcon_list_drivers();
	printf("Supported LORCON drivers:\n");
	while (dri)
	{
		printf("%-10.10s %s\n", dri->name, dri->details);
		dri = dri->next;
	}
	lorcon_free_driver_list(drvlist);
}
int main(int argc, char *argv[]){
	unsigned char packet[108] = {
	0x08, 0x41, 0x0a, 0x00, 0x00, 0x03, 0x1b, 0xc2,
        0x45, 0x33, 0x00, 0x1b, 0x4b, 0x29, 0x61, 0xb1,
	0xff, 0x10, 0x07, 0x00, 0x12, 0x53, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00,
 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
	0x00, 0x00, 0x75, 0x41, 0x37, 0x5a, 0x4b, 0xbc,
	0x55, 0x69, 0x07, 0x58, 0x4c, 0x03, 0xf4, 0xa7,
	0x69, 0xbc, 0xdf, 0x46, 0x27, 0x4d, 0xd0, 0xb6,
	0xcc, 0x7c, 0x8b, 0x8b, 0x46, 0x06, 0x30, 0x72,
	0x67, 0x72, 0x5d, 0x49, 0xe6, 0x0a, 0xfb, 0x74,
	0xef, 0x59, 0x1c, 0x24, 0x0b, 0x07, 0x60, 0xee,
	0x1b, 0x87, 0xf1, 0x6f, 0x3a, 0x88, 0x54, 0x25,
	0x5a, 0x90, 0xb4, 0x68
	};
	lorcon_driver_t *drvlist, *dri;
	char *driver = NULL, *interface = NULL;
	int cnt = 1, delay = 0, ret = 0, c = 0, channel = 0, txcnt = 0;
	lorcon_t *ctx;

	while ((c = getopt(argc, argv, "n:i:d:c:s:")) != EOF) {
		switch (c) {
		case 's':
			if (sscanf(optarg, "%d", &delay) != 1) {
				fprintf(stderr, "%s: Illegal delay on cmd line\n", argv[0]);
				usage();
				return -1;
			}
			break;
		case 'n':
			if (sscanf(optarg, "%d", &cnt) != 1) {
				fprintf(stderr, "%s: Illegal count on cmd line.\n", argv[0]);
				usage();
				return -1;
			}
			break;
		case 'i':
			interface = strdup(optarg);
			break;
		case 'd':
			driver = strdup(optarg);
			break;
		case 'c':
			if (sscanf(optarg, "%d", &channel) != 1) {
				fprintf(stderr, "%s: Illegal channel on cmd line.\n", argv[0]);
				usage();
				return -1;
			}
			break;
		default:
			break;
		}
	}
	if (interface == NULL) {
		fprintf(stderr, "Must specify an interface\n");
		usage();
		return -1;
	}
	if (driver != NULL) {
		dri = lorcon_find_driver(driver);
		if (dri == NULL) {
			fprintf(stderr, "Couldn't find driver %s for %s\n", driver,interface);
			usage();
			return -1;
		}
	} else {
		dri = lorcon_auto_driver(interface);
		if (dri == NULL) {
			fprintf(stderr, "Couldn't detect driver or no supported driver for %s\n", interface);
			return -1;
		}
		printf("Detected driver %s for %s\n", dri->name, interface);
	}
}
