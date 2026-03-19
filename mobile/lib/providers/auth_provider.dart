import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../services/api_service.dart';

class AuthProvider extends ChangeNotifier {
  final ApiService _api = ApiService();

  String? _token;
  String? _userId;
  String? _email;
  bool _loading = false;
  String? _error;

  String? get token => _token;
  String? get userId => _userId;
  String? get email => _email;
  bool get isAuthenticated => _token != null;
  bool get isLoading => _loading;
  String? get error => _error;

  AuthProvider() {
    _loadSavedAuth();
  }

  Future<void> _loadSavedAuth() async {
    final prefs = await SharedPreferences.getInstance();
    _token = prefs.getString('rc_token');
    _userId = prefs.getString('rc_user_id');
    _email = prefs.getString('rc_email');
    if (_token != null) {
      notifyListeners();
    }
  }

  Future<void> _saveAuth() async {
    final prefs = await SharedPreferences.getInstance();
    if (_token != null) {
      await prefs.setString('rc_token', _token!);
      await prefs.setString('rc_user_id', _userId ?? '');
      await prefs.setString('rc_email', _email ?? '');
    } else {
      await prefs.remove('rc_token');
      await prefs.remove('rc_user_id');
      await prefs.remove('rc_email');
    }
  }

  Future<bool> login(String email, String password) async {
    _loading = true;
    _error = null;
    notifyListeners();

    try {
      final result = await _api.login(email, password);
      _token = result['token'] as String?;
      _userId = result['user_id'] as String?;
      _email = email;
      await _saveAuth();
      _loading = false;
      notifyListeners();
      return true;
    } on ApiException catch (e) {
      _error = e.message;
      _loading = false;
      notifyListeners();
      return false;
    } catch (e) {
      _error = 'Connection error. Check your server settings.';
      _loading = false;
      notifyListeners();
      return false;
    }
  }

  Future<bool> register(String email, String password) async {
    _loading = true;
    _error = null;
    notifyListeners();

    try {
      await _api.register(email, password);
      _loading = false;
      notifyListeners();
      return true;
    } on ApiException catch (e) {
      _error = e.message;
      _loading = false;
      notifyListeners();
      return false;
    } catch (e) {
      _error = 'Connection error. Check your server settings.';
      _loading = false;
      notifyListeners();
      return false;
    }
  }

  Future<void> logout() async {
    if (_token != null) {
      try {
        await _api.logout(_token!);
      } catch (_) {
        // Best-effort logout
      }
    }
    _token = null;
    _userId = null;
    _email = null;
    await _saveAuth();
    notifyListeners();
  }
}
