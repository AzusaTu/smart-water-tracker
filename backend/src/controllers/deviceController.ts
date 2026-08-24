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

function maskToken(token: string): string {
  if (!token || token.length < 8) return '****';
  return `${token.substring(0, 4)}****${token.substring(token.length - 4)}`;
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
      // If already bound to this user, return response with masked token
      const response: DeviceResponse = {
        id: existingDevice.id,
        name: existingDevice.name,
        deviceToken: maskToken(existingDevice.device_token),
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
      deviceToken, // Full token returned ONLY on initial binding
      lastSeenAt: null,
      isOnline: false,
      createdAt: now,
    };

    res.status(201).json({
      device: response,
      message: 'Device bound successfully. Save the deviceToken now; it will not be shown again in full.',
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

    // Return masked tokens in list view to prevent credential leakage
    const response: DeviceResponse[] = devices.map((d) => ({
      id: d.id,
      name: d.name,
      deviceToken: maskToken(d.device_token),
      lastSeenAt: d.last_seen_at,
      isOnline: isDeviceOnline(d.last_seen_at),
      createdAt: d.created_at,
    }));

    res.status(200).json({ devices: response });
  } catch (err) {
    next(err);
  }
}

export function rotateDeviceToken(req: AuthenticatedRequest, res: Response, next: NextFunction): void {
  try {
    const userId = req.user?.id;
    const deviceId = req.params.id;

    if (!userId) {
      res.status(401).json({ error: 'Unauthorized' });
      return;
    }

    const db = getDatabase();
    const device = db
      .prepare('SELECT id FROM devices WHERE id = ? AND user_id = ?')
      .get(deviceId, userId) as unknown as Pick<Device, 'id'> | undefined;

    if (!device) {
      res.status(404).json({ error: 'Device not found or not owned by you' });
      return;
    }

    const newDeviceToken = `dvt_${crypto.randomBytes(24).toString('hex')}`;
    db.prepare('UPDATE devices SET device_token = ? WHERE id = ? AND user_id = ?').run(
      newDeviceToken,
      deviceId,
      userId
    );

    res.status(200).json({
      deviceId,
      deviceToken: newDeviceToken,
      message: 'Device token rotated successfully. Update your ESP32 configuration with this new token.',
    });
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
