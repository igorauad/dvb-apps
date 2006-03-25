/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham (manu@kromtek.com)
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)
    Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#include <string.h>
#include <dvbmisc.h>
#include <pthread.h>
#include <ucsi/dvb/types.h>
#include "en50221_app_mmi.h"
#include "asn_1.h"

// tags supported by this resource
#define TAG_CLOSE_MMI               0x9f8800
#define TAG_DISPLAY_CONTROL         0x9f8801
#define TAG_DISPLAY_REPLY           0x9f8802
#define TAG_TEXT_LAST               0x9f8803
#define TAG_TEXT_MORE               0x9f8804
#define TAG_KEYPAD_CONTROL          0x9f8805
#define TAG_KEYPRESS                0x9f8806
#define TAG_ENQUIRY                 0x9f8807
#define TAG_ANSWER                  0x9f8808
#define TAG_MENU_LAST               0x9f8809
#define TAG_MENU_MORE               0x9f880a
#define TAG_MENU_ANSWER             0x9f880b
#define TAG_LIST_LAST               0x9f880c
#define TAG_LIST_MORE               0x9f880d
#define TAG_SUBTITLE_SEGMENT_LAST   0x9f880e
#define TAG_SUBTITLE_SEGMENT_MORE   0x9f880f
#define TAG_DISPLAY_MESSAGE         0x9f8810
#define TAG_SCENE_END_MARK          0x9f8811
#define TAG_SCENE_DONE              0x9f8812
#define TAG_SCENE_CONTROL           0x9f8813
#define TAG_SUBTITLE_DOWNLOAD_LAST  0x9f8814
#define TAG_SUBTITLE_DOWNLOAD_MORE  0x9f8815
#define TAG_FLUSH_DOWNLOAD          0x9f8816
#define TAG_DOWNLOAD_REPLY          0x9f8817

struct en50221_app_mmi_session {
        uint16_t session_number;

        uint8_t *menu_block_chain;
        uint32_t menu_block_length;

        uint8_t *list_block_chain;
        uint32_t list_block_length;

        uint8_t *subtitlesegment_block_chain;
        uint32_t subtitlesegment_block_length;

        uint8_t *subtitledownload_block_chain;
        uint32_t subtitledownload_block_length;

        struct en50221_app_mmi_session *next;
};

struct en50221_app_mmi_private {
        struct en50221_app_send_functions *funcs;
        struct en50221_app_mmi_session *sessions;

        en50221_app_mmi_close_callback closecallback;
        void *closecallback_arg;

        en50221_app_mmi_display_control_callback displaycontrolcallback;
        void *displaycontrolcallback_arg;

        en50221_app_mmi_keypad_control_callback keypadcontrolcallback;
        void *keypadcontrolcallback_arg;

        en50221_app_mmi_subtitle_segment_callback subtitlesegmentcallback;
        void *subtitlesegmentcallback_arg;

        en50221_app_mmi_scene_end_mark_callback sceneendmarkcallback;
        void *sceneendmarkcallback_arg;

        en50221_app_mmi_scene_control_callback scenecontrolcallback;
        void *scenecontrolcallback_arg;

        en50221_app_mmi_subtitle_download_callback subtitledownloadcallback;
        void *subtitledownloadcallback_arg;

        en50221_app_mmi_flush_download_callback flushdownloadcallback;
        void *flushdownloadcallback_arg;

        en50221_app_mmi_enq_callback enqcallback;
        void *enqcallback_arg;

        en50221_app_mmi_menu_callback menucallback;
        void *menucallback_arg;

        en50221_app_mmi_list_callback listcallback;
        void *listcallback_arg;

        pthread_mutex_t lock;
};

