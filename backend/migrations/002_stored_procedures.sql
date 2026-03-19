-- Migration 002: Stored procedures
-- All database operations go through these procedures

BEGIN;

-- ═══════════════════════════════════════════════════════════════════
-- REGISTRATION & ACTIVATION
-- ═══════════════════════════════════════════════════════════════════

-- Register a new user. Returns user_id and activation_token.
-- Password must be pre-hashed by the application (bcrypt/argon2).
CREATE OR REPLACE FUNCTION sp_register_user(
    p_email         VARCHAR(255),
    p_password_hash VARCHAR(255)
)
RETURNS TABLE(
    out_user_id     UUID,
    out_token       VARCHAR(128),
    out_error       VARCHAR(100)
) AS $$
DECLARE
    v_user_id       UUID;
    v_token         VARCHAR(128);
BEGIN
    -- Check if email already exists
    IF EXISTS (SELECT 1 FROM users WHERE email = LOWER(TRIM(p_email))) THEN
        RETURN QUERY SELECT NULL::UUID, NULL::VARCHAR(128), 'EMAIL_EXISTS'::VARCHAR(100);
        RETURN;
    END IF;

    -- Insert user
    INSERT INTO users (email, password_hash)
    VALUES (LOWER(TRIM(p_email)), p_password_hash)
    RETURNING id INTO v_user_id;

    -- Generate activation token (64 hex chars)
    v_token := encode(gen_random_bytes(64), 'hex');

    -- Insert activation token (expires in 24 hours)
    INSERT INTO activation_tokens (user_id, token, expires_at)
    VALUES (v_user_id, v_token, NOW() + INTERVAL '24 hours');

    RETURN QUERY SELECT v_user_id, v_token, NULL::VARCHAR(100);
END;
$$ LANGUAGE plpgsql;

-- Activate a user by token. Returns success or error.
CREATE OR REPLACE FUNCTION sp_activate_user(
    p_token VARCHAR(128)
)
RETURNS TABLE(
    out_user_id UUID,
    out_error   VARCHAR(100)
) AS $$
DECLARE
    v_record RECORD;
BEGIN
    -- Find valid token
    SELECT at.id AS token_id, at.user_id, at.expires_at, at.used
    INTO v_record
    FROM activation_tokens at
    WHERE at.token = p_token;

    IF NOT FOUND THEN
        RETURN QUERY SELECT NULL::UUID, 'INVALID_TOKEN'::VARCHAR(100);
        RETURN;
    END IF;

    IF v_record.used THEN
        RETURN QUERY SELECT NULL::UUID, 'TOKEN_USED'::VARCHAR(100);
        RETURN;
    END IF;

    IF v_record.expires_at < NOW() THEN
        RETURN QUERY SELECT NULL::UUID, 'TOKEN_EXPIRED'::VARCHAR(100);
        RETURN;
    END IF;

    -- Activate user
    UPDATE users SET is_activated = TRUE WHERE id = v_record.user_id;

    -- Mark token as used
    UPDATE activation_tokens SET used = TRUE WHERE id = v_record.token_id;

    RETURN QUERY SELECT v_record.user_id, NULL::VARCHAR(100);
END;
$$ LANGUAGE plpgsql;

-- ═══════════════════════════════════════════════════════════════════
-- AUTHENTICATION
-- ═══════════════════════════════════════════════════════════════════

-- Get user by email for login validation.
-- Application verifies password hash.
CREATE OR REPLACE FUNCTION sp_get_user_by_email(
    p_email VARCHAR(255)
)
RETURNS TABLE(
    out_user_id         UUID,
    out_password_hash   VARCHAR(255),
    out_is_activated    BOOLEAN,
    out_error           VARCHAR(100)
) AS $$
BEGIN
    RETURN QUERY
    SELECT u.id, u.password_hash, u.is_activated, NULL::VARCHAR(100)
    FROM users u
    WHERE u.email = LOWER(TRIM(p_email));

    IF NOT FOUND THEN
        RETURN QUERY SELECT NULL::UUID, NULL::VARCHAR(255), NULL::BOOLEAN, 'USER_NOT_FOUND'::VARCHAR(100);
    END IF;
END;
$$ LANGUAGE plpgsql;

-- ═══════════════════════════════════════════════════════════════════
-- SESSION MANAGEMENT
-- ═══════════════════════════════════════════════════════════════════

