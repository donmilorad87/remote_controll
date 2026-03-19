#include "email.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

/* CURL payload for reading email body line by line */
typedef struct {
    const char *data;
    size_t      bytes_remaining;
} rc_email_payload_t;

static size_t email_read_callback(char *buffer, size_t size, size_t nitems,
                                  void *userdata)
{
    rc_email_payload_t *payload = (rc_email_payload_t *)userdata;
    size_t max_bytes = size * nitems;

    assert(payload != NULL);

    if (payload->bytes_remaining == 0) {
        return 0;
    }

    size_t to_copy = payload->bytes_remaining;
    if (to_copy > max_bytes) {
        to_copy = max_bytes;
    }

    memcpy(buffer, payload->data, to_copy);
    payload->data += to_copy;
    payload->bytes_remaining -= to_copy;

    return to_copy;
}

int rc_email_send_activation(const rc_config_t *cfg, const char *to_email,
                             const char *activation_url)
{
    assert(cfg != NULL);
    assert(to_email != NULL);
    assert(activation_url != NULL);

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "[email] Failed to init curl\n");
        return -1;
    }

    /* Build SMTP URL: smtp://host:port */
    char smtp_url[256];
    snprintf(smtp_url, sizeof(smtp_url), "smtp://%s:%u",
             cfg->smtp_host, cfg->smtp_port);

    /* Build From header */
    char from_header[512];
    snprintf(from_header, sizeof(from_header), "%s <%s>",
             cfg->smtp_from_name, cfg->smtp_from);

    /* Build email body (RFC 5322 format) */
    char email_body[4096];
    int body_len = snprintf(email_body, sizeof(email_body),
        "From: %s\r\n"
        "To: <%s>\r\n"
        "Subject: Activate Your RemoteControl Account\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n"
        "<!DOCTYPE html>\r\n"
        "<html><body style=\"font-family: Arial, sans-serif; max-width: 600px; "
        "margin: 0 auto; padding: 20px;\">\r\n"
        "<h2 style=\"color: #2563eb;\">Welcome to RemoteControl</h2>\r\n"
        "<p>Thank you for registering. Please click the button below to "
        "activate your account:</p>\r\n"
        "<p style=\"text-align: center; margin: 30px 0;\">\r\n"
        "  <a href=\"%s\" style=\"background: #2563eb; color: white; "
        "padding: 14px 28px; text-decoration: none; border-radius: 6px; "
        "font-weight: bold; display: inline-block;\">Activate Account</a>\r\n"
        "</p>\r\n"
        "<p style=\"color: #666; font-size: 14px;\">Or copy this link:</p>\r\n"
        "<p style=\"color: #3b82f6; font-size: 14px; word-break: break-all;\">"
        "%s</p>\r\n"
        "<hr style=\"border: none; border-top: 1px solid #e2e8f0; margin: 30px 0;\">\r\n"
        "<p style=\"color: #999; font-size: 12px;\">This link expires in 24 hours. "
        "If you did not create this account, please ignore this email.</p>\r\n"
        "</body></html>\r\n",
        from_header, to_email, activation_url, activation_url);

    if (body_len < 0 || (size_t)body_len >= sizeof(email_body)) {
        curl_easy_cleanup(curl);
        return -1;
    }

    rc_email_payload_t payload = {
        .data = email_body,
        .bytes_remaining = (size_t)body_len
    };

    /* Set recipients */
    struct curl_slist *recipients = NULL;
    recipients = curl_slist_append(recipients, to_email);

    /* Configure CURL for SMTP with authentication */
    curl_easy_setopt(curl, CURLOPT_URL, smtp_url);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, cfg->smtp_from);
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, email_read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &payload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* SMTP Authentication */
    if (cfg->smtp_username[0] != '\0') {
        curl_easy_setopt(curl, CURLOPT_USERNAME, cfg->smtp_username);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, cfg->smtp_password);
    }

    /* Use STARTTLS if available */
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_TRY);

    /* Timeouts */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    /* Verbose for debugging (remove in production) */
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[email] SMTP send failed: %s\n", curl_easy_strerror(res));
        return -1;
    }

    fprintf(stdout, "[email] Activation email sent to %s via %s:%u\n",
            to_email, cfg->smtp_host, cfg->smtp_port);
    return 0;
}