static int en50221_app_mmi_parse_close(struct en50221_app_mmi_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_display_control(struct en50221_app_mmi_private *private,
                                                 uint8_t slot_id, uint16_t session_number,
                                                 uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_keypad_control(struct en50221_app_mmi_private *private,
                                                uint8_t slot_id, uint16_t session_number,
                                                uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_enq(struct en50221_app_mmi_private *private,
                                     uint8_t slot_id, uint16_t session_number,
                                     uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_list_menu(struct en50221_app_mmi_private *private,
                                      uint8_t slot_id, uint16_t session_number, uint32_t tag_id,
                                      int more_last, uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_subtitle(struct en50221_app_mmi_private *private,
                                          uint8_t slot_id, uint16_t session_number, uint32_t tag_id,
                                          int more_last, uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_scene_end_mark(struct en50221_app_mmi_private *private,
                                                uint8_t slot_id, uint16_t session_number,
                                                uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_scene_control(struct en50221_app_mmi_private *private,
                                               uint8_t slot_id, uint16_t session_number,
                                               uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_subtitle(struct en50221_app_mmi_private *private,
                                          uint8_t slot_id, uint16_t session_number, uint32_t tag_id,
                                          int more_last, uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_parse_flush_download(struct en50221_app_mmi_private *private,
                                                uint8_t slot_id, uint16_t session_number,
                                                uint8_t *data, uint32_t data_length);
static int en50221_app_mmi_defragment(struct en50221_app_mmi_private *private,
                                      uint16_t session_number,
                                      uint32_t tag_id,
                                      int more_last,
                                      uint8_t *indata,
                                      uint32_t indata_length,
                                      uint8_t **outdata,
                                      uint32_t *outdata_length);
static int en50221_app_mmi_defragment_text(uint8_t *data,
                                           uint32_t data_length,
                                           uint8_t **outdata,
                                           uint32_t *outdata_length,
                                           uint32_t *outconsumed);



en50221_app_mmi en50221_app_mmi_create(struct en50221_app_send_functions *funcs)
{
    struct en50221_app_mmi_private *private = NULL;

    // create structure and set it up
    private = malloc(sizeof(struct en50221_app_mmi_private));
    if (private == NULL) {
        return NULL;
    }
    private->funcs = funcs;
    private->closecallback = NULL;
    private->displaycontrolcallback = NULL;
    private->keypadcontrolcallback = NULL;
    private->subtitlesegmentcallback = NULL;
    private->sceneendmarkcallback = NULL;
    private->scenecontrolcallback = NULL;
    private->subtitledownloadcallback = NULL;
    private->flushdownloadcallback = NULL;
    private->enqcallback = NULL;
    private->menucallback = NULL;
    private->listcallback = NULL;
    private->sessions = NULL;

    pthread_mutex_init(&private->lock, NULL);

    // done
    return private;
}

void en50221_app_mmi_destroy(en50221_app_mmi mmi)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    struct en50221_app_mmi_session *cur_s = private->sessions;
    while(cur_s) {
        struct en50221_app_mmi_session *next = cur_s->next;
        if (cur_s->menu_block_chain)
            free(cur_s->menu_block_chain);
        if (cur_s->list_block_chain)
            free(cur_s->list_block_chain);
        if (cur_s->subtitlesegment_block_chain)
            free(cur_s->subtitlesegment_block_chain);
        if (cur_s->subtitledownload_block_chain)
            free(cur_s->subtitledownload_block_chain);
        free(cur_s);
        cur_s = next;
    }

    pthread_mutex_destroy(&private->lock);
    free(private);
}

void en50221_app_mmi_clear_session(en50221_app_mmi mmi, uint16_t session_number)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    struct en50221_app_mmi_session *cur_s = private->sessions;
    struct en50221_app_mmi_session *prev_s = NULL;
    while(cur_s) {
        if (cur_s->session_number == session_number) {
            if (cur_s->menu_block_chain)
                free(cur_s->menu_block_chain);
            if (cur_s->list_block_chain)
                free(cur_s->list_block_chain);
            if (cur_s->subtitlesegment_block_chain)
                free(cur_s->subtitlesegment_block_chain);
            if (cur_s->subtitledownload_block_chain)
                free(cur_s->subtitledownload_block_chain);
            if (prev_s) {
                prev_s->next = cur_s->next;
            } else {
                private->sessions = cur_s->next;
            }
            free(cur_s);
            return;
        }

        prev_s = cur_s;
        cur_s=cur_s->next;
    }
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_close_callback(en50221_app_mmi mmi,
                                             en50221_app_mmi_close_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->closecallback = callback;
    private->closecallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_display_control_callback(en50221_app_mmi mmi,
                                                       en50221_app_mmi_display_control_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->displaycontrolcallback = callback;
    private->displaycontrolcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_keypad_control_callback(en50221_app_mmi mmi,
                                               en50221_app_mmi_keypad_control_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->keypadcontrolcallback = callback;
    private->keypadcontrolcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_subtitle_segment_callback(en50221_app_mmi mmi,
                                               en50221_app_mmi_subtitle_segment_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->subtitlesegmentcallback = callback;
    private->subtitlesegmentcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_scene_end_mark_callback(en50221_app_mmi mmi,
                                                      en50221_app_mmi_scene_end_mark_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->sceneendmarkcallback = callback;
    private->sceneendmarkcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_scene_control_callback(en50221_app_mmi mmi,
                                               en50221_app_mmi_scene_control_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->scenecontrolcallback = callback;
    private->scenecontrolcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_subtitle_download_callback(en50221_app_mmi mmi,
                                               en50221_app_mmi_subtitle_download_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->subtitledownloadcallback = callback;
    private->subtitledownloadcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_flush_download_callback(en50221_app_mmi mmi,
                                               en50221_app_mmi_flush_download_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->flushdownloadcallback = callback;
    private->flushdownloadcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_enq_callback(en50221_app_mmi mmi,
                                               en50221_app_mmi_enq_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->enqcallback = callback;
    private->enqcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_menu_callback(en50221_app_mmi mmi,
                                            en50221_app_mmi_menu_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->menucallback = callback;
    private->menucallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

void en50221_app_mmi_register_list_callback(en50221_app_mmi mmi,
                                               en50221_app_mmi_list_callback callback, void *arg)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;

    pthread_mutex_lock(&private->lock);
    private->listcallback = callback;
    private->listcallback_arg = arg;
    pthread_mutex_unlock(&private->lock);
}

int en50221_app_mmi_close(en50221_app_mmi mmi,
                                 uint16_t session_number,
                                 uint8_t cmd_id,
                                 uint8_t delay)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t data[6];
    int data_length = 5;

    data[0] = (TAG_CLOSE_MMI >> 16) & 0xFF;
    data[1] = (TAG_CLOSE_MMI >> 8) & 0xFF;
    data[2] = TAG_CLOSE_MMI & 0xFF;
    data[3] = 1;
    data[4] = cmd_id;
    if (cmd_id == MMI_CLOSE_MMI_CMD_ID_DELAY) {
        data[3] = 2;
        data[5] = delay;
        data_length = 6;
    }
    return private->funcs->send_data(private->funcs->arg, session_number, data, data_length);
}

int en50221_app_mmi_display_reply(en50221_app_mmi mmi,
                                         uint16_t session_number,
                                         uint8_t reply_id,
                                         struct en502221_app_mmi_display_reply_details *details)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t data[32];
    struct iovec iov[2];
    uint32_t iov_count;
    int length_field_len;

    // fill out the start of the header
    data[0] = (TAG_DISPLAY_REPLY >> 16) & 0xFF;
    data[1] = (TAG_DISPLAY_REPLY >> 8) & 0xFF;
    data[2] = TAG_DISPLAY_REPLY & 0xFF;

    switch(reply_id) {
    case MMI_DISPLAY_REPLY_ID_MMI_MODE_ACK:
        data[3] = 2;
        data[4] = reply_id;
        data[5] = details->u.mode_ack.mmi_mode;
        iov[0].iov_base = data;
        iov[0].iov_len = 6;
        iov_count = 1;
        break;

    case MMI_DISPLAY_REPLY_ID_LIST_DISPLAY_CHAR_TABLES:
    case MMI_DISPLAY_REPLY_ID_LIST_INPUT_CHAR_TABLES:
        if ((length_field_len = asn_1_encode(details->u.char_table.table_length+1, data+3, 3)) < 0) {
            return -1;
        }
        data[3+length_field_len] = reply_id;
        iov[0].iov_base = data;
        iov[0].iov_len = 3+length_field_len+1;
        iov[1].iov_base = details->u.char_table.table;
        iov[1].iov_len = details->u.char_table.table_length;
        iov_count = 2;
        break;

    case MMI_DISPLAY_REPLY_ID_LIST_OVERLAY_GFX_CHARACTERISTICS:
    case MMI_DISPLAY_REPLY_ID_LIST_FULLSCREEN_GFX_CHARACTERISTICS:
    {
        if ((length_field_len = asn_1_encode(1+9+(details->u.gfx.num_pixel_depths*2), data+3, 3)) < 0) {
            return -1;
        }
        data[3+length_field_len] = reply_id;
        data[3+length_field_len+1] = details->u.gfx.width >> 8;
        data[3+length_field_len+2] = details->u.gfx.width;
        data[3+length_field_len+3] = details->u.gfx.height >> 8;
        data[3+length_field_len+4] = details->u.gfx.height;
        data[3+length_field_len+5] = ((details->u.gfx.aspect_ratio & 0x0f) << 4) |
                                     ((details->u.gfx.gfx_relation_to_video & 0x07) << 1) |
                                     (details->u.gfx.multiple_depths & 1);
        data[3+length_field_len+6] = details->u.gfx.display_bytes >> 4;
        data[3+length_field_len+7] = ((details->u.gfx.display_bytes & 0x0f) << 4) |
                                     ((details->u.gfx.composition_buffer_bytes & 0xf0) >> 4);
        data[3+length_field_len+8] = ((details->u.gfx.composition_buffer_bytes & 0x0f) << 4) |
                                     ((details->u.gfx.object_cache_bytes & 0xf0) >> 4);
        data[3+length_field_len+9] = ((details->u.gfx.object_cache_bytes & 0x0f) << 4) |
                                     (details->u.gfx.num_pixel_depths & 0x0f);

        // render the pixel depths themselves
        uint8_t *pixdepths = alloca(details->u.gfx.num_pixel_depths * 2);
        if (pixdepths == NULL) {
            return -1;
        }
        uint32_t i;
        for(i=0; i < details->u.gfx.num_pixel_depths; i++) {
            pixdepths[0] = ((details->u.gfx.pixel_depths[i].display_depth & 0x07) << 5) |
                           ((details->u.gfx.pixel_depths[i].pixels_per_byte & 0x07) << 2);
            pixdepths[1] = details->u.gfx.pixel_depths[i].region_overhead;
            pixdepths+=2;
        }

        // make up the iovs
        iov[0].iov_base = data;
        iov[0].iov_len = 3+length_field_len+10;
        iov[1].iov_base = pixdepths;
        iov[1].iov_len = details->u.gfx.num_pixel_depths *2;
        iov_count = 2;
        break;
    }

    default:
        data[3] = 1;
        data[4] = reply_id;
        iov[0].iov_base = data;
        iov[0].iov_len = 5;
        iov_count = 1;
        break;
    }

    // sendit
    return private->funcs->send_datav(private->funcs->arg, session_number, iov, iov_count);
}

int en50221_app_mmi_keypress(en50221_app_mmi mmi,
                                    uint16_t session_number,
                                    uint8_t keycode)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t data[5];

    data[0] = (TAG_KEYPRESS >> 16) & 0xFF;
    data[1] = (TAG_KEYPRESS >> 8) & 0xFF;
    data[2] = TAG_KEYPRESS & 0xFF;
    data[3] = 1;
    data[4] = keycode;
    return private->funcs->send_data(private->funcs->arg, session_number, data, 5);
}

int en50221_app_mmi_display_message(en50221_app_mmi mmi,
                                           uint16_t session_number,
                                           uint8_t display_message_id)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t data[5];

    data[0] = (TAG_DISPLAY_MESSAGE >> 16) & 0xFF;
    data[1] = (TAG_DISPLAY_MESSAGE >> 8) & 0xFF;
    data[2] = TAG_DISPLAY_MESSAGE & 0xFF;
    data[3] = 1;
    data[4] = display_message_id;
    return private->funcs->send_data(private->funcs->arg, session_number, data, 5);
}

int en50221_app_mmi_scene_done(en50221_app_mmi mmi,
                                      uint16_t session_number,
                                      uint8_t decoder_continue,
                                      uint8_t scene_reveal,
                                      uint8_t scene_tag)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t data[5];

    data[0] = (TAG_SCENE_DONE >> 16) & 0xFF;
    data[1] = (TAG_SCENE_DONE >> 8) & 0xFF;
    data[2] = TAG_SCENE_DONE & 0xFF;
    data[3] = 1;
    data[4] = (decoder_continue ? 0x80 : 0x00) |
              (scene_reveal ? 0x40 : 0x00) |
              (scene_tag & 0x0f);
    return private->funcs->send_data(private->funcs->arg, session_number, data, 5);
}

int en50221_app_mmi_download_reply(en50221_app_mmi mmi,
                                          uint16_t session_number,
                                          uint16_t object_id,
                                          uint8_t download_reply_id)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t data[7];

    data[0] = (TAG_DOWNLOAD_REPLY >> 16) & 0xFF;
    data[1] = (TAG_DOWNLOAD_REPLY >> 8) & 0xFF;
    data[2] = TAG_DOWNLOAD_REPLY & 0xFF;
    data[3] = 3;
    data[4] = object_id >> 8;
    data[5] = object_id;
    data[6] = download_reply_id;
    return private->funcs->send_data(private->funcs->arg, session_number, data, 7);
}

int en50221_app_mmi_answ(en50221_app_mmi mmi,
                                uint16_t session_number,
                                uint8_t answ_id,
                                uint8_t *text,
                                uint32_t text_count)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t buf[10];

    // set up the tag
    buf[0] = (TAG_ANSWER >> 16) & 0xFF;
    buf[1] = (TAG_ANSWER >> 8) & 0xFF;
    buf[2] = TAG_ANSWER & 0xFF;

    // encode the length field
    struct iovec iov[2];
    int length_field_len = 0;
    int iov_count = 1;
    if (answ_id == MMI_ANSW_ID_ANSWER) {
        if ((length_field_len = asn_1_encode(text_count+1, buf+3, 3)) < 0) {
            return -1;
        }
        buf[3+length_field_len] = answ_id;

        iov[0].iov_base = buf;
        iov[0].iov_len = 3+length_field_len+1;
        iov[1].iov_base = text;
        iov[1].iov_len = text_count;
        iov_count=2;
    } else {
        buf[3] = 1;
        buf[4] = answ_id;
        iov[0].iov_base = buf;
        iov[0].iov_len = 5;
        iov_count = 1;
    }

    // create the data and send it
    return private->funcs->send_datav(private->funcs->arg, session_number, iov, iov_count);
}

int en50221_app_mmi_menu_answ(en50221_app_mmi mmi,
                                     uint16_t session_number,
                                     uint8_t choice_ref)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    uint8_t data[5];

    data[0] = (TAG_MENU_ANSWER >> 16) & 0xFF;
    data[1] = (TAG_MENU_ANSWER >> 8) & 0xFF;
    data[2] = TAG_MENU_ANSWER & 0xFF;
    data[3] = 1;
    data[4] = choice_ref;
    return private->funcs->send_data(private->funcs->arg, session_number, data, 5);
}

int en50221_app_mmi_message(en50221_app_mmi mmi,
                                  uint8_t slot_id,
                                  uint16_t session_number,
                                  uint32_t resource_id,
                                  uint8_t *data, uint32_t data_length)
{
    struct en50221_app_mmi_private *private = (struct en50221_app_mmi_private *) mmi;
    (void) resource_id;

    // get the tag
    if (data_length < 3) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    uint32_t tag = (data[0] << 16) | (data[1] << 8) | data[2];

    switch(tag)
    {
        case TAG_CLOSE_MMI:
            return en50221_app_mmi_parse_close(private, slot_id, session_number, data+3, data_length-3);
        case TAG_DISPLAY_CONTROL:
            return en50221_app_mmi_parse_display_control(private, slot_id, session_number, data+3, data_length-3);
        case TAG_KEYPAD_CONTROL:
            return en50221_app_mmi_parse_keypad_control(private, slot_id, session_number, data+3, data_length-3);
        case TAG_ENQUIRY:
            return en50221_app_mmi_parse_enq(private, slot_id, session_number, data+3, data_length-3);
        case TAG_MENU_LAST:
            return en50221_app_mmi_parse_list_menu(private, slot_id, session_number, tag, 1, data+3, data_length-3);
        case TAG_MENU_MORE:
            return en50221_app_mmi_parse_list_menu(private, slot_id, session_number, tag, 0, data+3, data_length-3);
        case TAG_LIST_LAST:
            return en50221_app_mmi_parse_list_menu(private, slot_id, session_number, tag, 1, data+3, data_length-3);
        case TAG_LIST_MORE:
            return en50221_app_mmi_parse_list_menu(private, slot_id, session_number, tag, 0, data+3, data_length-3);
        case TAG_SUBTITLE_SEGMENT_LAST:
            return en50221_app_mmi_parse_subtitle(private, slot_id, session_number, tag, 1, data+3, data_length-3);
        case TAG_SUBTITLE_SEGMENT_MORE:
            return en50221_app_mmi_parse_subtitle(private, slot_id, session_number, tag, 0, data+3, data_length-3);
        case TAG_SCENE_END_MARK:
            return en50221_app_mmi_parse_scene_end_mark(private, slot_id, session_number, data+3, data_length-3);
        case TAG_SCENE_CONTROL:
            return en50221_app_mmi_parse_scene_control(private, slot_id, session_number, data+3, data_length-3);
        case TAG_SUBTITLE_DOWNLOAD_LAST:
            return en50221_app_mmi_parse_subtitle(private, slot_id, session_number, tag, 1, data+3, data_length-3);
        case TAG_SUBTITLE_DOWNLOAD_MORE:
            return en50221_app_mmi_parse_subtitle(private, slot_id, session_number, tag, 0, data+3, data_length-3);
        case TAG_FLUSH_DOWNLOAD:
            return en50221_app_mmi_parse_flush_download(private, slot_id, session_number, data+3, data_length-3);
    }

    print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
    return -1;
}





static int en50221_app_mmi_parse_close(struct en50221_app_mmi_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length < 2) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    if (data[0] > (data_length-1)) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    uint8_t cmd_id = data[1];
    uint8_t delay = 0;
    if (cmd_id == MMI_CLOSE_MMI_CMD_ID_DELAY) {
        if (data[0] != 2) {
            print(LOG_LEVEL, ERROR, 1, "Received short data\n");
            return -1;
        }
        delay = data[2];
    }

    // tell the app
    pthread_mutex_lock(&private->lock);
    en50221_app_mmi_close_callback cb = private->closecallback;
    void *cb_arg = private->closecallback_arg;
    pthread_mutex_unlock(&private->lock);
    if (cb) {
        return cb(cb_arg, slot_id, session_number, cmd_id, delay);
    }
    return 0;
}

static int en50221_app_mmi_parse_display_control(struct en50221_app_mmi_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length < 2) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    if (data[0] > (data_length-1)) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    uint8_t cmd_id = data[1];
    uint8_t mmi_mode = 0;
    if (cmd_id == MMI_DISPLAY_CONTROL_CMD_ID_SET_MMI_MODE) {
        if (data[0] != 2) {
            print(LOG_LEVEL, ERROR, 1, "Received short data\n");
            return -1;
        }
        mmi_mode = data[2];
    }

    // tell the app
    pthread_mutex_lock(&private->lock);
    en50221_app_mmi_display_control_callback cb = private->displaycontrolcallback;
    void *cb_arg = private->displaycontrolcallback_arg;
    pthread_mutex_unlock(&private->lock);
    if (cb) {
        return cb(cb_arg, slot_id, session_number, cmd_id, mmi_mode);
    }
    return 0;
}

static int en50221_app_mmi_parse_keypad_control(struct en50221_app_mmi_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // first of all, decode the length field
    uint16_t asn_data_length;
    int length_field_len;
    if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
        print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
        return -1;
    }

    // check it
    if (asn_data_length > (data_length-length_field_len)) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    if (asn_data_length < 1) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }

    // skip over the length field
    data += length_field_len;

    // extract the information
    uint8_t cmd_id = data[0];
    uint8_t *keycodes = data+1;

    // tell the app
    pthread_mutex_lock(&private->lock);
    en50221_app_mmi_keypad_control_callback cb = private->keypadcontrolcallback;
    void *cb_arg = private->keypadcontrolcallback_arg;
    pthread_mutex_unlock(&private->lock);
    if (cb) {
        return cb(cb_arg, slot_id, session_number, cmd_id, keycodes, asn_data_length-1);
    }
    return 0;
}

static int en50221_app_mmi_parse_enq(struct en50221_app_mmi_private *private,
                                     uint8_t slot_id, uint16_t session_number,
                                     uint8_t *data, uint32_t data_length)
{
    // first of all, decode the length field
    uint16_t asn_data_length;
    int length_field_len;
    if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
        print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
        return -1;
    }

    // check it
    if (asn_data_length > (data_length-length_field_len)) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    if (asn_data_length < 2) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }

    // skip over the length field
    data += length_field_len;

    // extract the information
    uint8_t blind_answer = (data[0] & 0x01) ? 1 : 0;
    uint8_t answer_length = data[1];
    uint8_t *text = data+2;

    // tell the app
    pthread_mutex_lock(&private->lock);
    en50221_app_mmi_enq_callback cb = private->enqcallback;
    void *cb_arg = private->enqcallback_arg;
    pthread_mutex_unlock(&private->lock);
    if (cb) {
        return cb(cb_arg, slot_id, session_number, blind_answer, answer_length, text, asn_data_length - 2);
    }
    return 0;
}

static int en50221_app_mmi_parse_list_menu(struct en50221_app_mmi_private *private,
                                           uint8_t slot_id, uint16_t session_number, uint32_t tag_id,
                                           int more_last, uint8_t *data, uint32_t data_length)
{
    int result = 0;
    uint8_t *text_flags = NULL;
    struct en50221_app_mmi_text *text_data = NULL;
    uint32_t i;
    uint8_t text_count = 0;

    // first of all, decode the length field
    uint16_t asn_data_length;
    int length_field_len;
    if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
        print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
        return -1;
    }

    // check it
    if (asn_data_length > (data_length-length_field_len)) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }

    // skip over the length field
    data += length_field_len;

    // defragment
    pthread_mutex_lock(&private->lock);
    uint8_t *outdata;
    uint32_t outdata_length;
    int dfstatus = en50221_app_mmi_defragment(private, session_number, tag_id, more_last,
                                              data, asn_data_length,
                                              &outdata, &outdata_length);
    if (dfstatus <= 0) {
        pthread_mutex_unlock(&private->lock);
        return dfstatus;
    }
    data = outdata;
    data_length = outdata_length;

    // check the reassembled data length
    if (data_length < 1) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        pthread_mutex_unlock(&private->lock);
        result = -1;
        goto exit_cleanup;
    }

    // now, parse the data
    uint8_t choice_nb = data[0];
    text_count = choice_nb + 3;
    if (choice_nb == 0xff) text_count = 3;
    data++;
    data_length--;

    // variables for extracted text state
    text_flags = alloca(text_count);
    if (text_flags == NULL) {
        pthread_mutex_unlock(&private->lock);
        result = -1;
        goto exit_cleanup;
    }
    memset(text_flags, 0, text_count);
    text_data = (struct en50221_app_mmi_text*) alloca(sizeof(struct en50221_app_mmi_text) * text_count);
    if (text_data == NULL) {
        pthread_mutex_unlock(&private->lock);
        result = -1;
        goto exit_cleanup;
    }
    memset(text_data, 0, sizeof(struct en50221_app_mmi_text) * text_count);

    // extract the text!
    for(i=0; i<text_count; i++) {
        uint32_t consumed = 0;
        int cur_status = en50221_app_mmi_defragment_text(data, data_length,
                                                         &text_data[i].text, &text_data[i].text_length,
                                                         &consumed);
        if (cur_status < 0) {
            pthread_mutex_unlock(&private->lock);
            result = -1;
            goto exit_cleanup;
        }

        text_flags[i] = cur_status;
        data += consumed;
        data_length -= consumed;
    }

    // work out what to pass to the user
    struct en50221_app_mmi_text *text_data_for_user =
            (struct en50221_app_mmi_text*) alloca(sizeof(struct en50221_app_mmi_text) * text_count);
    if (text_data_for_user == NULL) {
        result = -1;
        goto exit_cleanup;
    }
    memcpy(text_data_for_user, text_data, sizeof(struct en50221_app_mmi_text) * text_count);
    struct en50221_app_mmi_text *text_ptr = NULL;
    if (text_count > 3) {
        text_ptr = &text_data_for_user[3];
    }
    uint8_t *items_raw = NULL;
    uint32_t items_raw_length = 0;
    if (choice_nb == 0xff) {
        items_raw = data;
        items_raw_length = data_length;
    }

    // do callback
    result = 0;
    switch(tag_id) {
        case TAG_MENU_LAST:
        {
            en50221_app_mmi_menu_callback cb = private->menucallback;
            void *cb_arg = private->menucallback_arg;
            pthread_mutex_unlock(&private->lock);
            if (cb) {
                result = cb(cb_arg, slot_id, session_number,
                            &text_data_for_user[0],
                            &text_data_for_user[1],
                            &text_data_for_user[2],
                            text_count-3,
                            text_ptr,
                            items_raw_length,
                            items_raw);
            }
            break;
        }

        case TAG_LIST_LAST:
        {
            en50221_app_mmi_list_callback cb = private->listcallback;
            void *cb_arg = private->listcallback_arg;
            pthread_mutex_unlock(&private->lock);
            if (cb) {
                result = cb(cb_arg, slot_id, session_number,
                            &text_data_for_user[0],
                            &text_data_for_user[1],
                            &text_data_for_user[2],
                            text_count-3,
                            text_ptr,
                            items_raw_length,
                            items_raw);
            }
            break;
        }

        default:
            pthread_mutex_unlock(&private->lock);
            break;
    }

exit_cleanup:
    if ((dfstatus == 2) && outdata) free(outdata);
    if (text_flags && text_data) {
        for(i=0; i< text_count; i++) {
            if ((text_flags[i] == 2) && text_data[i].text) {
                free(text_data[i].text);
            }
        }
    }
    return result;
}

