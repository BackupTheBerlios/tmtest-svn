/* conn.h
 *
 * Copyright (C) 2006 Scott Bronson
 * This file is under the MIT license.
 * See the COPYING file for specifics.
 *
 * A connection represents a currently open single TCP connection that
 * passes through the tunnel.
 */


struct conn;
struct tunnel;
struct list;


struct conn* conn_from_list(struct list_head *list);

void conn_remote_is_ready(struct conn *conn);
void conn_accept(io_atom *ioa, struct tunnel *tunnel);

void conn_close(struct conn *conn);
void conn_destroy(struct conn *conn);

