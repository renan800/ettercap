/*
    ettercap -- stream buffer module

    Copyright (C) ALoR & NaGA

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    $Id: ec_streambuf.c,v 1.1 2003/08/22 19:23:40 alor Exp $
*/

#include <ec.h>
#include <ec_packet.h>
#include <ec_streambuf.h>

/* mutexes */

#define STREAMBUF_INIT_LOCK(x)  do{ pthread_mutex_init(&x, NULL); }while(0)
#define STREAMBUF_LOCK(x)       do{ pthread_mutex_lock(&x); }while(0)
#define STREAMBUF_UNLOCK(x)     do{ pthread_mutex_unlock(&x); }while(0)

/* protos */

void streambuf_init(struct stream_buf *sb);
int streambuf_add(struct stream_buf *sb, struct packet_object *po);
int streambuf_get(struct stream_buf *sb, u_char *buf, size_t len, int mode);
void streambuf_wipe(struct stream_buf *sb);

/************************************************/

/*
 * initialize the buffer
 */
void streambuf_init(struct stream_buf *sb)
{
   //DEBUG_MSG("streambuf_init");
 
   /* init the size */
   sb->size = 0;
   
   /* init the tail */
   TAILQ_INIT(&sb->streambuf_tail);
   
   /* init the mutex */
   STREAMBUF_INIT_LOCK(sb->streambuf_mutex);
}


/* 
 * add the packet to the stream_buf.
 */
int streambuf_add(struct stream_buf *sb, struct packet_object *po)
{
   struct stream_pck_list *p;

   p = calloc(1, sizeof(struct stream_pck_list));
   ON_ERROR(p, NULL, "Can't allocate memory");

   /* fill the struct */
   p->size = po->DATA.len;
   p->ptr = 0;
  
   /* copy the buffer */
   p->buf = calloc(po->DATA.len, sizeof(u_char));
   ON_ERROR(p->buf, NULL, "Can't allocate memory");
   
   memcpy(p->buf, po->DATA.data, po->DATA.len);

   STREAMBUF_LOCK(sb->streambuf_mutex);
   
   /* insert the packet in the tail */
   TAILQ_INSERT_TAIL(&sb->streambuf_tail, p, next);

   /* update the total size */
   sb->size += p->size;
      
   STREAMBUF_UNLOCK(sb->streambuf_mutex);

   return 0;
}


/*
 * copies in the 'buf' the first 'len' bytes 
 * of the stream buffer
 *
 * STREAM_ATOMIC  returns an error if there is not enough
 *                data to fill 'len' bytes in 'buf'
 *
 * STREAM_PARTIAL will fill the buffer 'buf' with the data
 *                contained in the streambuf even if it is
 *                less than 'len'. size is returned accordingly
 */
int streambuf_get(struct stream_buf *sb, u_char *buf, size_t len, int mode)
{
   struct stream_pck_list *p;
   struct stream_pck_list *old = NULL;
   size_t size = 0, to_copy = 0;

   /* always wipe the buffer */
   memset(buf, 0, len);
   
   /* check if we have enough data */
   if (mode == STREAM_ATOMIC && sb->size < len)
      return -EINVALID;

   STREAMBUF_LOCK(sb->streambuf_mutex);
   
   /* insert the packet in the tail */
   TAILQ_FOREACH(p, &sb->streambuf_tail, next) {

      SAFE_FREE(old);
      
      /* we have copied all the needed bytes */
      if (size >= len)
         break;
     
      /* calculate the lenght to be copied */
      if (len - size < p->size)
         to_copy = len - size;
      else
         to_copy = p->size;
     
      if (p->ptr + to_copy > p->size)
         to_copy = p->size - p->ptr;

      /* 
       * copy the data in the buffer
       * p->ptr is the pointer to last read 
       * byte if the buffer was read partially
       */
      memcpy(buf + size, p->buf + p->ptr, to_copy);

      /* bytes in the buffer 'buf' */
      size += to_copy;
      
      /* remember how may byte we have read */
      p->ptr += to_copy;

      /* packet not completed */
      if (p->ptr < p->size) {
         break;
      }
      
      /* remove the entry from the tail */
      SAFE_FREE(p->buf);
      TAILQ_REMOVE(&sb->streambuf_tail, p, next);
      old = p;
      
   }

   SAFE_FREE(old);

   /* update the total size */
   sb->size -= size;
      
   STREAMBUF_UNLOCK(sb->streambuf_mutex);

   
   return size;
}


/*
 * empty a give buffer.
 * all the elements in the list are deleted
 */
void streambuf_wipe(struct stream_buf *sb)
{
   struct stream_pck_list *e;

   DEBUG_MSG("streambuf_wipe");
   
   STREAMBUF_LOCK(sb->streambuf_mutex);
   
   /* delete the list */
   while ((e = TAILQ_FIRST(&sb->streambuf_tail)) != TAILQ_END(&sb->streambuf_tail)) {
      TAILQ_REMOVE(&sb->streambuf_tail, e, next);
      SAFE_FREE(e->buf);
      SAFE_FREE(e);
   }

   /* reset the buffer */
   TAILQ_INIT(&sb->streambuf_tail);

   STREAMBUF_UNLOCK(sb->streambuf_mutex);
}


/* EOF */

// vim:ts=3:expandtab