-- Create a session. For desktop device_type, revoke any existing active desktop session first.
CREATE OR REPLACE FUNCTION sp_create_session(
    p_user_id       UUID,
    p_device_type   VARCHAR(20),
    p_jwt_id        VARCHAR(64),
    p_ip_address    VARCHAR(45),
    p_user_agent    VARCHAR(512),
    p_expires_at    TIMESTAMPTZ
)
RETURNS TABLE(
    out_session_id      UUID,
    out_kicked_session  UUID,
    out_error           VARCHAR(100)
) AS $$
DECLARE
    v_session_id        UUID;
    v_kicked_session    UUID := NULL;
BEGIN
    -- For desktop: enforce one-desktop-per-account by revoking existing
    IF p_device_type = 'desktop' THEN
        UPDATE sessions
        SET is_active = FALSE, revoked_at = NOW()
        WHERE user_id = p_user_id
          AND device_type = 'desktop'
          AND is_active = TRUE
        RETURNING id INTO v_kicked_session;

        -- Also disconnect any active desktop device connections
        IF v_kicked_session IS NOT NULL THEN
            UPDATE device_connections
            SET is_connected = FALSE, disconnected_at = NOW()
            WHERE session_id = v_kicked_session AND is_connected = TRUE;
        END IF;
    END IF;

    -- Create new session
    INSERT INTO sessions (user_id, device_type, jwt_id, ip_address, user_agent, expires_at)
    VALUES (p_user_id, p_device_type, p_jwt_id, p_ip_address, p_user_agent, p_expires_at)
    RETURNING id INTO v_session_id;

    RETURN QUERY SELECT v_session_id, v_kicked_session, NULL::VARCHAR(100);
END;
$$ LANGUAGE plpgsql;

-- Validate a session by JWT ID. Returns session info if active and not expired.
CREATE OR REPLACE FUNCTION sp_validate_session(
    p_jwt_id VARCHAR(64)
)
RETURNS TABLE(
    out_session_id  UUID,
    out_user_id     UUID,
    out_device_type VARCHAR(20),
    out_error       VARCHAR(100)
) AS $$
BEGIN
    RETURN QUERY
    SELECT s.id, s.user_id, s.device_type, NULL::VARCHAR(100)
    FROM sessions s
    WHERE s.jwt_id = p_jwt_id
      AND s.is_active = TRUE
      AND s.expires_at > NOW();

    IF NOT FOUND THEN
        RETURN QUERY SELECT NULL::UUID, NULL::UUID, NULL::VARCHAR(20), 'SESSION_INVALID'::VARCHAR(100);
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Revoke a session (logout).
CREATE OR REPLACE FUNCTION sp_revoke_session(
    p_jwt_id VARCHAR(64)
)
RETURNS TABLE(
    out_success BOOLEAN,
    out_error   VARCHAR(100)
) AS $$
BEGIN
    UPDATE sessions
    SET is_active = FALSE, revoked_at = NOW()
    WHERE jwt_id = p_jwt_id AND is_active = TRUE;

    IF NOT FOUND THEN
        RETURN QUERY SELECT FALSE, 'SESSION_NOT_FOUND'::VARCHAR(100);
        RETURN;
    END IF;

    -- Disconnect associated device connections
    UPDATE device_connections
    SET is_connected = FALSE, disconnected_at = NOW()
    WHERE session_id = (SELECT id FROM sessions WHERE jwt_id = p_jwt_id)
      AND is_connected = TRUE;

    RETURN QUERY SELECT TRUE, NULL::VARCHAR(100);
END;
$$ LANGUAGE plpgsql;

-- ═══════════════════════════════════════════════════════════════════
-- DEVICE CONNECTIONS
-- ═══════════════════════════════════════════════════════════════════

-- Register a device connection (when socket connects).
CREATE OR REPLACE FUNCTION sp_connect_device(
    p_user_id       UUID,
    p_session_id    UUID,
    p_device_type   VARCHAR(20),
    p_platform      VARCHAR(20)
)
RETURNS TABLE(
    out_connection_id   UUID,
    out_error           VARCHAR(100)
) AS $$
DECLARE
    v_conn_id UUID;