static int en50221_app_mmi_parse_subtitle(struct en50221_app_mmi_private *private,
                                          uint8_t slot_id, uint16_t session_number, uint32_t tag_id,
                                          int more_last, uint8_t *data, uint32_t data_length)
{
    // first of all, decode the length field
    uint16_t asn_data_length;
    int length_field_len;
    if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
        print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
        return -1;
    }

    // check it
    if (asn_data_length > (data_length-length_field_len)) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }

    // skip over the length field
    data += length_field_len;

    // defragment
    pthread_mutex_lock(&private->lock);
    uint8_t *outdata;
    uint32_t outdata_length;
    int dfstatus = en50221_app_mmi_defragment(private, session_number, tag_id, more_last,
                                            data, asn_data_length,
                                            &outdata, &outdata_length);
    if (dfstatus <= 0) {
        pthread_mutex_unlock(&private->lock);
        return dfstatus;
    }

    // do callback
    int cbstatus = 0;
    switch(tag_id) {
        case TAG_SUBTITLE_SEGMENT_LAST:
        {
            en50221_app_mmi_subtitle_segment_callback cb = private->subtitlesegmentcallback;
            void *cb_arg = private->subtitlesegmentcallback_arg;
            pthread_mutex_unlock(&private->lock);
            if (cb) {
                cbstatus = cb(cb_arg, slot_id, session_number, outdata, outdata_length);
            }
            break;
        }

        case TAG_SUBTITLE_DOWNLOAD_LAST:
        {
            en50221_app_mmi_subtitle_download_callback cb = private->subtitledownloadcallback;
            void *cb_arg = private->subtitledownloadcallback_arg;
            pthread_mutex_unlock(&private->lock);
            if (cb) {
                cbstatus = cb(cb_arg, slot_id, session_number, outdata, outdata_length);
            }
            break;
        }
    }

    // free the data returned by the defragment call if asked to
    if (dfstatus == 2) {
        free(outdata);
    }

    // done
    return cbstatus;
}

