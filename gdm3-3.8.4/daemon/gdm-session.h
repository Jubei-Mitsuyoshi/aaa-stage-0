/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006 Ray Strode <rstrode@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __GDM_SESSION_H
#define __GDM_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GDM_TYPE_SESSION (gdm_session_get_type ())
#define GDM_SESSION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDM_TYPE_SESSION, GdmSession))
#define GDM_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GDM_TYPE_SESSION, GdmSessionClass))
#define GDM_IS_SESSION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDM_TYPE_SESSION))
#define GDM_IS_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDM_TYPE_SESSION))
#define GDM_SESSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GDM_TYPE_SESSION, GdmSessionClass))

typedef struct _GdmSessionPrivate GdmSessionPrivate;

typedef enum
{
        GDM_SESSION_VERIFICATION_MODE_LOGIN,
        GDM_SESSION_VERIFICATION_MODE_CHOOSER,
        GDM_SESSION_VERIFICATION_MODE_REAUTHENTICATE
} GdmSessionVerificationMode;

typedef struct
{
        GObject            parent;
        GdmSessionPrivate *priv;
} GdmSession;

typedef struct
{
        GObjectClass parent_class;

        /* Signals */
        void (* client_ready_for_session_to_start) (GdmSession   *session,
                                                    const char   *service_name,
                                                    gboolean      client_is_ready);

        void (* cancelled)                   (GdmSession   *session);
        void (* client_connected)            (GdmSession   *session);
        void (* client_disconnected)         (GdmSession   *session);
        void (* disconnected)                (GdmSession   *session);
        void (* verification_complete)       (GdmSession   *session,
                                              const char   *service_name);
        void (* session_opened)              (GdmSession   *session,
                                              const char   *service_name,
                                              const char   *session_id);
        void (* session_started)             (GdmSession   *session,
                                              const char   *service_name,
                                              const char   *session_id,
                                              int           pid);
        void (* session_start_failed)        (GdmSession   *session,
                                              const char   *service_name,
                                              const char   *message);
        void (* session_exited)              (GdmSession   *session,
                                              int           exit_code);
        void (* session_died)                (GdmSession   *session,
                                              int           signal_number);
        void (* reauthentication_started)    (GdmSession   *session,
                                              GPid          pid_of_caller);
        void (* reauthenticated)             (GdmSession   *session,
                                              const char   *service_name);
        void (* conversation_started)        (GdmSession   *session,
                                              const char   *service_name);
        void (* conversation_stopped)        (GdmSession   *session,
                                              const char   *service_name);
        void (* setup_complete)              (GdmSession   *session,
                                              const char   *service_name);
} GdmSessionClass;

GType            gdm_session_get_type                 (void);

GdmSession      *gdm_session_new                      (GdmSessionVerificationMode verification_mode,
                                                       uid_t         allowed_user,
                                                       const char   *display_name,
                                                       const char   *display_hostname,
                                                       const char   *display_device,
                                                       const char   *display_seat_id,
                                                       const char   *display_x11_authority_file,
                                                       gboolean      display_is_local,
                                                       const char * const *environment);
uid_t             gdm_session_get_allowed_user       (GdmSession     *session);
void              gdm_session_start_reauthentication (GdmSession *session,
                                                      GPid        pid_of_caller,
                                                      uid_t       uid_of_caller);

char             *gdm_session_get_server_address          (GdmSession     *session);
char             *gdm_session_get_username                (GdmSession     *session);
char             *gdm_session_get_display_device          (GdmSession     *session);
char             *gdm_session_get_display_seat_id         (GdmSession     *session);
char             *gdm_session_get_session_id              (GdmSession     *session);
gboolean          gdm_session_bypasses_xsession           (GdmSession     *session);

void              gdm_session_start_conversation          (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_stop_conversation           (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_setup                       (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_setup_for_user              (GdmSession *session,
                                                           const char *service_name,
                                                           const char *username);
void              gdm_session_setup_for_program           (GdmSession *session,
                                                           const char *service_name,
                                                           const char *username,
                                                           const char *log_file);
void              gdm_session_set_environment_variable    (GdmSession *session,
                                                           const char *key,
                                                           const char *value);
void              gdm_session_send_environment            (GdmSession *self,
                                                           const char *service_name);
void              gdm_session_reset                       (GdmSession *session);
void              gdm_session_authenticate                (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_authorize                   (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_accredit                    (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_open_session                (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_start_session               (GdmSession *session,
                                                           const char *service_name);
void              gdm_session_close                       (GdmSession *session);

void              gdm_session_answer_query                (GdmSession *session,
                                                           const char *service_name,
                                                           const char *text);
void              gdm_session_select_program              (GdmSession *session,
                                                           const char *command_line);
void              gdm_session_select_session_type         (GdmSession *session,
                                                           const char *session_type);
void              gdm_session_select_session              (GdmSession *session,
                                                           const char *session_name);
void              gdm_session_select_language             (GdmSession *session,
                                                           const char *language);
void              gdm_session_select_user                 (GdmSession *session,
                                                           const char *username);
void              gdm_session_cancel                      (GdmSession *session);
void              gdm_session_reset                       (GdmSession *session);
void              gdm_session_request_timed_login         (GdmSession *session,
                                                           const char *username,
                                                           int         delay);
gboolean          gdm_session_client_is_connected         (GdmSession *session);

G_END_DECLS

#endif /* GDM_SESSION_H */
