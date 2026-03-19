import 'dart:convert';
import 'dart:io';
import 'package:flutter_dotenv/flutter_dotenv.dart';

class ApiService {
  late final String _baseUrl;
  late final HttpClient _httpClient;

  ApiService() {
    final host = dotenv.env['RC_SERVER_HOST'] ?? 'local.remotecontrol.rs';
    final port = dotenv.env['RC_SERVER_API_PORT'] ?? '443';
    final scheme = dotenv.env['RC_SERVER_SCHEME'] ?? 'https';

    // Skip port in URL if it's the default for the scheme
    if ((scheme == 'https' && port == '443') ||
        (scheme == 'http' && port == '80')) {
      _baseUrl = '$scheme://$host';
    } else {
      _baseUrl = '$scheme://$host:$port';
    }

    // Allow self-signed certificates
    _httpClient = HttpClient()
      ..badCertificateCallback = (cert, host, port) => true;
  }

  Future<Map<String, dynamic>> _post(String path,
      {Map<String, dynamic>? body, String? token}) async {
    final uri = Uri.parse('$_baseUrl$path');
    final request = await _httpClient.postUrl(uri);

    request.headers.set('Content-Type', 'application/json');
    if (token != null) {
      request.headers.set('Authorization', 'Bearer $token');
    }

    if (body != null) {
      request.write(jsonEncode(body));
    }

    final response = await request.close();
    final responseBody = await response.transform(utf8.decoder).join();

    return {
      'statusCode': response.statusCode,
      'body': responseBody.isNotEmpty
          ? jsonDecode(responseBody) as Map<String, dynamic>
          : <String, dynamic>{},
    };
  }

  Future<Map<String, dynamic>> register(String email, String password) async {
    final result = await _post('/api/register', body: {
      'email': email,
      'password': password,
    });

    final statusCode = result['statusCode'] as int;
    final body = result['body'] as Map<String, dynamic>;

    if (statusCode == 201 || statusCode == 200) {
      return body;
    }

    throw ApiException(
        body['error'] ?? 'registration_failed', statusCode);
  }

  Future<Map<String, dynamic>> login(String email, String password) async {
    final result = await _post('/api/login', body: {
      'email': email,
      'password': password,
      'device_type': 'mobile',
    });

    final statusCode = result['statusCode'] as int;
    final body = result['body'] as Map<String, dynamic>;

    if (statusCode == 200) {
      return body;
    }

    throw ApiException(body['error'] ?? 'login_failed', statusCode);
  }

  Future<void> logout(String token) async {
    await _post('/api/logout', token: token);
  }
}

class ApiException implements Exception {
  final String code;
  final int statusCode;

  ApiException(this.code, this.statusCode);

  String get message {
    switch (code) {
      case 'EMAIL_EXISTS':
        return 'An account with this email already exists.';
      case 'invalid_credentials':
        return 'Invalid email or password.';
      case 'account_not_activated':
        return 'Please check your email and activate your account.';
      case 'invalid_email':
        return 'Please enter a valid email address.';
      case 'password_too_short':
        return 'Password must be at least 8 characters.';
      default:
        return 'An error occurred: $code';
    }
  }

  @override
  String toString() => message;
}