static int en50221_app_mmi_parse_scene_end_mark(struct en50221_app_mmi_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length != 2) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    if (data[0] != 1) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    uint8_t flags = data[1];

    // tell the app
    pthread_mutex_lock(&private->lock);
    en50221_app_mmi_scene_end_mark_callback cb = private->sceneendmarkcallback;
    void *cb_arg = private->sceneendmarkcallback_arg;
    pthread_mutex_unlock(&private->lock);
    if (cb) {
        return cb(cb_arg, slot_id, session_number,
                  (flags & 0x80) ? 1 : 0,
                  (flags & 0x40) ? 1 : 0,
                  (flags & 0x20) ? 1 : 0,
                  flags & 0x0f);
    }
    return 0;
}

static int en50221_app_mmi_parse_scene_control(struct en50221_app_mmi_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length != 2) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    if (data[0] != 1) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    uint8_t flags = data[1];

    // tell the app
    pthread_mutex_lock(&private->lock);
    en50221_app_mmi_scene_control_callback cb = private->scenecontrolcallback;
    void *cb_arg = private->scenecontrolcallback_arg;
    pthread_mutex_unlock(&private->lock);
    if (cb) {
        return cb(cb_arg, slot_id, session_number,
                  (flags & 0x80) ? 1 : 0,
                  (flags & 0x40) ? 1 : 0,
                  flags & 0x0f);
    }
    return 0;
}

