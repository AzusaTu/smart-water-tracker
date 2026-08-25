import { Request, Response, NextFunction } from 'express';
import bcrypt from 'bcryptjs';
import jwt from 'jsonwebtoken';
import { v4 as uuidv4 } from 'uuid';
import { z } from 'zod';
import { config } from '../config/env';
import { getDatabase } from '../database/db';
import { User, UserResponse } from '../types';

const registerSchema = z.object({
  email: z.string().email('Invalid email format'),
  password: z.string().min(6, 'Password must be at least 6 characters long'),
  displayName: z.string().optional(),
});

const loginSchema = z.object({
  email: z.string().email('Invalid email format'),
  password: z.string().min(1, 'Password is required'),
});

export async function register(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const { email, password, displayName } = registerSchema.parse(req.body);
    const db = getDatabase();

    const existingUser = db.prepare('SELECT id FROM users WHERE email = ?').get(email);
    if (existingUser) {
      res.status(409).json({ error: 'Email is already registered' });
      return;
    }

    const userId = uuidv4();
    const passwordHash = await bcrypt.hash(password, 10);
    const now = new Date().toISOString();

    db.prepare(
      `INSERT INTO users (id, email, password_hash, display_name, daily_goal_ml, created_at, updated_at)
       VALUES (?, ?, ?, ?, ?, ?, ?)`
    ).run(userId, email, passwordHash, displayName || null, 2000, now, now);

    const token = jwt.sign({ userId, email }, config.jwtSecret, { expiresIn: '30d' });

    const userResponse: UserResponse = {
      id: userId,
      email,
      displayName: displayName || null,
      dailyGoalMl: 2000,
      createdAt: now,
    };

    res.status(201).json({
      token,
      user: userResponse,
    });
  } catch (err) {
    next(err);
  }
}

export async function login(req: Request, res: Response, next: NextFunction): Promise<void> {
  try {
    const { email, password } = loginSchema.parse(req.body);
    const db = getDatabase();

    const user = db.prepare('SELECT * FROM users WHERE email = ?').get(email) as unknown as User | undefined;
    if (!user) {
      res.status(401).json({ error: 'Invalid email or password' });
      return;
    }

    const isMatch = await bcrypt.compare(password, user.password_hash);
    if (!isMatch) {
      res.status(401).json({ error: 'Invalid email or password' });
      return;
    }

    const token = jwt.sign({ userId: user.id, email: user.email }, config.jwtSecret, {
      expiresIn: '30d',
    });

    const userResponse: UserResponse = {
      id: user.id,
      email: user.email,
      displayName: user.display_name,
      dailyGoalMl: user.daily_goal_ml,
      createdAt: user.created_at,
    };

    res.status(200).json({
      token,
      user: userResponse,
    });
  } catch (err) {
    next(err);
  }
}
