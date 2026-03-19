#ifndef RC_EMAIL_H
#define RC_EMAIL_H

#include "config.h"

/* Send an activation email to the user via SMTP with authentication.
   Returns 0 on success. */
int rc_email_send_activation(const rc_config_t *cfg, const char *to_email,
                             const char *activation_url);

#endif /* RC_EMAIL_H */