BEGIN
    -- Disconnect any existing connection of same type for this user
    UPDATE device_connections
    SET is_connected = FALSE, disconnected_at = NOW()
    WHERE user_id = p_user_id
      AND device_type = p_device_type
      AND is_connected = TRUE;

    -- Create new connection
    INSERT INTO device_connections (user_id, session_id, device_type, platform)
    VALUES (p_user_id, p_session_id, p_device_type, p_platform)
    RETURNING id INTO v_conn_id;

    RETURN QUERY SELECT v_conn_id, NULL::VARCHAR(100);
END;
$$ LANGUAGE plpgsql;

-- Disconnect a device.
CREATE OR REPLACE FUNCTION sp_disconnect_device(
    p_connection_id UUID
)
RETURNS VOID AS $$
BEGIN
    UPDATE device_connections
    SET is_connected = FALSE, disconnected_at = NOW()
    WHERE id = p_connection_id AND is_connected = TRUE;
END;
$$ LANGUAGE plpgsql;

-- Get the paired desktop for a mobile user (for event routing).
CREATE OR REPLACE FUNCTION sp_get_paired_desktop(
    p_user_id UUID
)
RETURNS TABLE(
    out_connection_id   UUID,
    out_session_id      UUID,
    out_platform        VARCHAR(20)
) AS $$
BEGIN
    RETURN QUERY
    SELECT dc.id, dc.session_id, dc.platform
    FROM device_connections dc
    WHERE dc.user_id = p_user_id
      AND dc.device_type = 'desktop'
      AND dc.is_connected = TRUE
    LIMIT 1;
END;
$$ LANGUAGE plpgsql;

-- Get the paired mobile for a desktop user.
CREATE OR REPLACE FUNCTION sp_get_paired_mobile(
    p_user_id UUID
)
RETURNS TABLE(
    out_connection_id   UUID,
    out_session_id      UUID,
    out_platform        VARCHAR(20)
) AS $$
BEGIN
    RETURN QUERY
    SELECT dc.id, dc.session_id, dc.platform
    FROM device_connections dc
    WHERE dc.user_id = p_user_id
      AND dc.device_type = 'mobile'
      AND dc.is_connected = TRUE
    LIMIT 1;
END;
$$ LANGUAGE plpgsql;

-- ═══════════════════════════════════════════════════════════════════
-- ADMIN QUERIES
-- ═══════════════════════════════════════════════════════════════════

-- List all users with pagination.
CREATE OR REPLACE FUNCTION sp_list_users(
    p_limit     INT DEFAULT 50,
    p_offset    INT DEFAULT 0
)
RETURNS TABLE(
    out_id              UUID,
    out_email           VARCHAR(255),
    out_is_activated    BOOLEAN,
    out_created_at      TIMESTAMPTZ,
    out_total_count     BIGINT
) AS $$
BEGIN
    RETURN QUERY
    SELECT u.id, u.email, u.is_activated, u.created_at,
           COUNT(*) OVER() AS total_count
    FROM users u
    ORDER BY u.created_at DESC
    LIMIT p_limit OFFSET p_offset;
END;
$$ LANGUAGE plpgsql;

-- List active connections.
CREATE OR REPLACE FUNCTION sp_list_active_connections()
RETURNS TABLE(
    out_user_email      VARCHAR(255),
    out_device_type     VARCHAR(20),
    out_platform        VARCHAR(20),
    out_connected_at    TIMESTAMPTZ
) AS $$
BEGIN
    RETURN QUERY
    SELECT u.email, dc.device_type, dc.platform, dc.connected_at
    FROM device_connections dc
    JOIN users u ON u.id = dc.user_id
    WHERE dc.is_connected = TRUE
    ORDER BY dc.connected_at DESC;
END;
$$ LANGUAGE plpgsql;

-- Count active sessions and connections for dashboard.
CREATE OR REPLACE FUNCTION sp_admin_dashboard_stats()
RETURNS TABLE(
    out_total_users         BIGINT,
    out_activated_users     BIGINT,
    out_active_sessions     BIGINT,
    out_active_connections  BIGINT
) AS $$
BEGIN
    RETURN QUERY
    SELECT
        (SELECT COUNT(*) FROM users),
        (SELECT COUNT(*) FROM users WHERE is_activated = TRUE),
        (SELECT COUNT(*) FROM sessions WHERE is_active = TRUE AND expires_at > NOW()),
        (SELECT COUNT(*) FROM device_connections WHERE is_connected = TRUE);
END;
$$ LANGUAGE plpgsql;

COMMIT;