static int en50221_app_mmi_parse_flush_download(struct en50221_app_mmi_private *private,
                                       uint8_t slot_id, uint16_t session_number,
                                       uint8_t *data, uint32_t data_length)
{
    // validate data
    if (data_length != 1) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }
    if (data[0] != 0) {
        print(LOG_LEVEL, ERROR, 1, "Received short data\n");
        return -1;
    }

    // tell the app
    pthread_mutex_lock(&private->lock);
    en50221_app_mmi_flush_download_callback cb = private->flushdownloadcallback;
    void *cb_arg = private->flushdownloadcallback_arg;
    pthread_mutex_unlock(&private->lock);
    if (cb) {
        return cb(cb_arg, slot_id, session_number);
    }
    return 0;
}

static int en50221_app_mmi_defragment(struct en50221_app_mmi_private *private,
                                      uint16_t session_number,
                                      uint32_t tag_id,
                                      int more_last,
                                      uint8_t *indata,
                                      uint32_t indata_length,
                                      uint8_t **outdata,
                                      uint32_t *outdata_length)
{
    struct en50221_app_mmi_session *cur_s = private->sessions;
    while(cur_s) {
        if (cur_s->session_number == session_number)
            break;
        cur_s=cur_s->next;
    }

    // more data is still to come
    if (!more_last) {
        // if there was no previous session, create one
        if (cur_s == NULL) {
            cur_s = malloc(sizeof(struct en50221_app_mmi_session));
            if (cur_s == NULL) {
                print(LOG_LEVEL, ERROR, 1, "Ran out of memory\n");
                return -1;
            }
            cur_s->session_number = session_number;
            cur_s->menu_block_chain = NULL;
            cur_s->menu_block_length = 0;
            cur_s->list_block_chain = NULL;
            cur_s->list_block_length = 0;
            cur_s->subtitlesegment_block_chain = NULL;
            cur_s->subtitlesegment_block_length = 0;
            cur_s->subtitledownload_block_chain = NULL;
            cur_s->subtitledownload_block_length = 0;
            cur_s->next = private->sessions;
            private->sessions = cur_s;
        }

        // find the block/block_length to use
        uint8_t **block_chain;
        uint32_t *block_length;
        switch(tag_id) {
            case TAG_MENU_LAST:
            case TAG_MENU_MORE:
                block_chain = &cur_s->menu_block_chain;
                block_length = &cur_s->menu_block_length;
                break;
            case TAG_LIST_LAST:
            case TAG_LIST_MORE:
                block_chain = &cur_s->list_block_chain;
                block_length = &cur_s->list_block_length;
                break;
            case TAG_SUBTITLE_SEGMENT_LAST:
            case TAG_SUBTITLE_SEGMENT_MORE:
                block_chain = &cur_s->subtitlesegment_block_chain;
                block_length = &cur_s->subtitlesegment_block_length;
                break;
            case TAG_SUBTITLE_DOWNLOAD_LAST:
            case TAG_SUBTITLE_DOWNLOAD_MORE:
                block_chain = &cur_s->subtitledownload_block_chain;
                block_length = &cur_s->subtitledownload_block_length;
                break;
        }

        // append the data
        uint8_t *new_data = realloc(*block_chain, *block_length + indata_length);
        if (new_data == NULL) {
            print(LOG_LEVEL, ERROR, 1, "Ran out of memory\n");
            return -1;
        }
        memcpy(new_data + *block_length, indata, indata_length);
        *block_chain = new_data;
        *block_length += indata_length;

        // success, but block not complete yet
        return 0;
    }

    // we hit the last of a possible chain of fragments
    if (cur_s != NULL) {
        // find the block/block_length to use
        uint8_t **block_chain;
        uint32_t *block_length;
        switch(tag_id) {
            case TAG_MENU_LAST:
            case TAG_MENU_MORE:
                block_chain = &cur_s->menu_block_chain;
                block_length = &cur_s->menu_block_length;
                break;
            case TAG_LIST_LAST:
            case TAG_LIST_MORE:
                block_chain = &cur_s->list_block_chain;
                block_length = &cur_s->list_block_length;
                break;
            case TAG_SUBTITLE_SEGMENT_LAST:
            case TAG_SUBTITLE_SEGMENT_MORE:
                block_chain = &cur_s->subtitlesegment_block_chain;
                block_length = &cur_s->subtitlesegment_block_length;
                break;
            case TAG_SUBTITLE_DOWNLOAD_LAST:
            case TAG_SUBTITLE_DOWNLOAD_MORE:
                block_chain = &cur_s->subtitledownload_block_chain;
                block_length = &cur_s->subtitledownload_block_length;
                break;
        }

        // we have a preceding fragment - need to append
        uint8_t *new_data = realloc(*block_chain, *block_length + indata_length);
        if (new_data == NULL) {
            print(LOG_LEVEL, ERROR, 1, "Ran out of memory\n");
            return -1;
        }
        memcpy(new_data + *block_length, indata, indata_length);
        *outdata_length = *block_length + indata_length;
        *outdata = new_data;
        *block_chain = NULL;
        *block_length = 0;

        // success, and indicate to free the block when done
        return 2;
    }

    // success, but indicate it is not to be freed
    *outdata_length = indata_length;
    *outdata = indata;
    return 1;
}

