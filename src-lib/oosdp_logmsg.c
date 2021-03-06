/*

  oosdp_logmsg.c - prints log messages

  (C)Copyright 2017-2019 Smithee Solutions LLC
*/
#define OSDP_CMD_MSC_GETPIV  (0x10)
#define OSDP_CMD_MSC_KP_ACT  (0x13)
#define OSDP_CMD_MSC_CR_AUTH (0x14)
#define OSDP_REP_MSC_PIVDATA (0x10)
#define OSDP_REP_MSC_CR_AUTH (0x14)
#define OSDP_REP_MSC_STAT    (0xFD)

char OSDP_VENDOR_INID [] = { 0x00, 0x75, 0x32 };
char OSDP_VENDOR_WAVELYNX [] = { 0x5C, 0x26, 0x23 };

typedef struct __attribute__((packed)) osdp_msc_crauth
{
  char vendor_code [3];
  char command_id;
  unsigned short int mpd_size_total;
  unsigned short int mpd_offset;
  unsigned short int mpd_fragment_size;
  unsigned char data [2]; // just first 2 of data.  algref and keyref in first block
} OSDP_MSC_CR_AUTH;

typedef struct __attribute__((packed)) osdp_msc_crauth_response
{
  char vendor_code [3];
  char command_id;
  unsigned short int mpd_size_total;
  unsigned short int mpd_offset;
  unsigned short int mpd_fragment_size;
  unsigned char data;
} OSDP_MSC_CR_AUTH_RESPONSE;

typedef struct __attribute__((packed)) osdp_msc_getpiv
{
  char vendor_code [3];
  char command_id;
  char piv_object [3];
  char piv_element;
  char piv_offset [2];
} OSDP_MSC_GETPIV;
  
typedef struct __attribute__((packed)) osdp_msc_kp_act
{
  char vendor_code [3];
  char command_id;
  unsigned short int kp_act_time;
} OSDP_MSC_KP_ACT;
  
typedef struct __attribute__((packed)) osdp_msc_piv_data
{
  char vendor_code [3];
  char command_id;
  unsigned short int mpd_size_total;
  unsigned short int mpd_offset;
  unsigned short int mpd_fragment_size;
  unsigned char data;
} OSDP_MSC_PIV_DATA;

typedef struct __attribute__((packed)) osdp_msc_status
{
  char vendor_code [3];
  char command_id;
  char status;
  char info [2];
} OSDP_MSC_STATUS;

/*
  oosdp-logmsg - open osdp log message routines

  (C)Copyright 2014-2017 Smithee,Spelvin,Agnew & Plinge, Inc.

  Support provided by the Security Industry Association
  http://www.securityindustry.org

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
 
    http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#include <stdio.h>
#include <time.h>
#include <string.h>


#include <osdp-tls.h>
#include <open-osdp.h>

extern OSDP_CONTEXT context;
extern OSDP_PARAMETERS p_card;
extern char trace_in_buffer [];
extern char trace_out_buffer [];


/*
  oosdp_make_message - construct useful log text for output

  used for monitor mode and logging
*/
int
  oosdp_make_message
    (int msgtype,
    char *logmsg,
    void *aux)
    
