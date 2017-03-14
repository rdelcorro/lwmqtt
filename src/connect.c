/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *******************************************************************************/

#include <string.h>

#include "connect.h"
#include "helpers.h"
#include "packet.h"

static int lwmqtt_serialize_connect_length(lwmqtt_options_t *options) {
  int len = 10;

  len += lwmqtt_strlen(options->client_id) + 2;

  if (options->will != NULL) {
    len += lwmqtt_strlen(options->will->topic) + 2 + lwmqtt_strlen(options->will->message) + 2;
  }

  if (options->username.c_string || options->username.lp_string.data) {
    len += lwmqtt_strlen(options->username) + 2;
  }

  if (options->password.c_string || options->password.lp_string.data) {
    len += lwmqtt_strlen(options->password) + 2;
  }

  return len;
}

int lwmqtt_serialize_connect(unsigned char *buf, int buf_len, lwmqtt_options_t *options) {
  unsigned char *ptr = buf;

  int rem_len = lwmqtt_serialize_connect_length(options);

  if (lwmqtt_header_len(rem_len) + rem_len > buf_len) {
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  lwmqtt_header_t header = {0};
  header.bits.type = LWMQTT_CONNECT_PACKET;
  lwmqtt_write_char(&ptr, header.byte);  // write header

  ptr += lwmqtt_header_encode(ptr, rem_len);  // write remaining length

  lwmqtt_write_c_string(&ptr, "MQTT");
  lwmqtt_write_char(&ptr, 4);

  lwmqtt_connect_flags_t flags = {0};
  flags.bits.clean_session = options->clean_session;
  flags.bits.will = (options->will != NULL) ? 1 : 0;
  if (flags.bits.will) {
    flags.bits.will_qos = (unsigned int)options->will->qos;
    flags.bits.will_retain = options->will->retained;
  }

  if (options->username.c_string || options->username.lp_string.data) {
    flags.bits.username = 1;
  }

  if (options->password.c_string || options->password.lp_string.data) {
    flags.bits.password = 1;
  }

  lwmqtt_write_char(&ptr, flags.byte);
  lwmqtt_write_int(&ptr, options->keep_alive);
  lwmqtt_write_string(&ptr, options->client_id);

  if (options->will != NULL) {
    lwmqtt_write_string(&ptr, options->will->topic);
    lwmqtt_write_string(&ptr, options->will->message);
  }

  if (flags.bits.username) {
    lwmqtt_write_string(&ptr, options->username);
  }

  if (flags.bits.password) {
    lwmqtt_write_string(&ptr, options->password);
  }

  return (int)(ptr - buf);
}

int lwmqtt_deserialize_connack(unsigned char *session_present, unsigned char *connack_rc, unsigned char *buf,
                               int buf_len) {
  lwmqtt_header_t header = {0};
  unsigned char *cur_ptr = buf;
  int rc = 0;
  int len;
  lwmqtt_connack_flags flags = {0};

  header.byte = lwmqtt_read_char(&cur_ptr);
  if (header.bits.type != LWMQTT_CONNACK_PACKET) {
    return rc;
  }

  cur_ptr += (rc = lwmqtt_header_decode(cur_ptr, &len));  // read remaining length
  unsigned char *end_ptr = cur_ptr + len;
  if (end_ptr - cur_ptr < 2) {
    return rc;
  }

  flags.byte = lwmqtt_read_char(&cur_ptr);
  *session_present = (unsigned char)flags.bits.session_present;
  *connack_rc = lwmqtt_read_char(&cur_ptr);

  return 1;
}

static int lwmqtt_serialize_zero(unsigned char *buf, int buf_len, unsigned char packet_type) {
  lwmqtt_header_t header = {0};
  unsigned char *ptr = buf;

  if (buf_len < 2) {
    return LWMQTT_BUFFER_TOO_SHORT;
  }

  header.byte = 0;
  header.bits.type = packet_type;
  lwmqtt_write_char(&ptr, header.byte);  // write header

  ptr += lwmqtt_header_encode(ptr, 0);  // write remaining length

  return (int)(ptr - buf);
}

int lwmqtt_serialize_disconnect(unsigned char *buf, int buf_len) {
  return lwmqtt_serialize_zero(buf, buf_len, LWMQTT_DISCONNECT_PACKET);
}

int lwmqtt_serialize_pingreq(unsigned char *buf, int buf_len) {
  return lwmqtt_serialize_zero(buf, buf_len, LWMQTT_PINGREQ_PACKET);
}
