import express from 'express';
import cors from 'cors';
import fs from 'fs';
import path from 'path';
import authRoutes from './routes/auth';
import userRoutes from './routes/user';
import deviceRoutes from './routes/devices';
import waterRoutes from './routes/water';
import { errorHandler } from './middleware/errorHandler';

function getPublicDir(): string {
  const candidates = [
    path.join(__dirname, 'public'),
    path.join(__dirname, '../src/public'),
    path.join(process.cwd(), 'src/public'),
    path.join(process.cwd(), 'backend/src/public'),
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return path.join(__dirname, 'public');
}

export function createApp(): express.Express {
  const app = express();
  const publicDir = getPublicDir();

  // Middleware
  app.use(cors());
  app.use(express.json({ limit: '1mb' }));

  // Static Dashboard Assets
  app.use(express.static(publicDir));

  // Health Check Endpoint
  app.get('/api/v1/health', (_req, res) => {
    res.status(200).json({
      status: 'ok',
      service: 'smart-water-tracker-backend',
      uptime: process.uptime(),
      timestamp: new Date().toISOString(),
    });
  });

  // API Routes
  app.use('/api/v1/auth', authRoutes);
  app.use('/api/v1/user', userRoutes);
  app.use('/api/v1/devices', deviceRoutes);
  app.use('/api/v1/water', waterRoutes);

  // Fallback for Single Page App Dashboard
  app.get('/', (_req, res) => {
    const indexPath = path.join(publicDir, 'index.html');
    if (fs.existsSync(indexPath)) {
      res.sendFile(indexPath);
    } else {
      res.status(200).send('Smart Water Tracker Backend is running.');
    }
  });

  // Global Error Handler
  app.use(errorHandler);

  return app;
}