{ /* oosdp_make_message */

  OSDP_SC_CCRYPT *ccrypt_payload;
  int count;
  OSDP_MSC_CR_AUTH *cr_auth;
  OSDP_MSC_CR_AUTH_RESPONSE *cr_auth_response;
  int d;
  OSDP_HDR_FILETRANSFER *filetransfer_message;
  OSDP_HDR_FTSTAT *ftstat;
  OSDP_MSC_GETPIV *get_piv;
  OSDP_HDR *hdr;
  char hstr [1024];
  int i;
  int idx;
  OSDP_MSC_STATUS *msc_status;
  OSDP_MSG *msg;
  unsigned short int newdelay;
  unsigned short int newmax;
  OSDP_HDR *oh;
  unsigned char osdp_command;
  OSDP_HDR *osdp_wire_message;
  OSDP_MSC_PIV_DATA *piv_data;
  int scb_present;
  char *sec_block;
  char tlogmsg [3*1024];
  char tmps [1024];
  char tmpstr [1024];
  char tmpstr2 [2*1024];
  int status;
  unsigned short int ustmp; // throw-away unsigned short integer (fits a "doubleByte")
  unsigned int utmp; // throw-away unsigned integer (fits a "quadByte")


  status = ST_OK;
  msg = NULL;
  oh = NULL;
  memset(hstr, 0, sizeof(hstr));

  // set up the OSDP header structure (if we have something to work with)
  if (aux)
  {
    msg = (OSDP_MSG *) aux;
    oh = (OSDP_HDR *)(msg->ptr);

    // calculate the payload size, accounting for CRC vs. CHECKSUM
    count = oh->len_lsb + (oh->len_msb << 8);
    count = count - sizeof(*oh);
    if (oh->ctrl & 0x04)
      count = count - 2;
    else
      count = count - 1;
  };

  switch (msgtype)
  {
  case OOSDP_MSG_ACURXSIZE:
    {
      int c;

      msg = (OSDP_MSG *) aux;

      // per spec it is lsb/msb

      c = msg->data_payload [0];
      c = 256*c + msg->data_payload [1];
      sprintf(tlogmsg, "ACU Receive Size: %0x\n", c);
    };
    break;

  case OOSDP_MSG_BUZ:
    msg = (OSDP_MSG *) aux;
    if (msg->security_block_length > 0)
    {
      oh = (OSDP_HDR *)(msg->ptr);
      tlogmsg [0] = 0;
      count = oh->len_lsb + (oh->len_msb << 8);
      count = count - 8;
      for (i=0; i<count; i++)
      {
        d = *(unsigned char *)(msg->data_payload+i);
        sprintf(tmpstr, "%02x", d);
        strcat(hstr, tmpstr);
      };
      sprintf(tlogmsg,
        "  Encrypted BUZ Payload (%d. bytes) %s\n", count, hstr);
    }
    else
    {
      sprintf(tlogmsg, "BUZ: Rdr %02x Tone Code %02x On=%d(ms) Off=%d(ms) Count %d\n",
        *(msg->data_payload + 0), *(msg->data_payload + 1),
        100 * *(msg->data_payload + 2), 100 * *(msg->data_payload + 3),
        *(msg->data_payload + 4));
    };
    break;

  case OOSDP_MSG_CCRYPT:
    msg = (OSDP_MSG *) aux;
    ccrypt_payload = (OSDP_SC_CCRYPT *)(msg->data_payload);
    sprintf (tlogmsg,
"  CCRYPT: cUID %02x%02x%02x%02x-%02x%02x%02x%02x RND.B %02x%02x%02x%02x-%02x%02x%02x%02x Client Cryptogram %02x%02x%02x%02x-%02x%02x%02x%02x %02x%02x%02x%02x-%02x%02x%02x%02x\n",
      ccrypt_payload->client_id [0], ccrypt_payload->client_id [1],
      ccrypt_payload->client_id [2], ccrypt_payload->client_id [3],
      ccrypt_payload->client_id [4], ccrypt_payload->client_id [5],
      ccrypt_payload->client_id [6], ccrypt_payload->client_id [7],
      ccrypt_payload->rnd_b [0], ccrypt_payload->rnd_b [1],
      ccrypt_payload->rnd_b [2], ccrypt_payload->rnd_b [3],
      ccrypt_payload->rnd_b [4], ccrypt_payload->rnd_b [5],
      ccrypt_payload->rnd_b [6], ccrypt_payload->rnd_b [7],
      ccrypt_payload->cryptogram [0], ccrypt_payload->cryptogram [1],
      ccrypt_payload->cryptogram [2], ccrypt_payload->cryptogram [3],
      ccrypt_payload->cryptogram [4], ccrypt_payload->cryptogram [5],
      ccrypt_payload->cryptogram [6], ccrypt_payload->cryptogram [7],
      ccrypt_payload->cryptogram [8], ccrypt_payload->cryptogram [9],
      ccrypt_payload->cryptogram [10], ccrypt_payload->cryptogram [11],
      ccrypt_payload->cryptogram [12], ccrypt_payload->cryptogram [13],
      ccrypt_payload->cryptogram [14], ccrypt_payload->cryptogram [15]);
    break;

  case OOSDP_MSG_CHLNG:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_CHLNG(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_COM:
    {
      int speed;

      msg = (OSDP_MSG *) aux;
      speed = *(1+msg->data_payload) + (*(2+msg->data_payload) << 8) +
        (*(3+msg->data_payload) << 16) + (*(4+msg->data_payload) << 24);

      sprintf(tlogmsg, "COM Returns New Address %02x New Speed %d.\n",
        *(0+msg->data_payload), speed);
    };
    break;

  case OOSDP_MSG_COMSET:
    {
      int speed;

      msg = (OSDP_MSG *) aux;
      speed = *(1+msg->data_payload) + (*(2+msg->data_payload) << 8) +
        (*(3+msg->data_payload) << 16) + (*(4+msg->data_payload) << 24);

      sprintf(tlogmsg, "COMSET Will use New Address %02x New Speed %d.\n",
        *(0+msg->data_payload), speed);
    };
    break;

  case OOSDP_MSG_FILETRANSFER:
    msg = (OSDP_MSG *) aux;
    filetransfer_message = (OSDP_HDR_FILETRANSFER *)(msg->data_payload);
    tlogmsg[0] = 0;
    osdp_array_to_quadByte(filetransfer_message->FtSizeTotal, &utmp);
    sprintf(tmpstr,
"File Transfer: Type %02x\n",
      filetransfer_message->FtType);
    strcat(tlogmsg, tmpstr);
    sprintf(tmpstr,
"                           Size %02x-%02x-%02x-%02x(%d.)\n",
      filetransfer_message->FtSizeTotal [0], filetransfer_message->FtSizeTotal [1],
      filetransfer_message->FtSizeTotal [2], filetransfer_message->FtSizeTotal [3],
      utmp);
    strcat(tlogmsg, tmpstr);
    osdp_array_to_quadByte(filetransfer_message->FtOffset, &utmp);
    sprintf(tmpstr,
"                         Offset %02x%02x%02x%02x(%d.)\n",
      filetransfer_message->FtOffset [0], filetransfer_message->FtOffset [1],
      filetransfer_message->FtOffset [2], filetransfer_message->FtOffset [3],
      utmp);
    strcat(tlogmsg, tmpstr);
    osdp_array_to_doubleByte(filetransfer_message->FtFragmentSize, &ustmp);
    sprintf(tmpstr,
"                  Fragment Size %02x-%02x(%d.)\n  First data: %02x\n",
      filetransfer_message->FtFragmentSize [0], filetransfer_message->FtFragmentSize [1],
      ustmp, filetransfer_message->FtData);
    strcat(tlogmsg, tmpstr);
    sprintf(tmpstr,
"  Current Offset %8d. Total Length %8d. Current Send Length %d. Handle %lx\n",
      context.xferctx.current_offset, context.xferctx.total_length, context.xferctx.current_send_length,
      (unsigned long)(context.xferctx.xferf));
    strcat(tlogmsg, tmpstr);
    break;

  case OOSDP_MSG_FTSTAT:
    msg = (OSDP_MSG *) aux;
    ftstat = (OSDP_HDR_FTSTAT *)(msg->data_payload);
    tlogmsg[0] = 0;
    osdp_array_to_doubleByte(ftstat->FtDelay, &newdelay);
    osdp_array_to_doubleByte(ftstat->FtUpdateMsgMax, &newmax);
    sprintf(tmpstr,
"File Transfer STATUS: Detail %02x%02x Action %02x Delay %02x-%02x(%d.) Update-max %02x-%02x(%d.)\n",
      ftstat->FtStatusDetail [0], ftstat->FtStatusDetail [1],
      ftstat->FtAction,
      ftstat->FtDelay [0], ftstat->FtDelay [1], newdelay,
      ftstat->FtUpdateMsgMax [0], ftstat->FtUpdateMsgMax [1], newmax);
    strcat(tlogmsg, tmpstr);
    break;

  case OOSDP_MSG_ISTATR:
    msg = (OSDP_MSG *) aux;
    oh = (OSDP_HDR *)(msg->ptr);
    tlogmsg [0] = 0;
    if (msg->security_block_length > 0)
    {
      strcat(tlogmsg, "  (ISTATR message contents encrypted)\n");
    };
    if (msg->security_block_length EQUALS 0)
    {
      int i;
      unsigned char *p;
      i = 0;
      count = oh->len_lsb + (oh->len_msb << 8);
      count = count - 8;
      strcat(tlogmsg, "Input Status:\n");
      p = msg->data_payload;
      while (count > 0)
      {
        if (*p)
          sprintf(tmpstr, " IN-%d Active(%02x)", i, *p);
        else
          sprintf(tmpstr, " IN-%d Inactive(%02x)", i, *p);
        strcat(tlogmsg, tmpstr);
        count --; // decrement SDU octet count
        i++; // increment input number (index into data)
        p++; // increment pointer into data
      };
      strcat(tlogmsg, "\n");
    };
    break;

  case OOSDP_MSG_KEEPACTIVE:
    {
      msg = (OSDP_MSG *) aux;
      if (msg->security_block_length > 0)
      {
        strcat(tlogmsg, "  (KEYPAD message contents encrypted)\n");
      };
      if (msg->security_block_length EQUALS 0)
      {
        sprintf (tlogmsg,
"Keep credential read active %02x %02x",
          *(msg->data_payload+0), *(msg->data_payload+1));
      };
    };
    break;

  case OOSDP_MSG_KEYPAD:
    {
      int
        i;
      int
        keycount;

      msg = (OSDP_MSG *) aux;

      if (msg->security_block_length > 0)
      {
        strcat(tlogmsg, "  (KEYPAD message contents encrypted)\n");
      };
      if (msg->security_block_length EQUALS 0)
      {
        char character [8];
        char tstring [1024];

        keycount = *(msg->data_payload+1);
        memset (tmpstr, 0, sizeof (tmpstr));
        tstring [0] = 0;
        memcpy (tmpstr, msg->data_payload+2, *(msg->data_payload+1));
        for (i=0; i<keycount; i++)
        {
          memset(character, 0, sizeof(character));
          character [0] = tmpstr[i];
          if (tmpstr [i] EQUALS 0x7F)
            character [0] = '#';
          if (tmpstr [i] EQUALS 0x0D)
            character [0] = '*';

          // if not printable ascii and not already found character

          if ((tmpstr [i] < 0x20) ||
            (tmpstr [i] > 0x7e))
              if (character [0] != 0)
                sprintf(tmpstr, "<%02x>", tmpstr [i]);
          strcat(tstring, character);
        };
        sprintf (tlogmsg,
"Keypad Input Rdr %d, %d digits: %s\n",
          *(msg->data_payload+0), keycount, tstring);
      };
    };
    break;

  case OOSDP_MSG_KEYSET:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_KEYSET(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_LED:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_LED(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_LSTATR:
    {
      unsigned char *osdp_lstat_response_data;

      msg = (OSDP_MSG *) aux;
      if (msg->security_block_length > 0)
      {
        strcat(tlogmsg, "  (LSTATR message contents encrypted)\n");
      };
      if (msg->security_block_length EQUALS 0)
      {
        osdp_lstat_response_data = (unsigned char *)(msg->data_payload);
        sprintf(tlogmsg, "LSTAT Response: Tamper %d Power-cycle %d\n",
          osdp_lstat_response_data [0], osdp_lstat_response_data [1]);
      };
    };
    break;

  case OOSDP_MSG_MFG:
    {
      OSDP_MFG_HEADER *mrep;
      int process_as_special;

      if ((msg->security_block_length EQUALS 0) || (msg->payload_decrypted))
      {
      memset(tlogmsg, 0, sizeof(tlogmsg));
      process_as_special = 0;
      msg = (OSDP_MSG *) aux;
      oh = (OSDP_HDR *)(msg->ptr);
      count = oh->len_lsb + (oh->len_msb << 8);
      count = count - sizeof(*oh);
      if (oh->ctrl & 0x04)
        count = count - 2;
      else
        count = count - 1;

      mrep = (OSDP_MFG_HEADER *)(msg->data_payload);
      process_as_special = 0;
      if (0 EQUALS
        memcmp(mrep->vendor_code, OSDP_VENDOR_INID,
          sizeof(OSDP_VENDOR_INID)))
        process_as_special = 1;
      if (0 EQUALS
        memcmp(mrep->vendor_code, OSDP_VENDOR_WAVELYNX,
          sizeof(OSDP_VENDOR_WAVELYNX)))
        process_as_special = 1;
      if (!process_as_special)
      {
        sprintf(tlogmsg,
"(General) MFG Request: OUI:%02x-%02x-%02x Command: %02x\n",
          mrep->vendor_code [0], mrep->vendor_code [1], mrep->vendor_code [2],
          mrep->command_id);
      };
      if (process_as_special)
      {
        sprintf(tlogmsg,
          "  MFG Request: OUI:%02x-%02x-%02x Command: %02x\n",
          mrep->vendor_code [0], mrep->vendor_code [1], mrep->vendor_code [2],
          mrep->command_id);
        switch (mrep->command_id)
        {
        case OSDP_CMD_MSC_CR_AUTH:
          {
            cr_auth = (OSDP_MSC_CR_AUTH *)(msg->data_payload);

            sprintf(tmps,
"MSC CRAUTH\n  TotSize:%d. Offset:%d FragSize: %d",
              cr_auth->mpd_size_total, cr_auth->mpd_offset,
              cr_auth->mpd_fragment_size);
            strcat(tlogmsg, tmps);
            if (cr_auth->mpd_offset EQUALS 0)
            {
              sprintf(tmps, " AlgRef %02x KeyRef %02x",
                cr_auth->data[0], cr_auth->data[1]);
              strcat(tlogmsg, tmps);
            };
            strcat(tlogmsg, "\n");
          };
          break;
        case OSDP_CMD_MSC_GETPIV:
          {
            get_piv = (OSDP_MSC_GETPIV *)(msg->data_payload);
            sprintf(tmps,
"MSC PIVDATAGET\n  PIV-Object:%02x %02x %02x Element: %02x Offset: %02x %02x",
              get_piv->piv_object [0], get_piv->piv_object [1],
              get_piv->piv_object [2],
              get_piv->piv_element,
              get_piv->piv_offset [0], get_piv->piv_offset [1]);
            strcat(tlogmsg, tmps);
            strcat(tlogmsg, "\n");
            count = sizeof(*get_piv) - sizeof(get_piv->vendor_code)
              - sizeof(get_piv->command_id);
          };
          break;
        case OSDP_CMD_MSC_KP_ACT:
          {
            OSDP_MSC_KP_ACT *keep_active;
            keep_active = (OSDP_MSC_KP_ACT *)(msg->data_payload);
            sprintf(tmps,
"MSC KP_ACT\n  KP_ACT_TIME %d. ms\n",
              keep_active->kp_act_time);
            strcat(tlogmsg, tmps);
            count = sizeof(*keep_active) - sizeof(keep_active->vendor_code)
              - sizeof(keep_active->command_id);
          };
          break;
        default:
          sprintf(tlogmsg,
"MSC (MFG) Request: OUI:%02x-%02x-%02x Command: %02x\n",
            mrep->vendor_code [0], mrep->vendor_code [1], mrep->vendor_code [2],
            mrep->command_id);
          break;
        };
      }; 
      count = count - 4; // less OUI (3) and command (1)
      if (count > 0)
      {
        dump_buffer_log(&context, "  Raw(MFG): ", &(mrep->data), count);
      };
      }
      else
      {
        sprintf(tlogmsg,
          "  (MFG message contents encrypted)\n");
      };
    };
    break;

  case OOSDP_MSG_MFGREP:
    {
      OSDP_MFG_HEADER *mrep;
      int process_as_special;

      memset(tlogmsg, 0, sizeof(tlogmsg));
      process_as_special = 0;

      msg = (OSDP_MSG *) aux;
      oh = (OSDP_HDR *)(msg->ptr);
      count = oh->len_lsb + (oh->len_msb << 8);
      count = count - sizeof(*mrep) + 1;
      mrep = (OSDP_MFG_HEADER *)(msg->data_payload);

      process_as_special = 0;
      if (0 EQUALS memcmp(mrep->vendor_code, OSDP_VENDOR_INID, sizeof(OSDP_VENDOR_INID)))
        process_as_special = 1;

      if (!process_as_special)
      {
        sprintf(tlogmsg, "(General) MFG Reply: OUI:%02x-%02x-%02x RepID: %02x\n",
          mrep->vendor_code [0], mrep->vendor_code [1], mrep->vendor_code [2],
          mrep->command_id);
      };
      if (process_as_special)
      {
        sprintf(tlogmsg, "MFG Reply: OUI:%02x-%02x-%02x RepID: %02x\n",
          mrep->vendor_code [0], mrep->vendor_code [1], mrep->vendor_code [2],
          mrep->command_id);
        switch (mrep->command_id)
        {
        case OSDP_REP_MSC_CR_AUTH:
          {
            cr_auth_response = (OSDP_MSC_CR_AUTH_RESPONSE *)(msg->data_payload);
            sprintf(tmps,
"MSC CRAUTH RESPONSE TotSize:%d. Offset:%d FragSize: %d\n",
              cr_auth_response->mpd_size_total, cr_auth_response->mpd_offset,
              cr_auth_response->mpd_fragment_size);
            strcat(tlogmsg, tmps);
            count = sizeof(*cr_auth_response) + cr_auth_response->mpd_fragment_size - 1 - sizeof(cr_auth_response->vendor_code)
              - sizeof(cr_auth_response->command_id);
          };
          break;

        case OSDP_REP_MSC_PIVDATA:
          {
            piv_data = (OSDP_MSC_PIV_DATA *)(msg->data_payload);
            sprintf(tmps,
"MSC PIVDATA\n  TotSize:%d. Offset:%d FragSize: %d\n",
              piv_data->mpd_size_total, piv_data->mpd_offset,
              piv_data->mpd_fragment_size);
            strcat(tlogmsg, tmps);
            count = sizeof(*piv_data) + piv_data->mpd_fragment_size - 1 - sizeof(piv_data->vendor_code)
              - sizeof(piv_data->command_id);
          };
          break;
        case OSDP_REP_MSC_STAT:
          {
            msc_status = (OSDP_MSC_STATUS *)(msg->data_payload);
            sprintf(tmps, "MSC STATUS %02x Info %02x %02x\n", 
              msc_status->status, msc_status->info [0], msc_status->info [1]);
            count = sizeof(*msc_status) - sizeof(piv_data->vendor_code)
              - sizeof(piv_data->command_id);
          };
          break;
        default:
          sprintf(tlogmsg, "(General) MFG Response: OUI:%02x-%02x-%02x RepID: %02x\n",
            mrep->vendor_code [0], mrep->vendor_code [1], mrep->vendor_code [2],
            mrep->command_id);
          break;
        };
      };

      strcat(tlogmsg, "  Raw:");
      for (idx=0; idx<count; idx++)
      {
        sprintf(tmps, " %02x", *(unsigned char *)(&(mrep->data)+idx));
        strcat(tlogmsg, tmps);
      };
      strcat(tlogmsg, "\n");
    };
    break;

  case OOSDP_MSG_NAK:
    {
      int nak_code;
      char nak_detail_text [1024];
      char tmpmsg2 [2*1024];

      if ((msg->security_block_length EQUALS 0) || (msg->payload_decrypted))
      {
        msg = (OSDP_MSG *) aux;
        nak_code = *(0+msg->data_payload);
        // it's 1 if just a nak code and more if there is nak 'data'
        strcpy(nak_detail_text, oo_lookup_nak_text(nak_code));

        // for monitoring track nak count
        if (nak_code EQUALS OO_NAK_SEQUENCE)
          context.seq_bad++;

        sprintf(tmpmsg2, " (%s)", nak_detail_text);
        if (msg->data_length > 1)
        {
          sprintf(tlogmsg, "  NAK: Error Code %02x%s Data %02x\n",
            *(0+msg->data_payload), tmpmsg2, *(1+msg->data_payload));
        }
        else
        {
          sprintf (tlogmsg, "NAK: Error Code %02x%s\n",
            *(0+msg->data_payload), tmpmsg2);
        };
      }
      else
      {
        sprintf(tlogmsg, "  NAK: (Details encrypted)\n");
      };
    };
    break;

  // special case - this outputs the basic "Message:.." message
  case OOSDP_MSG_OSDP:
    osdp_command = *(unsigned char *)aux;
    hdr = 2+aux;

    // dump as named in the IEC spec
    msg = (OSDP_MSG *) aux;
    osdp_wire_message = (OSDP_HDR *)(msg->ptr); // actual message off the wire
    sprintf(tmpstr2, "SOM ADDR=%02x LEN_LSB=%02x LEN_MSB=%02x\n",
      osdp_wire_message->addr, osdp_wire_message->len_lsb,
      osdp_wire_message->len_msb);
    strcat(tlogmsg, tmpstr2); tmpstr2 [0] = 0;
    sprintf(tlogmsg, "  CTRL=%02x (", osdp_wire_message->ctrl);
    strcat(tlogmsg, tmpstr2); tmpstr2 [0] = 0;
    if (osdp_wire_message->ctrl & 0x08)
    {
      scb_present = 1;
      sprintf(tlogmsg, "Security Control Block Present; ");
      strcat(tlogmsg, tmpstr2); tmpstr2 [0] = 0;
    }
    else
    {
      scb_present = 0;
      sprintf(tlogmsg, "No Security Control Block; ");
      strcat(tlogmsg, tmpstr2); tmpstr2 [0] = 0;
    };
    if (scb_present)
    {
      sec_block = (char *)&(osdp_wire_message->command);
      switch (sec_block[1])  // "sec block type"
      {
      case OSDP_SEC_SCS_11:
        sprintf(tlogmsg, "SCS_11; ");
        strcat(tlogmsg, tmpstr2); tmpstr2 [0] = 0;
        break;
      case OSDP_SEC_SCS_12:
        sprintf(tlogmsg, "SCS_12; ");
        strcat(tlogmsg, tmpstr2); tmpstr2 [0] = 0;
        break;
      default:
fprintf(stderr, "unknown Security Block %d.\n", sec_block [1]);
        break;
      };
      if (sec_block [2] EQUALS OSDP_KEY_SCBK_D)
        sprintf(tlogmsg, "Key=SCBK-D(default) ");
      else
        sprintf(tlogmsg, "Key=SCBK ");
      strcat(tlogmsg, tmpstr2); tmpstr2 [0] = 0;
      strcat(tlogmsg, "\n");
    };

    strcpy(tmpstr, osdp_command_reply_to_string(osdp_command, *(unsigned char *)(1+aux)));
    sprintf(tmpstr2, "Message: %s\n", tmpstr);
    strcpy(tmpstr, osdp_sec_block_dump(2+aux+sizeof(*hdr)-1));
    strcat(tlogmsg, tmpstr2);
    strcat(tlogmsg, tmpstr);
    strcat(tlogmsg, "\n");
    break;

  case OOSDP_MSG_OUT:
    {
      OSDP_OUT_MSG *out_message;
      msg = (OSDP_MSG *) aux;
      out_message = (OSDP_OUT_MSG *)msg->data_payload;

      // assume one only

      if (msg->security_block_length > 0)
      {
        sprintf(tlogmsg, "  (OUT message contents encrypted)\n");
      };
      if (msg->security_block_length EQUALS 0)
      {
        sprintf (tlogmsg, "  Out: Line %02x Ctl %02x LSB %02x MSB %02x\n",
          out_message->output_number, out_message->control_code,
          out_message->timer_lsb, out_message->timer_msb);
      };
    };
    break;

  case OOSDP_MSG_OUT_STATUS:
    {
      int i;
      unsigned char *out_status;
      char tmpstr [1024];

      tlogmsg [0] = 0;
      if ((msg->security_block_length EQUALS 0) || (msg->payload_decrypted))
      {
        strcpy(tlogmsg, "I/O Status-OUT:");
        out_status = msg->data_payload;
        for (i=0; i<count; i++)
        {
          sprintf (tmpstr, " %02d:%d",
            i, out_status [i]);
          strcat (tlogmsg, tmpstr);
        };
        strcat(tlogmsg, "\n");
      };
    };
    break;

  case OOSDP_MSG_PD_CAPAS:
    {
      int i;
      OSDP_HDR *oh;
      char tstr [2*1024];
      int value;

      msg = (OSDP_MSG *) aux;
      oh = (OSDP_HDR *)(msg->ptr);
      count = oh->len_lsb + (oh->len_msb << 8);
      count = count - 8;
      if (msg->security_block_length > 0)
        sprintf(tstr, "PD Capabilities payload encrypted.\n");
      if ((msg->security_block_length EQUALS 0) || (msg->payload_decrypted))
      {
        sprintf (tstr, "PD Capabilities (%d)\n", (msg->data_length)/3);
        strcpy (tlogmsg, tstr);

        for (i=0; i<msg->data_length; i=i+3)
        {
          switch (*(i+0+msg->data_payload))
          {
          case 4:
            {
              int compliance;
              char tstr2 [1024];
              compliance = *(i+1+msg->data_payload);
              sprintf(tstr2, "?(0x%x)", compliance);
              if (compliance == 1) strcpy (tstr2, "On/Off Only");
              if (compliance == 2) strcpy (tstr2, "Timed");
              if (compliance == 3) strcpy (tstr2, "Timed, Bi-color");
              if (compliance == 4) strcpy (tstr2, "Timed, Tri-color");
              sprintf (tstr, "  [%02d] %s %d LED's Compliance:%s;\n",
                1+i/3, osdp_pdcap_function (*(i+0+msg->data_payload)), 
                *(i+2+msg->data_payload),
                tstr2);
            };
            break;
          case 10:
            value = *(i+1+msg->data_payload) + 256 * (*(i+2+msg->data_payload));
            sprintf (tstr, "  [%02d] %s %d;\n",
              1+i/3, osdp_pdcap_function (*(i+0+msg->data_payload)), value);
            break;
          case 11:
            value = *(i+1+msg->data_payload) + 256 * (*(i+2+msg->data_payload));
            context.max_message = value; // SIDE EFFECT (naughty me) - sets value when displaying it.
            sprintf (tstr, "  [%02d] %s %d;\n",
              1+i/3, osdp_pdcap_function (*(i+0+msg->data_payload)), value);
            break;
          default:
            sprintf (tstr, "  [%02d] %s %02x %02x;\n",
              1+i/3, osdp_pdcap_function (*(i+0+msg->data_payload)), *(i+1+msg->data_payload), *(i+2+msg->data_payload));
            break;
          };
          strcat (tlogmsg, tstr);
        };
      }; // decrypted or cleartext payload
    };
    break;

  case OOSDP_MSG_PD_IDENT:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_PD_IDENT(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_PKT_STATS:
    sprintf (tlogmsg, " ACU-Polls %6d PD-Acks %6d PD-NAKs %6d CkSumErr %6d\n",
      context.cp_polls, context.pd_acks, context.sent_naks,
      context.checksum_errs);
    break;

  case OOSDP_MSG_RAW:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_RAW(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_RMAC_I:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_RMAC_I(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_SCRYPT:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_SCRYPT(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_TEXT:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_TEXT(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_XREAD:
    msg = (OSDP_MSG *) aux;
    status = oosdp_print_message_XRD(&context, msg, tlogmsg);
    break;

  case OOSDP_MSG_XWRITE:
    msg = (OSDP_MSG *) aux;
    sprintf(tlogmsg, "Extended Write: %02x %02x %02x %02x\n",
      *(msg->data_payload + 0), *(msg->data_payload + 1),
      *(msg->data_payload + 2), *(msg->data_payload + 3));
    break;

  default:
    sprintf (tlogmsg, "Unknown message type %d", msgtype);
    break;
  };
  strcpy (logmsg, tlogmsg);
  return (status);
}


int
  oosdp_log
    (OSDP_CONTEXT
      *context,
    int
      logtype,
    int
      level,
    char
      *message)

{ /* oosdp_log */

  time_t current_raw_time;
  struct tm *current_cooked_time;
  int llogtype;
  char *role_tag;
  int status;
  char timestamp [2*1024];


  status = ST_OK;

  // dump the trace buffer before creating the log message
  osdp_trace_dump(context, 1);

  llogtype = logtype;
  role_tag = "";
  strcpy (timestamp, "");
  if (logtype EQUALS OSDP_LOG_STRING_CP)
  {
    role_tag = "ACU";
    llogtype = OSDP_LOG_STRING;
  };
  if (logtype EQUALS OSDP_LOG_STRING_PD)
  {
    role_tag = "PD";
    llogtype = OSDP_LOG_STRING;
  };
  if (llogtype == OSDP_LOG_STRING)
  {
    char address_suffix [1024];
    struct timespec
      current_time_fine;

    clock_gettime (CLOCK_REALTIME, &current_time_fine);
    (void) time (&current_raw_time);
    current_cooked_time = localtime (&current_raw_time);
if (strcmp ("ACU", role_tag)==0)
  sprintf (address_suffix, " DestAddr=%02x(hex)", context->this_message_addr);
else
  sprintf (address_suffix, " A=%02x(hex)", context->this_message_addr);
    sprintf (timestamp,
"\n---OSDP %s Frame:%04d%s Timestamp:%04d%02d%02d-%02d%02d%02d (Sec/Nanosec: %ld %ld)\n",
      role_tag, context->packets_received, address_suffix,
      1900+current_cooked_time->tm_year, 1+current_cooked_time->tm_mon,
      current_cooked_time->tm_mday,
      current_cooked_time->tm_hour, current_cooked_time->tm_min, 
      current_cooked_time->tm_sec,
      current_time_fine.tv_sec, current_time_fine.tv_nsec);
  };
  if (context->role == OSDP_ROLE_MONITOR)
  {
    fprintf (context->log, "%s%s", timestamp, message);
    fflush (context->log);
  }
  else
    if (context->verbosity >= level)
    {
      fprintf (context->log, "%s%s", timestamp, message);
      fflush (context->log);
    };
  
  return (status);

} /* oosdp_log */

