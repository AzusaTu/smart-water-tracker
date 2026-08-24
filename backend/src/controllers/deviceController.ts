import { Response, NextFunction } from 'express';
import crypto from 'crypto';
import { z } from 'zod';
import { getDatabase } from '../database/db';
import { AuthenticatedRequest, Device, DeviceResponse } from '../types';

const bindDeviceSchema = z.object({
  deviceId: z.string().min(3, 'Device ID must be at least 3 characters'),
  name: z.string().optional(),
});

function isDeviceOnline(lastSeenAt: string | null): boolean {
  if (!lastSeenAt) return false;
  const lastSeenTime = new Date(lastSeenAt).getTime();
  const now = Date.now();
  // Online if communicated within the last 5 minutes (300,000 ms)
  return now - lastSeenTime <= 5 * 60 * 1000;
}

export function bindDevice(req: AuthenticatedRequest, res: Response, next: NextFunction): void {
  try {
    const userId = req.user?.id;
    if (!userId) {
      res.status(401).json({ error: 'Unauthorized' });
      return;
    }

    const { deviceId, name } = bindDeviceSchema.parse(req.body);
    const db = getDatabase();

    const existingDevice = db.prepare('SELECT * FROM devices WHERE id = ?').get(deviceId) as unknown as
      | Device
      | undefined;

    if (existingDevice) {
      if (existingDevice.user_id !== userId) {
        res.status(409).json({ error: 'Device is already bound to another user' });
        return;
      }
      // If already bound to this user, refresh token or return existing
      const response: DeviceResponse = {
        id: existingDevice.id,
        name: existingDevice.name,
        deviceToken: existingDevice.device_token,
        lastSeenAt: existingDevice.last_seen_at,
        isOnline: isDeviceOnline(existingDevice.last_seen_at),
        createdAt: existingDevice.created_at,
      };
      res.status(200).json({ device: response, message: 'Device already bound to your account' });
      return;
    }

    const deviceToken = `dvt_${crypto.randomBytes(24).toString('hex')}`;
    const now = new Date().toISOString();

    db.prepare(
      `INSERT INTO devices (id, user_id, device_token, name, created_at)
       VALUES (?, ?, ?, ?, ?)`
    ).run(deviceId, userId, deviceToken, name || null, now);

    const response: DeviceResponse = {
      id: deviceId,
      name: name || null,
      deviceToken,
      lastSeenAt: null,
      isOnline: false,
      createdAt: now,
    };

    res.status(201).json({
      device: response,
      message: 'Device bound successfully. Save the deviceToken to configure your ESP32 device.',
    });
  } catch (err) {
    next(err);
  }
}

export function listDevices(req: AuthenticatedRequest, res: Response, next: NextFunction): void {
  try {
    const userId = req.user?.id;
    if (!userId) {
      res.status(401).json({ error: 'Unauthorized' });
      return;
    }

    const db = getDatabase();
    const devices = db
      .prepare('SELECT * FROM devices WHERE user_id = ? ORDER BY created_at DESC')
      .all(userId) as unknown as Device[];

    const response: DeviceResponse[] = devices.map((d) => ({
      id: d.id,
      name: d.name,
      deviceToken: d.device_token,
      lastSeenAt: d.last_seen_at,
      isOnline: isDeviceOnline(d.last_seen_at),
      createdAt: d.created_at,
    }));

    res.status(200).json({ devices: response });
  } catch (err) {
    next(err);
  }
}

export function unbindDevice(req: AuthenticatedRequest, res: Response, next: NextFunction): void {
  try {
    const userId = req.user?.id;
    const deviceId = req.params.id;

    if (!userId) {
      res.status(401).json({ error: 'Unauthorized' });
      return;
    }

    const db = getDatabase();
    const device = db.prepare('SELECT id FROM devices WHERE id = ? AND user_id = ?').get(deviceId, userId);

    if (!device) {
      res.status(404).json({ error: 'Device not found or not owned by you' });
      return;
    }

    db.prepare('DELETE FROM devices WHERE id = ? AND user_id = ?').run(deviceId, userId);

    res.status(200).json({ success: true, message: 'Device unbound and token revoked successfully' });
  } catch (err) {
    next(err);
  }
}

export function getDeviceStatus(req: AuthenticatedRequest, res: Response, next: NextFunction): void {
  try {
    const userId = req.user?.id;
    const deviceId = req.params.id;

    if (!userId) {
      res.status(401).json({ error: 'Unauthorized' });
      return;
    }

    const db = getDatabase();
    const device = db
      .prepare('SELECT * FROM devices WHERE id = ? AND user_id = ?')
      .get(deviceId, userId) as unknown as Device | undefined;

    if (!device) {
      res.status(404).json({ error: 'Device not found or not owned by you' });
      return;
    }

    res.status(200).json({
      deviceId: device.id,
      name: device.name,
      lastSeenAt: device.last_seen_at,
      isOnline: isDeviceOnline(device.last_seen_at),
    });
  } catch (err) {
    next(err);
  }
}
