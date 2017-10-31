/*
    Generates data frames at every MCS value across 20 and 40mhz
    for testing tx and rx.

    Allows tagging packets with location info for site surveys.

    Packets are sent as beacons encoded at different MCS rates; 
    beacons SHOULD bypass the triggering of CTS/RTS frame control.

    Packets are sent with 2 custom IE tags:

    ie 10, length 14
        Packed byte field encoding:

            Byte 0 - MCS rate and flags, where
                Bit 7 indicates HT40 mode
                Bit 6 indicates ShortGI mode
                Bits 0-3 indicate MCS values 0-15
            Byte 1 - Location code (0-255)
            Bytes 2-5
                32 bit current packet count
            Bytes 6-9
                32 bit maximum packet count
            Bytes 10-13
                32 bit random session id

    ie 11, length 64
        Text field containing a text description of the packed field
*/

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <lorcon2/lorcon.h>
#include <lorcon2/lorcon_forge.h>
#include <lorcon2/lorcon_packasm.h>

/* MCS only goes 0-15 or 4 bits, so we use bits 6 and 7 to indicate if we
 * are sending HT40 and GI */
#define HT_FLAG_40  (1 << 7)
#define HT_FLAG_GI  (1 << 6)

#define PAYLOAD_LEN 64

void usage(char *argv[]) {
    printf("\t-i <interface>        Radio interface\n");
    printf("\t-c <channel>          Channel (should be HT40)\n");
    printf("\t-m <MCS_index>        MCS index (0~15)\n");
    printf("\t-b <band_width>       Band width(0-20M | 1-40M)\n");
    printf("\t-g <guard_interval>   Guard interval\n");
    printf("\t-n <count>            Number of packets at each MCS to send\n");
    printf("\t-d <delay>            Interframe delay\n");

    printf("\nExample:\n");
    printf("\t%s -i wlan0 -c 6HT40+ -m 0 -b 0 -g 0 -n 1000\n\n", argv[0]);
}
int main(int argc, char *argv[]) {
    char *interface = NULL;
    unsigned int lcode = 0;
    unsigned int npackets = 100;
    unsigned int MCS = 0;

    int value[6];
    int c,i,tmp;
    int channel, ch_flags;

    lorcon_driver_t *drvlist, *driver;
    lorcon_t *context;

    lcpa_metapack_t *metapack;
    lorcon_packet_t *txpack;

    /* delay interval */
    unsigned int interval = 1;

    /* Iterations through HT and GI */
//    int mcs_iter = 0;
    int BW = 0;
    int GI = 0; 
//    int ht_iter = 0;
//    int gi_iter = 0;
    unsigned int count = 0;

    unsigned int totalcount = 1;

    uint8_t *dmac = "\x04\xF0\x21\x32\xBD\xA5";

    uint8_t *bmac = "\x00\xDE\xAD\xBE\xEF\x00";

//    uint8_t *RA_MAC = "\x04\xF0\x21\x32\xBD\xA5";
    uint8_t RA_MAC[6];
    RA_MAC[0] = 0x04;
    RA_MAC[1] =0xF0;
    RA_MAC[2] =0x21;
    RA_MAC[3] =0x32;
    RA_MAC[4] =0xBD;
    RA_MAC[5] =0xA5;
    uint8_t *TA_MAC;
    uint8_t *DA_MAC = RA_MAC;
    uint8_t *BSSID_MAC = bmac;

    uint8_t fcflags = 3;
    uint8_t fragement = 3;
    uint8_t sequence = 2;
    unsigned int duration = 0;
    uint8_t encoded_payload[14];
    uint32_t *encoded_counter = (uint32_t *) (encoded_payload + 2);
    uint32_t *encoded_max = (uint32_t *) (encoded_payload + 6);
    uint32_t *encoded_session = (uint32_t *) (encoded_payload + 10);

    uint8_t payload[2*PAYLOAD_LEN];
    uint8_t payload_1[PAYLOAD_LEN];

    // Timestamp
    struct timeval time; 
    uint64_t timestamp; 

    // Beacon Interval
    int beacon_interval = 100;

    // Capabilities
    int capabilities = 0x0421;

    // Session ID
    uint32_t session_id;
    FILE *urandom;

    printf ("%s - packet injector NEW!\n", argv[0]);
    printf ("-----------------------------------------------------\n\n");

    while ((c = getopt(argc, argv, "hi:c:m:b:g:n:d:a:")) != EOF) {
	switch (c) {
	case 'i': 
		interface = strdup(optarg);
		break;
	case 'c':
		if (lorcon_parse_ht_channel(optarg, &channel, &ch_flags) == 0) {
	    		printf("ERROR: Unable to parse channel\n");
	    		return -1;
		}
		break;
	case 'm':
		if (sscanf(optarg, "%u", &MCS) != 1){
		    printf("ERROR: Unable to parse MCS idex\n");
		    return -1;
		}
		break;
    	case 'b':
		if (sscanf(optarg, "%u", &BW) != 1){
		    printf("ERROR: Unable to parse bandwidth \n");
		    return -1;
		}
		break;
	case 'g':
		if (sscanf(optarg, "%u", &GI) != 1){
		    printf("ERROR: Unable to parse guard interval \n");
		    return -1;
		}
		break;
    	case 'n':
		if (sscanf(optarg, "%u", &npackets) != 1) {
		    printf("ERROR: Unable to parse number of packets\n");
		    return -1;
		}
		break;

    	case 'd':
		if (sscanf(optarg, "%u", &interval) != 1) {
		    printf("ERROR: Unable to parse interframe interval\n");
		    return -1;
		}
		break;
	case 'a':
		//if (6 == sscanf(optarg, "%x:%x:%x:%x:%x:%x", &value[0],&value[1],&value[2],&value[3],&value[4],&value[5]) ){
		tmp = sscanf(optarg, "%x:%x:%x:%x:%x:%x", &value[0],&value[1],&value[2],&value[3],&value[4],&value[5]);
		
		printf("Read MAC, num:%d\n",tmp);
		if (6 == tmp ){
			printf("Read MAC, entering loop\n");
			for(i = 0; i < 6; i++){
				printf("Read MAC, loop index:%d\n",i);
				RA_MAC[i] = (uint8_t)value[i];	
			}
		}else{
		    printf("ERROR: Unable to parse MAC address\n");
		    return -1;
		}
		break;	
	case 'h':
		printf("ERROR: cannot parse the input\n");
		usage(argv);
		return -1;
		break;

	default:
		usage(argv);
		return -1;
		break;
	}
    }

    if ( interface == NULL) { 
	printf ("ERROR: Interface, or channel not set (see injector -h for more info)\n");
	return -1;
    }

    if ((urandom = fopen("/dev/urandom", "rb")) == NULL) {
        printf("ERROR:  Could not open urandom for session id: %s\n", strerror(errno));
        return -1;
    }

    fread(&session_id, 4, 1, urandom);
    fclose(urandom);

    printf("[+] Using interface %s\n",interface);
	
    if ((driver = lorcon_auto_driver(interface)) == NULL) {
	printf("[!] Could not determine the driver for %s\n", interface);
	return -1;
    } else {
	printf("[+]\t Driver: %s\n",driver->name);
    }

    if ((context = lorcon_create(interface, driver)) == NULL) {
        printf("[!]\t Failed to create context");
        return -1; 
    }

    // Create Monitor Mode Interface
    if (lorcon_open_injmon(context) < 0) {
	printf("[!]\t Could not create Monitor Mode interface!\n");
	return -1;
    } else {
	printf("[+]\t Monitor Mode VAP: %s\n\n",lorcon_get_vap(context));
	lorcon_free_driver_list(driver);
    }

    // Get the MAC of the radio
    if (lorcon_get_hwmac(context, &TA_MAC) <= 0) {
        printf("[!]\t Could not get hw mac address\n");
        return -1;
    }

    printf("[+]\t Using MAC: %02x:%02x:%02x:%02x:%02x:%02x \n",TA_MAC[0],TA_MAC[1],TA_MAC[2],TA_MAC[3],TA_MAC[4],TA_MAC[5]);
    printf("[+]\t RX MAC: %02x:%02x:%02x:%02x:%02x:%02x \n",RA_MAC[0],RA_MAC[1],RA_MAC[2],RA_MAC[3],RA_MAC[4],RA_MAC[5]);
    // Set the channel we'll be injecting on
    lorcon_set_ht_channel(context, channel, ch_flags);

    printf("[+]\t Using channel: %d flags %d\n", channel, ch_flags);

    
    printf("\n[.]\tMCS %u %s %s\n\n", MCS, BW ? "40MHz" : "20MHz", GI ? "short-gi" : "long-gi");

    for (count = 0; count < npackets; count++) {
        memset(payload, 0, 2*PAYLOAD_LEN);
        memset(payload_1, 0, PAYLOAD_LEN);
	for (i = 0; i < PAYLOAD_LEN; i++){
		payload[2*i] = count & 0x00FF;
		payload[2*i+1] = (count & 0xFF00) >> 8;
	}
        memset(encoded_payload, 0, 14);

        // Set MCS count
        encoded_payload[0] = MCS;
        if (GI)
            encoded_payload[0] |= HT_FLAG_GI;
        if (BW)
            encoded_payload[0] |= HT_FLAG_40;

        // set the location code
        encoded_payload[1] = lcode & 0xFF;

        *encoded_counter = htonl(count);
        *encoded_max = htonl(npackets);
        *encoded_session = htonl(session_id);

	
        snprintf((char *) payload_1, PAYLOAD_LEN, "MCS %u %s%s Packet %u of %u",
                    MCS,
                    BW ? "40MHz" : "20MHz",
                    GI ? " short-gi": "",
                    count,
                    npackets);


        metapack = lcpa_init();

        // Create timestamp
        gettimeofday(&time, NULL);
        timestamp = time.tv_sec * 1000000 + time.tv_usec;

        lcpf_data(metapack,fcflags,duration,RA_MAC,TA_MAC,RA_MAC,TA_MAC,fragement,sequence);


        lcpf_add_ie(metapack, 0, strlen("Packet_Injection"), "Packet_Injection");
        lcpf_add_ie(metapack, 10, 14, encoded_payload);
        lcpf_add_ie(metapack, 11, 2*PAYLOAD_LEN, payload);
        lcpf_add_ie(metapack, 12, strlen((char *) payload_1), payload_1);


        // Convert the LORCON metapack to a LORCON packet for sending
        txpack = (lorcon_packet_t *) lorcon_packet_from_lcpa(context, metapack);

        lorcon_packet_set_mcs(txpack, 1, MCS, GI, BW);
		
        if (lorcon_inject(context,txpack) < 0 ) 
            return -1;

        usleep(interval * 1000);

        printf("\033[K\r");
        printf("[+] Sent %d frames, Hit CTRL + C to stop...", totalcount);
        fflush(stdout);
        totalcount++;

        lcpa_free(metapack); 
    }

    printf("\n");
    // Close the interface
    lorcon_close(context);
    // Free the LORCON Context
    lorcon_free(context);	
	
    return 0;
}

