/*
*  license and disclaimer for the use of this source code as per statement below
*  Lizenz und Haftungsausschluss f�r die Verwendung dieses Sourcecodes siehe unten
*/

#include "os.h"
#include "ipTcpStack.h"

// This is (normally) called from a Network-Card-Driver
// It interprets the whole data (also the Ethernet-Header)

// Parameters:
//  Data  : A pointer to the Data
//  Length: The Size of the Paket

// Internal:
//  TODO

void ipTcpStack_recv(void* Data, uint32_t Length)
{
  struct ethernet* eth;
  struct arp*      arp;
  struct ip*       ip;

  // first we cast our Data pointer into a pointer at our Ethernet-Frame
  eth = (struct ethernet*)Data;
  settextcolor(14,0); printformat("--- TCP-IP stack ---\n"); settextcolor(15,0);

  // we dump the Data
  settextcolor(3,0);
  for(uint32_t c = 0; c < Length; c++ )
  {
    printformat("%y ", ((char*)Data)[c]);
  }

  // we leave the transmitter-mac-addr and receiver-mac-addr unchanged
  // DEBUG < we just print them >
  settextcolor(14,0); printformat("\n\nEthernet header:\n"); settextcolor(15,0);
  settextcolor(13,0); printformat("MAC Transmitter: "); settextcolor(3,0);
  printformat("%y-%y-%y-%y-%y-%y\n", eth->send_mac[0], eth->send_mac[1], eth->send_mac[2], eth->send_mac[3], eth->send_mac[4], eth->send_mac[5]);
  settextcolor(13,0); printformat("MAC Receiver:    "); settextcolor(3,0);
  printformat("%y-%y-%y-%y-%y-%y\n", eth->recv_mac[0], eth->recv_mac[1], eth->recv_mac[2], eth->recv_mac[3], eth->recv_mac[4], eth->recv_mac[5]);
  settextcolor(15,0);

  // now we check if it is Ethernet 1 or Ethernet 2
  // (but we just throw it away, because we can read the length of the data from the other Layers)
  if( ( (eth->type_len[0] << 8) | eth->type_len[1] ) > 1500 )
  {
    settextcolor(14,0); printformat("Ethernet 2 Packet.\n"); settextcolor(15,0);
  }
  else
  {
    settextcolor(14,0); printformat("Ethernet 1 Packet.\n"); settextcolor(15,0);
  }

  // now we set our arp/ip pointer to the Ethernet-payload
  arp = (struct arp*)( (unsigned long)eth + sizeof(struct ethernet) );
  ip  = (struct ip* )( (unsigned long)eth + sizeof(struct ethernet) );

  // to decide if it is an ip or an arp paket we just look at the ip-version
  if( (ip->version_ihl >> 4) == 4 )
  {
    settextcolor(14,0); printformat("IPv4 Packet.\n"); settextcolor(15,0);
  }
  else if( (ip->version_ihl >> 4) == 6 )
  {
    settextcolor(14,0); printformat("IPv6 Packet.\n"); settextcolor(15,0);
  }
  else
  {
    // we decide _now_ that it could be an arp paket
    // ASK < any other ideas to test for the type of the protocol? >

    // now we check if it is really an ipv4 ARP paket
    if( ( ( (arp->hardware_addresstype[0] << 8) | arp->hardware_addresstype[1] ) ==    1 ) &&
        ( ( (arp->protocol_addresstype[0] << 8) | arp->protocol_addresstype[1] ) == 2048 ) &&
        ( arp->hardware_addresssize                                              ==    6 ) &&
        ( arp->protocol_addresssize                                              ==    4 )
      )
      {
        settextcolor(14,0); printformat("ARP Paket.\n"); settextcolor(15,0);

        // extract the operation
        uint16_t operation = (arp->operation[0] << 8) | arp->operation[1];

        // print the operation
        if( operation == 1 ) // it is an ARP-Request
        {
            settextcolor(14,0); printformat("Operation: Request\n"); settextcolor(15,0);
        }
        else if( operation == 2 ) // it is an ARP-Response
        {
            settextcolor(14,0); printformat("Operation: Response\n"); settextcolor(15,0);
        }
      }
      else
      {
          // NOTE < here we ignore silently other packets that we don't know >
      }
  }
  settextcolor(14,0); printformat("--- TCP-IP stack ---\n"); settextcolor(15,0);
}

/*
* Copyright (c) 2009 The PrettyOS Project. All rights reserved.
*
* http://www.c-plusplus.de/forum/viewforum-var-f-is-62.html
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/