import request from 'supertest';
import { createApp } from '../src/app';
import { initDatabase, closeDatabase } from '../src/database/db';
import { Express } from 'express';

describe('Smart Water Tracker Backend API Test Suite', () => {
  let app: Express;
  let userToken: string;
  let userId: string;
  let deviceToken: string;
  const testEmail = `tester_${Date.now()}@example.com`;
  const testPassword = 'Password123!';
  const testDeviceId = `water_test_${Date.now().toString(16)}`;

  beforeAll(() => {
    // Initialize in-memory SQLite database for testing
    initDatabase(':memory:');
    app = createApp();
  });

  afterAll(() => {
    closeDatabase();
  });

  describe('1. Health Check', () => {
    it('GET /api/v1/health should return ok status', async () => {
      const res = await request(app).get('/api/v1/health');
      expect(res.status).toBe(200);
      expect(res.body.status).toBe('ok');
      expect(res.body.service).toBe('smart-water-tracker-backend');
      expect(typeof res.body.uptime).toBe('number');
    });
  });

  describe('2. User Authentication & Profile', () => {
    it('POST /api/v1/auth/register should register a new user and return JWT', async () => {
      const res = await request(app).post('/api/v1/auth/register').send({
        email: testEmail,
        password: testPassword,
        displayName: '水水測試員',
      });

      expect(res.status).toBe(201);
      expect(res.body).toHaveProperty('token');
      expect(res.body.user).toHaveProperty('id');
      expect(res.body.user.email).toBe(testEmail);
      expect(res.body.user.displayName).toBe('水水測試員');
      expect(res.body.user.dailyGoalMl).toBe(2000);

      userToken = res.body.token;
      userId = res.body.user.id;
    });

    it('POST /api/v1/auth/register should reject duplicate email', async () => {
      const res = await request(app).post('/api/v1/auth/register').send({
        email: testEmail,
        password: testPassword,
      });

      expect(res.status).toBe(409);
      expect(res.body.error).toContain('already registered');
    });

    it('POST /api/v1/auth/login should authenticate with correct credentials', async () => {
      const res = await request(app).post('/api/v1/auth/login').send({
        email: testEmail,
        password: testPassword,
      });

      expect(res.status).toBe(200);
      expect(res.body).toHaveProperty('token');
      expect(res.body.user.id).toBe(userId);
    });

    it('POST /api/v1/auth/login should reject wrong password', async () => {
      const res = await request(app).post('/api/v1/auth/login').send({
        email: testEmail,
        password: 'wrong_password',
      });

      expect(res.status).toBe(401);
    });

    it('GET /api/v1/user/me should require authentication', async () => {
      const res = await request(app).get('/api/v1/user/me');
      expect(res.status).toBe(401);
    });

    it('GET /api/v1/user/me should return current user profile', async () => {
      const res = await request(app)
        .get('/api/v1/user/me')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(res.body.user.id).toBe(userId);
      expect(res.body.user.email).toBe(testEmail);
    });

    it('PUT /api/v1/user/me should update user daily goal', async () => {
      const res = await request(app)
        .put('/api/v1/user/me')
        .set('Authorization', `Bearer ${userToken}`)
        .send({
          dailyGoalMl: 2500,
          displayName: '水水大師',
        });

      expect(res.status).toBe(200);
      expect(res.body.user.dailyGoalMl).toBe(2500);
      expect(res.body.user.displayName).toBe('水水大師');
    });
  });

  describe('3. Device Management & Device Token', () => {
    it('POST /api/v1/devices should bind a new device and generate a device token', async () => {
      const res = await request(app)
        .post('/api/v1/devices')
        .set('Authorization', `Bearer ${userToken}`)
        .send({
          deviceId: testDeviceId,
          name: '辦公桌智慧水杯',
        });

      expect(res.status).toBe(201);
      expect(res.body.device.id).toBe(testDeviceId);
      expect(res.body.device.name).toBe('辦公桌智慧水杯');
      expect(res.body.device.deviceToken).toMatch(/^dvt_/);

      deviceToken = res.body.device.deviceToken;
    });

    it('GET /api/v1/devices should list all bound devices', async () => {
      const res = await request(app)
        .get('/api/v1/devices')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(Array.isArray(res.body.devices)).toBe(true);
      expect(res.body.devices.length).toBe(1);
      expect(res.body.devices[0].id).toBe(testDeviceId);
    });

    it('GET /api/v1/devices/:id/status should return device status', async () => {
      const res = await request(app)
        .get(`/api/v1/devices/${testDeviceId}/status`)
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(res.body.deviceId).toBe(testDeviceId);
      expect(typeof res.body.isOnline).toBe('boolean');
    });
  });

  describe('4. Water Records Sync & Deduplication', () => {
    const eventId1 = `${testDeviceId}-1787918400-0`;

    it('POST /api/v1/water/records should accept ESP32 drink event with Device Token', async () => {
      const res = await request(app)
        .post('/api/v1/water/records')
        .set('Authorization', `Bearer ${deviceToken}`)
        .send({
          eventId: eventId1,
          type: 'drink',
          amountMl: 300,
          remainingMl: 250,
          todayTotalMl: 300,
          timeSynced: true,
          occurredAt: Math.floor(Date.now() / 1000),
        });

      expect(res.status).toBe(201);
      expect(res.body.duplicated).toBe(false);
      expect(res.body.record.eventId).toBe(eventId1);
      expect(res.body.record.amountMl).toBe(300);
      expect(res.body.record.eventType).toBe('drink');
    });

    it('POST /api/v1/water/records should reject excessive amountMl (>5000ml)', async () => {
      const res = await request(app)
        .post('/api/v1/water/records')
        .set('Authorization', `Bearer ${deviceToken}`)
        .send({
          type: 'drink',
          amountMl: 999999,
        });

      expect(res.status).toBe(400);
    });

    it('POST /api/v1/water/records should deduplicate on identical eventId (idempotent)', async () => {
      const res = await request(app)
        .post('/api/v1/water/records')
        .set('Authorization', `Bearer ${deviceToken}`)
        .send({
          eventId: eventId1,
          type: 'drink',
          amountMl: 300,
          remainingMl: 250,
          todayTotalMl: 300,
          timeSynced: true,
          occurredAt: Math.floor(Date.now() / 1000),
        });

      expect(res.status).toBe(200);
      expect(res.body.duplicated).toBe(true);
      expect(res.body.record.eventId).toBe(eventId1);
      expect(res.body.message).toContain('idempotent');
    });

    it('POST /api/v1/water/records should accept Refill event without counting toward drink goal', async () => {
      const refillEventId = `${testDeviceId}-1787918400-1`;
      const res = await request(app)
        .post('/api/v1/water/records')
        .set('Authorization', `Bearer ${deviceToken}`)
        .send({
          eventId: refillEventId,
          type: 'refill',
          amountMl: 400,
          remainingMl: 650,
          todayTotalMl: 300,
          timeSynced: true,
          occurredAt: Math.floor(Date.now() / 1000),
        });

      expect(res.status).toBe(201);
      expect(res.body.record.eventType).toBe('refill');
      expect(res.body.record.amountMl).toBe(400);
    });

    it('POST /api/v1/water/records should handle timeSynced: false with server timestamp fallback', async () => {
      const unsyncedEventId = `${testDeviceId}-unsynced-2`;
      const res = await request(app)
        .post('/api/v1/water/records')
        .set('Authorization', `Bearer ${deviceToken}`)
        .send({
          eventId: unsyncedEventId,
          type: 'drink',
          amountMl: 200,
          timeSynced: false,
          occurredAt: 0,
        });

      expect(res.status).toBe(201);
      expect(res.body.record.occurredAt).toBeDefined();
      expect(new Date(res.body.record.occurredAt).getFullYear()).toBeGreaterThanOrEqual(2025);
    });

    it('GET /api/v1/water/records should list records for user with pagination and date filter', async () => {
      const res = await request(app)
        .get('/api/v1/water/records?limit=10&page=1')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(res.body.records.length).toBe(3); // 2 drinks + 1 refill
      expect(res.body.pagination.total).toBe(3);
    });
  });

  describe('5. Water Statistics (Daily, Weekly, Monthly)', () => {
    it('GET /api/v1/water/stats/daily should compute daily progress and counts', async () => {
      const res = await request(app)
        .get('/api/v1/water/stats/daily')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      // Total drink = 300ml + 200ml = 500ml. Refill (400ml) is excluded from totalMl.
      expect(res.body.totalMl).toBe(500);
      expect(res.body.goalMl).toBe(2500);
      expect(res.body.progress).toBe(0.2); // 500 / 2500 = 0.2
      expect(res.body.goalMet).toBe(false);
      expect(res.body.drinkCount).toBe(2);
      expect(res.body.refillCount).toBe(1);
    });

    it('GET /api/v1/water/stats/daily should validate date format and reject invalid date query', async () => {
      const res = await request(app)
        .get('/api/v1/water/stats/daily?date=invalid-date')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(400);
    });

    it('GET /api/v1/water/stats/daily should return zero for empty past date', async () => {
      const res = await request(app)
        .get('/api/v1/water/stats/daily?date=2020-01-01')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(res.body.date).toBe('2020-01-01');
      expect(res.body.totalMl).toBe(0);
      expect(res.body.drinkCount).toBe(0);
    });

    it('GET /api/v1/water/stats/weekly should return 7 days breakdown and averages using indexed range', async () => {
      const res = await request(app)
        .get('/api/v1/water/stats/weekly')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(res.body.days.length).toBe(7);
      expect(res.body.totalWeekMl).toBe(500);
      expect(typeof res.body.averageMl).toBe('number');
      expect(typeof res.body.goalMetDays).toBe('number');
    });

    it('GET /api/v1/water/stats/monthly should return 30 days breakdown and streak metrics', async () => {
      const res = await request(app)
        .get('/api/v1/water/stats/monthly')
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(res.body.days.length).toBe(30);
      expect(typeof res.body.currentStreak).toBe('number');
      expect(typeof res.body.bestStreak).toBe('number');
    });
  });

  describe('6. Device Unbinding & Token Revocation', () => {
    it('DELETE /api/v1/devices/:id should unbind device', async () => {
      const res = await request(app)
        .delete(`/api/v1/devices/${testDeviceId}`)
        .set('Authorization', `Bearer ${userToken}`);

      expect(res.status).toBe(200);
      expect(res.body.success).toBe(true);
    });

    it('POST /api/v1/water/records should reject revoked device token', async () => {
      const res = await request(app)
        .post('/api/v1/water/records')
        .set('Authorization', `Bearer ${deviceToken}`)
        .send({
          amountMl: 200,
        });

      expect(res.status).toBe(401);
    });
  });
});
