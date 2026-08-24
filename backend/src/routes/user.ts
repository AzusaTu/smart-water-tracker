import { Router } from 'express';
import { getProfile, updateProfile } from '../controllers/userController';
import { authenticateUser } from '../middleware/auth';

const router = Router();

router.use(authenticateUser);

router.get('/me', getProfile);
router.put('/me', updateProfile);

export default router;
