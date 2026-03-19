#include "admin_panel.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADMIN_BUF_SIZE  (64 * 1024)
#define USERS_PER_PAGE  50

static const char *ADMIN_HEADER =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\"><head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "<title>RemoteControl Admin</title>\n"
    "<style>\n"
    "  * { margin: 0; padding: 0; box-sizing: border-box; }\n"
    "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n"
    "         background: #f1f5f9; color: #1e293b; }\n"
    "  nav { background: #1e293b; padding: 16px 24px; display: flex; gap: 24px; align-items: center; }\n"
    "  nav a { color: #94a3b8; text-decoration: none; font-size: 14px; }\n"
    "  nav a:hover, nav a.active { color: #fff; }\n"
    "  nav .brand { color: #3b82f6; font-weight: 700; font-size: 18px; margin-right: 24px; }\n"
    "  .container { max-width: 1200px; margin: 24px auto; padding: 0 24px; }\n"
    "  .card { background: #fff; border-radius: 8px; padding: 24px; margin-bottom: 16px;\n"
    "          box-shadow: 0 1px 3px rgba(0,0,0,0.08); }\n"
    "  .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; }\n"
    "  .stat-card { background: #fff; border-radius: 8px; padding: 20px;\n"
    "               box-shadow: 0 1px 3px rgba(0,0,0,0.08); text-align: center; }\n"
    "  .stat-num { font-size: 36px; font-weight: 700; color: #3b82f6; }\n"
    "  .stat-label { font-size: 14px; color: #64748b; margin-top: 4px; }\n"
    "  table { width: 100%%; border-collapse: collapse; }\n"
    "  th { background: #f8fafc; text-align: left; padding: 12px; font-size: 13px;\n"
    "       color: #64748b; text-transform: uppercase; letter-spacing: 0.5px; }\n"
    "  td { padding: 12px; border-top: 1px solid #e2e8f0; font-size: 14px; }\n"
    "  .badge { display: inline-block; padding: 2px 10px; border-radius: 12px;\n"
    "           font-size: 12px; font-weight: 600; }\n"
    "  .badge-green { background: #dcfce7; color: #16a34a; }\n"
    "  .badge-yellow { background: #fef9c3; color: #ca8a04; }\n"
    "  .pagination { display: flex; gap: 8px; margin-top: 16px; justify-content: center; }\n"
    "  .pagination a { padding: 8px 16px; background: #fff; border: 1px solid #e2e8f0;\n"
    "                  border-radius: 6px; text-decoration: none; color: #475569; }\n"
    "  .pagination a:hover { background: #f1f5f9; }\n"
    "  h2 { font-size: 20px; margin-bottom: 16px; }\n"
    "</style>\n"
    "</head><body>\n"
    "<nav>\n"
    "  <span class=\"brand\">RemoteControl</span>\n"
    "  <a href=\"/admin/\">Dashboard</a>\n"
    "  <a href=\"/admin/users\">Users</a>\n"
    "  <a href=\"/admin/connections\">Connections</a>\n"
    "</nav>\n"
    "<div class=\"container\">\n";

static const char *ADMIN_FOOTER =
    "</div>\n"
    "</body></html>\n";

char *rc_admin_render_dashboard(rc_db_pool_t *pool)
{
    assert(pool != NULL);

    rc_admin_stats_t stats;
    if (rc_db_admin_stats(pool, &stats) != 0) {
        return NULL;
    }

    char *buf = malloc(ADMIN_BUF_SIZE);
    if (buf == NULL) return NULL;

    int len = snprintf(buf, ADMIN_BUF_SIZE,
        "%s"
        "<h2>Dashboard</h2>\n"
        "<div class=\"stats\">\n"
        "  <div class=\"stat-card\"><div class=\"stat-num\">%lld</div>\n"
        "    <div class=\"stat-label\">Total Users</div></div>\n"
        "  <div class=\"stat-card\"><div class=\"stat-num\">%lld</div>\n"
        "    <div class=\"stat-label\">Activated Users</div></div>\n"
        "  <div class=\"stat-card\"><div class=\"stat-num\">%lld</div>\n"
        "    <div class=\"stat-label\">Active Sessions</div></div>\n"
        "  <div class=\"stat-card\"><div class=\"stat-num\">%lld</div>\n"
        "    <div class=\"stat-label\">Live Connections</div></div>\n"
        "</div>\n"
        "%s",
        ADMIN_HEADER,
        (long long)stats.total_users,
        (long long)stats.activated_users,
        (long long)stats.active_sessions,
        (long long)stats.active_connections,
        ADMIN_FOOTER);

    if (len < 0 || (size_t)len >= ADMIN_BUF_SIZE) {
        free(buf);
        return NULL;
    }

    return buf;
}

