/* Swfdec
 * Copyright (C) 2008 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef _SWFDEC_RTMP_SOCKET_H_
#define _SWFDEC_RTMP_SOCKET_H_

#include <glib-object.h>
#include <swfdec/swfdec_rtmp_connection.h>

G_BEGIN_DECLS


typedef struct _SwfdecRtmpSocketClass SwfdecRtmpSocketClass;

#define SWFDEC_TYPE_RTMP_SOCKET                    (swfdec_rtmp_socket_get_type())
#define SWFDEC_IS_RTMP_SOCKET(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_RTMP_SOCKET))
#define SWFDEC_IS_RTMP_SOCKET_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_RTMP_SOCKET))
#define SWFDEC_RTMP_SOCKET(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_RTMP_SOCKET, SwfdecRtmpSocket))
#define SWFDEC_RTMP_SOCKET_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_RTMP_SOCKET, SwfdecRtmpSocketClass))
#define SWFDEC_RTMP_SOCKET_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), SWFDEC_TYPE_RTMP_SOCKET, SwfdecRtmpSocketClass))

struct _SwfdecRtmpSocket {
  GObject       	object;

  SwfdecRtmpConnection *conn;		/* the connection that spawned and refs us */
  char *		error;		/* NULL or description of error socket is in */
};

struct _SwfdecRtmpSocketClass {
  GObjectClass		object_class;

  /* actually open the RTMP connection */
  void			(* open)			(SwfdecRtmpSocket *	socket,
							 const char *		url);
  /* close the (open) RTMP connection */
  void			(* close)			(SwfdecRtmpSocket *	socket);
  /* send data down the RTMP connection (the conn will be open) */
  void			(* send)			(SwfdecRtmpSocket *	socket,
							 SwfdecBuffer *		data);
};

GType			swfdec_rtmp_socket_get_type	(void);

SwfdecRtmpSocket *	swfdec_rtmp_socket_new		(SwfdecRtmpConnection *	conn,
							 const char *		url);

void			swfdec_rtmp_socket_receive	(SwfdecRtmpSocket *	socket,
							 SwfdecBuffer *		data);
void			swfdec_rtmp_socket_error	(SwfdecRtmpSocket *	socket,
							 const char *		error,
							 ...) G_GNUC_PRINTF (2, 3);
void	    		swfdec_rtmp_socket_errorv	(SwfdecRtmpSocket *	socket,
							 const char *		error,
							 va_list		args);


G_END_DECLS
#endif
