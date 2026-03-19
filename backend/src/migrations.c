#include "database.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define MAX_MIGRATIONS 64

/* Compare migration filenames for sorting */
static int migration_cmp(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

int rc_db_run_migrations(rc_db_pool_t *pool, const char *migrations_path)
{
    assert(pool != NULL);
    assert(migrations_path != NULL);

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) {
        return -1;
    }

    /* Create migrations tracking table if not exists */
    PGresult *res = PQexec(conn,
        "CREATE TABLE IF NOT EXISTS _migrations ("
        "  id SERIAL PRIMARY KEY,"
        "  filename VARCHAR(255) NOT NULL UNIQUE,"
        "  applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()"
        ")");

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "[migrations] Failed to create tracking table: %s\n",
                PQerrorMessage(conn));
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }
    PQclear(res);

    /* Scan migrations directory */
    DIR *dir = opendir(migrations_path);
    if (dir == NULL) {
        fprintf(stderr, "[migrations] Cannot open directory: %s\n", migrations_path);
        rc_db_release(pool, conn);
        return -1;
    }

    char *files[MAX_MIGRATIONS];
    int file_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && file_count < MAX_MIGRATIONS) {
        size_t name_len = strlen(entry->d_name);
        if (name_len < 5) continue;
        /* Only .sql files */
        if (strcmp(entry->d_name + name_len - 4, ".sql") != 0) continue;

        files[file_count] = strdup(entry->d_name);
        if (files[file_count] != NULL) {
            file_count++;
        }
    }
    closedir(dir);

    /* Sort alphabetically (001_, 002_, etc.) */
    qsort(files, (size_t)file_count, sizeof(char *), migration_cmp);

    int applied = 0;
    for (int i = 0; i < file_count; i++) {
        /* Check if already applied */
        const char *params[1] = { files[i] };
        res = PQexecParams(conn,
            "SELECT 1 FROM _migrations WHERE filename = $1",
            1, NULL, params, NULL, NULL, 0);

        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
            /* Already applied, skip */
            PQclear(res);
            free(files[i]);
            continue;
        }
        PQclear(res);

        /* Read migration file */
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", migrations_path, files[i]);

        char *sql = rc_read_file(filepath, NULL);
        if (sql == NULL) {
            fprintf(stderr, "[migrations] Cannot read: %s\n", filepath);
            free(files[i]);
            continue;
        }

        /* Execute migration */
        fprintf(stdout, "[migrations] Applying: %s\n", files[i]);
        res = PQexec(conn, sql);
        free(sql);

        if (PQresultStatus(res) != PGRES_COMMAND_OK &&
            PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "[migrations] Failed to apply %s: %s\n",
                    files[i], PQerrorMessage(conn));
            PQclear(res);
            free(files[i]);
            /* Continue to next migration instead of failing entirely */
            continue;
        }
        PQclear(res);

        /* Record migration as applied */
        res = PQexecParams(conn,
            "INSERT INTO _migrations (filename) VALUES ($1)",
            1, NULL, params, NULL, NULL, 0);
        PQclear(res);

        applied++;
        free(files[i]);
    }

    fprintf(stdout, "[migrations] Applied %d new migration(s)\n", applied);
    rc_db_release(pool, conn);
    return 0;
}
