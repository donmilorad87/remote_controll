#!/bin/bash
set -e

# Update hostname if env var is set
if [ -n "$POSTFIX_HOSTNAME" ]; then
    postconf -e "myhostname=$POSTFIX_HOSTNAME"
fi

if [ -n "$POSTFIX_DOMAIN" ]; then
    postconf -e "mydomain=$POSTFIX_DOMAIN"
    postconf -e "myorigin=\$mydomain"
fi

# Start postfix in foreground
postfix start-fg
