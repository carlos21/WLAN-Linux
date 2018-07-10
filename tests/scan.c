//
//  scan.c
//  WLAN Linux
//
//  Created by Carlos Duclos on 7/7/18.
//

#include "scan.h"
#include <stdio.h>
#include <time.h>
#include <iwlib.h>

int main(void) {
    wireless_scan_head head;
    wireless_scan *result;
    iwrange range;
    int sock;
    
    /* Open socket to kernel */
    sock = iw_sockets_open();
    
    /* Get some metadata to use for scanning */
    if (iw_get_range_info(sock, "wlan0", &range) < 0) {
        printf("Error during iw_get_range_info. Aborting.\n");
        exit(2);
    }
    
    /* Perform the scan */
    if (iw_scan(sock, "wlan0", range.we_version_compiled, &head) < 0) {
        printf("Error during iw_scan. Aborting.\n");
        exit(2);
    }
    
    /* Traverse the results */
    result = head.result;
    while (NULL != result) {
        printf("%s\n", result->b.essid);
        result = result->next;
    }
    
    exit(0);
}
