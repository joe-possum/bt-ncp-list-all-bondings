/***************************************************************************//**
 * @file
 * @brief Event handling and application code for Empty NCP Host application example
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/* standard library headers */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

/* BG stack headers */
#include "bg_types.h"
#include "gecko_bglib.h"

/* Own header */
#include "app.h"
#include "dump.h"
#include "support.h"

// App booted flag
static bool appBooted = false;
static struct {
  char *name;
  uint32 advertising_interval;
  uint16 connection_interval, mtu; 
  bd_addr remote;
  uint8 advertise, connection;
} config = { .remote = { .addr = {0,0,0,0,0,0}},
	     .connection = 0xff,
	     .advertise = 1,
	     .name = NULL,
	     .advertising_interval = 160, // 100 ms
	     .connection_interval = 80, // 100 ms
	     .mtu = 23,
};

#define MAX_BONDS 128
static struct bonding {
  bd_addr address;
  uint8 present, type, index;
} bondings[MAX_BONDS];
uint8 bonding_count = 0;

void parse_address(const char *fmt,bd_addr *address) {
  char buf[3];
  int octet;
  for(uint8 i = 0; i < 6; i++) {
    memcpy(buf,&fmt[3*i],2);
    buf[2] = 0;
    sscanf(buf,"%02x",&octet);
    address->addr[5-i] = octet;
  }
}

uint8 ad_flags(uint8 *buffer, uint flags) {
  if(0 == flags) return 0;
  buffer[0] = 2;
  buffer[1] = 1;
  buffer[2] = flags;
  return 3;
}

uint8 ad_name(uint8 *buffer, char *name) {
  uint8 len = strlen(name);
  buffer[0] = len + 1;
  buffer[1] = 9;
  memcpy(&buffer[2],(uint8*)&name[0],len);
  return len + 2;
}

uint8 ad_manufacturer(uint8 *buffer, uint8 *data, uint8 len) {
  buffer[0] = len + 3;
  buffer[1] = 0xff;
  buffer[2] = 0xff;
  buffer[3] = 0xff;
  memcpy(&buffer[4],data,len);
  return len + 4;
}

const char *getAppOptions(void) {
  return "a<remote-address>n<name>";
}

void appOption(int option, const char *arg) {
  double dv;
  switch(option) {
  case 'a':
    parse_address(arg,&config.remote);
    config.advertise = 0;
    break;
  case 'i':
    sscanf(arg,"%lf",&dv);
    config.advertising_interval = round(dv/0.625);
    config.connection_interval = round(dv/1.25);
    break;
  case 'n':
    config.name = strdup(arg);
    break;
  default:
    fprintf(stderr,"Unhandled option '-%c'\n",option);
    exit(1);
  }
}

void appInit(void) {
  memset(bondings,0,sizeof(bondings));
}

/***********************************************************************************************//**
 *  \brief  Event handler function.
 *  \param[in] evt Event pointer.
 **************************************************************************************************/
void appHandleEvents(struct gecko_cmd_packet *evt)
{
  if (NULL == evt) {
    return;
  }

  // Do not handle any events until system is booted up properly.
  if ((BGLIB_MSG_ID(evt->header) != gecko_evt_system_boot_id)
      && !appBooted) {
#if defined(DEBUG)
    printf("Event: 0x%04x\n", BGLIB_MSG_ID(evt->header));
#endif
    millisleep(50);
    return;
  }

  /* Handle events */
#ifdef DUMP
  switch (BGLIB_MSG_ID(evt->header)) {
  default:
    dump_event(evt);
  }
#endif
  switch (BGLIB_MSG_ID(evt->header)) {
  case gecko_evt_system_boot_id:
    appBooted = true;
    gecko_cmd_sm_list_all_bondings();
    gecko_cmd_hardware_set_soft_timer(10,0,1);
    break;

  case gecko_evt_sm_list_bonding_entry_id: /*************************************************************** sm_list_bonding_entry **/
#define ED evt->data.evt_sm_list_bonding_entry
    bondings[ED.bonding].present = 1;
    bondings[ED.bonding].type = ED.address_type;
    memcpy(&bondings[ED.bonding].address.addr[0],&ED.address.addr[0],6);
    bonding_count++;
    gecko_cmd_hardware_set_soft_timer(10,0,1);
    break;
#undef ED

  case gecko_evt_sm_list_all_bondings_complete_id: /*********************************************** sm_list_all_bondings_complete **/
#define ED evt->data.evt_sm_list_all_bondings_complete
    if(!bonding_count) {
      printf("No bondings\n");
    } else {
      printf("%d bondings:\n", bonding_count);
      printf("Bonding     Bluetooth     Address\n");
      printf(" Index       Address       Type  \n");
      printf("======= ================= =======\n");
      for(uint8 bonding = 0; bonding < MAX_BONDS; bonding++) {
	if(bondings[bonding].present) printf("   %2x   %02x:%02x:%02x:%02x:%02x:%02x   %02x\n", bonding,
					     bondings[bonding].address.addr[5],
					     bondings[bonding].address.addr[4],
					     bondings[bonding].address.addr[3],
					     bondings[bonding].address.addr[2],
					     bondings[bonding].address.addr[1],
					     bondings[bonding].address.addr[0],
					     bondings[bonding].type);
      }
    }
    gecko_cmd_hardware_set_soft_timer(0,0,1);
    gecko_cmd_system_reset(0);
    exit(0);
    break;
#undef ED

  case gecko_evt_hardware_soft_timer_id: /******************************************************************* hardware_soft_timer **/
#define ED evt->data.evt_hardware_soft_timer
    printf("ERROR: timeout\n");
    gecko_cmd_system_reset(0);
    exit(1);
    break;
#undef ED

  default:
    break;
  }
}