char *rc_admin_render_users(rc_db_pool_t *pool, int page)
{
    assert(pool != NULL);
    assert(page >= 1);

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return NULL;

    char limit_str[16], offset_str[16];
    snprintf(limit_str, sizeof(limit_str), "%d", USERS_PER_PAGE);
    snprintf(offset_str, sizeof(offset_str), "%d", (page - 1) * USERS_PER_PAGE);

    const char *params[2] = { limit_str, offset_str };
    PGresult *res = PQexecParams(conn,
        "SELECT out_id, out_email, out_is_activated, out_created_at, out_total_count "
        "FROM sp_list_users($1::int, $2::int)",
        2, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        rc_db_release(pool, conn);
        return NULL;
    }

    char *buf = malloc(ADMIN_BUF_SIZE);
    if (buf == NULL) {
        PQclear(res);
        rc_db_release(pool, conn);
        return NULL;
    }

    int len = snprintf(buf, ADMIN_BUF_SIZE,
        "%s"
        "<h2>Users</h2>\n"
        "<div class=\"card\">\n"
        "<table>\n"
        "<thead><tr><th>Email</th><th>Status</th><th>Created</th><th>ID</th></tr></thead>\n"
        "<tbody>\n",
        ADMIN_HEADER);

    int nrows = PQntuples(res);
    int64_t total = 0;

    for (int i = 0; i < nrows && (size_t)len < ADMIN_BUF_SIZE - 512; i++) {
        const char *email = PQgetvalue(res, i, 1);
        const char *activated = PQgetvalue(res, i, 2);
        const char *created = PQgetvalue(res, i, 3);
        const char *id = PQgetvalue(res, i, 0);

        if (i == 0 && !PQgetisnull(res, i, 4)) {
            total = strtoll(PQgetvalue(res, i, 4), NULL, 10);
        }

        int is_active = (strcmp(activated, "t") == 0);
        len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len,
            "<tr><td>%s</td><td><span class=\"badge %s\">%s</span></td>"
            "<td>%s</td><td style=\"font-size:12px;color:#94a3b8;\">%s</td></tr>\n",
            email,
            is_active ? "badge-green" : "badge-yellow",
            is_active ? "Active" : "Pending",
            created, id);
    }

    len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len,
        "</tbody></table>\n</div>\n");

    /* Pagination */
    int total_pages = (int)((total + USERS_PER_PAGE - 1) / USERS_PER_PAGE);
    if (total_pages < 1) total_pages = 1;

    if (total_pages > 1) {
        len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len,
            "<div class=\"pagination\">\n");
        for (int p = 1; p <= total_pages && p <= 20; p++) {
            len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len,
                "<a href=\"/admin/users?page=%d\"%s>%d</a>\n",
                p, (p == page) ? " style=\"background:#3b82f6;color:#fff;\"" : "", p);
        }
        len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len, "</div>\n");
    }

    len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len, "%s", ADMIN_FOOTER);

    PQclear(res);
    rc_db_release(pool, conn);
    return buf;
}

char *rc_admin_render_connections(rc_db_pool_t *pool)
{
    assert(pool != NULL);

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return NULL;

    PGresult *res = PQexec(conn,
        "SELECT out_user_email, out_device_type, out_platform, out_connected_at "
        "FROM sp_list_active_connections()");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        rc_db_release(pool, conn);
        return NULL;
    }

    char *buf = malloc(ADMIN_BUF_SIZE);
    if (buf == NULL) {
        PQclear(res);
        rc_db_release(pool, conn);
        return NULL;
    }

    int len = snprintf(buf, ADMIN_BUF_SIZE,
        "%s"
        "<h2>Active Connections</h2>\n"
        "<div class=\"card\">\n"
        "<table>\n"
        "<thead><tr><th>User</th><th>Device</th><th>Platform</th>"
        "<th>Connected At</th></tr></thead>\n"
        "<tbody>\n",
        ADMIN_HEADER);

    int nrows = PQntuples(res);
    for (int i = 0; i < nrows && (size_t)len < ADMIN_BUF_SIZE - 512; i++) {
        len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len,
            "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
            PQgetvalue(res, i, 0), PQgetvalue(res, i, 1),
            PQgetvalue(res, i, 2), PQgetvalue(res, i, 3));
    }

    if (nrows == 0) {
        len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len,
            "<tr><td colspan=\"4\" style=\"text-align:center;color:#94a3b8;\">"
            "No active connections</td></tr>\n");
    }

    len += snprintf(buf + len, ADMIN_BUF_SIZE - (size_t)len,
        "</tbody></table>\n</div>\n%s", ADMIN_FOOTER);

    PQclear(res);
    rc_db_release(pool, conn);
    return buf;
}
