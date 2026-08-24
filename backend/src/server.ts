import { createApp } from './app';
import { config } from './config/env';
import { initDatabase, closeDatabase } from './database/db';

function startServer(): void {
  // Initialize Database
  initDatabase();
  console.log(`[Database] SQLite initialized at: ${config.databasePath}`);

  const app = createApp();

  const server = app.listen(config.port, () => {
    console.log(`================================================`);
    console.log(`🥤 Smart Water Tracker Backend is running!`);
    console.log(`🚀 URL: http://localhost:${config.port}`);
    console.log(`🩺 Health: http://localhost:${config.port}/api/v1/health`);
    console.log(`================================================`);
  });

  const shutdown = (signal: string) => {
    console.log(`\n[Server] Received ${signal}. Shutting down gracefully...`);
    server.close(() => {
      closeDatabase();
      console.log('[Server] Database connection closed. Server exited cleanly.');
      process.exit(0);
    });

    // Force kill if graceful close exceeds 5 seconds
    setTimeout(() => {
      console.error('[Server] Forced shutdown after timeout.');
      process.exit(1);
    }, 5000).unref();
  };

  process.on('SIGTERM', () => shutdown('SIGTERM'));
  process.on('SIGINT', () => shutdown('SIGINT'));
}

startServer();