static int en50221_app_mmi_defragment_text(uint8_t *data,
                                           uint32_t data_length,
                                           uint8_t **outdata,
                                           uint32_t *outdata_length,
                                           uint32_t *outconsumed)
{
    uint8_t *text = NULL;
    uint32_t text_length = 0;
    uint32_t consumed = 0;

    while(1) {
        // get the tag
        if (data_length < 3) {
            print(LOG_LEVEL, ERROR, 1, "Short data\n");
            if (text) free(text);
            return -1;
        }
        uint32_t tag = (data[0] << 16) | (data[1] << 8) | data[2];
        data += 3;
        data_length -=3;
        consumed += 3;

        // get the length of the data and adjust
        uint16_t asn_data_length;
        int length_field_len;
        if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
            print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
            if (text) free(text);
            return -1;
        }
        data += length_field_len;
        data_length -= length_field_len;
        consumed += length_field_len;

        // deal with the tags
        if (tag == TAG_TEXT_LAST) {
            if (text == NULL) {
                *outdata = data;
                *outdata_length = asn_data_length;
                *outconsumed = consumed + asn_data_length;
                return 1;
            } else {
                // append the data
                uint8_t *new_text = realloc(text, text_length + asn_data_length);
                if (new_text == NULL) {
                    print(LOG_LEVEL, ERROR, 1, "Ran out of memory\n");
                    if (text) free(text);
                    return -1;
                }
                memcpy(new_text + text_length, data, asn_data_length);
                *outdata = new_text;
                *outdata_length = text_length + asn_data_length;
                *outconsumed = consumed + asn_data_length;
                return 2;
            }

        } else if (tag == TAG_TEXT_MORE) {
            // append the data
            uint8_t *new_text = realloc(text, text_length + asn_data_length);
            if (new_text == NULL) {
                print(LOG_LEVEL, ERROR, 1, "Ran out of memory\n");
                if (text) free(text);
                return -1;
            }
            memcpy(new_text + text_length, data, asn_data_length);
            text = new_text;
            text_length += asn_data_length;

            // consume the data
            data += asn_data_length;
            data_length -= asn_data_length;
            consumed += asn_data_length;
        } else {
            // unknown tag
            print(LOG_LEVEL, ERROR, 1, "Unknown MMI text tag\n");
            if (text) free(text);
            return -1;
        }
    }
}